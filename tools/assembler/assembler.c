/*
 * assembler.c
 * ------
 * Reads a 8bit cpu assembler file and writes a compiled executable
 * for our architecture.
 *
 * Compile project via: clang assembler.c -o assembler
 *
 * Usage: ./assembler program.asm program.out
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION                 "0.2"

#define PROGRAM_MEMORY_SIZE     256

#define MAX_JUMP_COUNT          64
#define MAX_LABEL_COUNT         32
#define MAX_LABEL_LENGTH        32
#define MAX_LINE_LENGTH         128
#define OPCODE_COUNT            33


/*
 * Struct: label
 * ------
 * Placeholder for occurring label and jump declaration, mapping
 * the address in the program memory to the label name.
 */

struct label
{
    int memory_addr;
    char label[MAX_LABEL_LENGTH];
};


/*
 * Struct: opcode
 * ------
 * Holds data about opcode mnemonic and its microcode addresses
 * for the regarding addressing modes.
 */

typedef struct
{
    char mnemonic[3];   // Mnemonic of opcode

    int a;              // Implicit / single-byte instruction (no operand)

    int a_abs;          // Absolute             $nn
    int a_imm;          // Immediate            #$nn
    int a_idx;          // Indexed              $nn,a
    int a_idx_ind;      // Indexed Indirect     ($nn,a)
    int a_ind;          // Indirect             ($nn)
    int a_ind_idx;      // Indirect Indexed     ($nn),a

    int a_label;        // Labelled (internal)
} opcode;


/*
 * Table: OPCODES[]
 * ------
 * Lists all given opcodes of our architecture defining
 * their microcode addresses in the control unit (CU).
 */

static const opcode OPCODES[] =
{
    { "adc", 0x00, 0x5a, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "and", 0x00, 0x70, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "asl", 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "bcc", 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x00, 0x98 },
    { "bcs", 0x00, 0x00, 0x9a, 0x00, 0x00, 0x00, 0x00, 0x9a },
    { "beq", 0x00, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x9e },
    { "bmi", 0x00, 0x00, 0x96, 0x00, 0x00, 0x00, 0x00, 0x96 },
    { "bne", 0x00, 0x00, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x9c },
    { "bpl", 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x94 },
    { "cib", 0xd7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "clc", 0xa2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "cmp", 0x00, 0xa6, 0xa4, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "dec", 0x6a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "eor", 0x00, 0x80, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "inc", 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "jmp", 0x00, 0xba, 0xb8, 0x00, 0x00, 0x00, 0x00, 0xb8 },
    { "jsr", 0x00, 0xc8, 0xbe, 0x00, 0x00, 0x00, 0x00, 0xbe },
    { "lda", 0x00, 0x08, 0x06, 0x00, 0x00, 0x0c, 0x00, 0x00 },
    { "ldb", 0x00, 0x14, 0x12, 0xd9, 0x25, 0x18, 0x1e, 0x00 },
    { "lsl", 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "lsr", 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "ora", 0x00, 0x78, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "pha", 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "pop", 0xb2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "rol", 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "ror", 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "rts", 0xd1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "sbc", 0x00, 0x62, 0x5f, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "sec", 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "sta", 0x00, 0x2c, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00 },
    { "stb", 0x00, 0x3b, 0x00, 0x36, 0x4c, 0x3f, 0x45, 0x00 },
    { "tab", 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { "tba", 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};


/*
 * Function: print_syntax_error
 * ------
 * Prints a formatted error message for the user.
 *
 * message: Error message string
 * token: Related artefact
 * line_index: Line number in code (-1 when not given)
 */

void print_syntax_error (char *message, char *token, int line_index)
{
    if (line_index > -1)
    {
        printf(
            "Found syntax error in '%s' @ line %i (%s)!\n",
            token,
            line_index,
            message
        );
    }
    else
    {
        printf(
            "Found syntax error in '%s' (%s)!\n",
            token,
            message
        );
    }
}


/*
 * Function: remove_comments
 * ------
 * Removes all assembler-style comments (;) from a string
 *
 * str: The processed string
 */

void remove_comments (char *str)
{
    int i;

    for (i = 0; str[i] != 00; i++)
    {
        if (str[i] == ';')
        {
            str[i] = 00;
            break;
        }
    }
}


/*
 * Function: is_hex_str
 * ------
 * Checks if string holds only symbols describing a hexadecimal number
 *
 * str: The processed string
 *
 * returns: 1 if string represents a hexadecimal number, otherwise 0
 */

bool is_hex_str (char *str)
{
    int i;
    bool result = 1;

    for (i = 0; str[i] != 00; i++)
    {
        if (!isxdigit(str[i]))
        {
            result = 0;
            break;
        }
    }

    return result;
}


/*
 * Function: is_abs, is_idx, is_imm, is_idx_ind, is_ind, is_ind_idx
 * ------
 * Checks if address string uses an specific addressing mode
 *
 * token: The string to check against
 *
 * returns: 1 if string uses the adressing mode, otherwise 0
 */

bool is_abs (char *token)
{
    if (
        token[0] == '$' &&
        token[strlen(token) - 2] != ','
    )
    {
        return 1;
    }

    return 0;
}


bool is_idx (char *token)
{
    if (
        token[0] == '$' &&
        token[strlen(token) - 2] == ',' &&
        token[strlen(token) - 1] == 'a'
    )
    {
        return 1;
    }

    return 0;
}


bool is_imm (char *token)
{
    if (token[0] == '#')
    {
        return 1;
    }

    return 0;
}


bool is_idx_ind (char *token)
{
    if (
        token[0] == '(' &&
        token[1] == '$' &&
        token[strlen(token) - 3] == ',' &&
        token[strlen(token) - 2] == 'a' &&
        token[strlen(token) - 1] == ')'
    )
    {
        return 1;
    }

    return 0;
}


bool is_ind (char *token)
{
    if (
        token[0] == '(' &&
        token[1] == '$' &&
        token[strlen(token) - 3] != ',' &&
        token[strlen(token) - 2] != 'a' &&
        token[strlen(token) - 1] == ')'
    )
    {
        return 1;
    }

    return 0;
}


bool is_ind_idx (char *token)
{
    if (
        token[0] == '(' &&
        token[1] == '$' &&
        token[strlen(token) - 3] != ')' &&
        token[strlen(token) - 2] != ',' &&
        token[strlen(token) - 1] == 'a'
    )
    {
        return 1;
    }

    return 0;
}


/*
 * Function: is_valid_address
 * ------
 * Checks if token is a correct address mode
 *
 * opcode_index: Used opcode
 * str: The processed string
 *
 * returns: 1 if string is a valid address, otherwise 0
 */

bool is_valid_address (int opcode_index, char *str)
{
    if (
        (OPCODES[opcode_index].a_abs && is_abs(str)) ||
        (OPCODES[opcode_index].a_ind && is_ind(str)) ||
        (OPCODES[opcode_index].a_imm && is_imm(str)) ||
        (OPCODES[opcode_index].a_ind_idx && is_ind_idx(str)) ||
        (OPCODES[opcode_index].a_idx_ind && is_idx_ind(str)) ||
        (OPCODES[opcode_index].a_idx && is_idx(str))
    )
    {
        return 1;
    }

    return 0;
}

/*
 * Function: find_opcode_index
 * ------
 * Returns the index of the opcode lookup table
 *
 * mnemonic: The mnemonic of the opcode
 *
 * returns: index of opcode in lookup table
 */

int find_opcode_index (char *mnemonic)
{
    int i;
    int found_index = -1;

    for (i = 0; i < OPCODE_COUNT; i++)
    {
        if (strncmp(OPCODES[i].mnemonic, mnemonic, 3) == 0)
        {
            found_index = i;
            break;
        }
    }

    return found_index;
}


/*
 * Function: handle_opcode
 * ------
 * Adds the single opcode (1byte) microcode instruction to the program.
 *
 * program: The program
 * index: Index of the current line in program
 * op: Index of the opcode in lookup-table
 */

void handle_opcode (int *program, int index, int op)
{
    program[index] = OPCODES[op].a;
    program[index + 1] = 0x00;
}


/*
 * Function: handle_opcode_and_operand
 * ------
 * Adds the opcode + operand (2byte) microcode instruction to the program.
 *
 * program: The program
 * index: Index of the current line in program
 * op: Index of the opcode in lookup-table
 * addr: Operand / address
 */

void handle_opcode_and_operand (int *program, int index, int op, char *addr)
{
    char* stripped_addr;

    if (is_imm(addr))
    {
        // Immediate adressing mode
        program[index] = OPCODES[op].a_imm;
        stripped_addr = addr + 2;
    }
    else if (is_idx(addr))
    {
        // Indexed adressing mode
        program[index] = OPCODES[op].a_idx;
        stripped_addr = addr + 1;
        stripped_addr[strlen(stripped_addr) - 2] = 0;
    }
    else if (is_abs(addr))
    {
        // Absolute adressing mode
        program[index] = OPCODES[op].a_abs;
        stripped_addr = addr + 1;
    }
    else if (is_ind(addr))
    {
        // Indirect adressing mode
        program[index] = OPCODES[op].a_ind;
        stripped_addr = addr + 2;
        stripped_addr[strlen(stripped_addr) - 1] = 0;
    }
    else if (is_ind_idx(addr))
    {
        // Indirect indexed adressing mode
        program[index] = OPCODES[op].a_ind_idx;
        stripped_addr = addr + 2;
        stripped_addr[strlen(stripped_addr) - 3] = 0;
    }
    else if (is_idx_ind(addr))
    {
        // Indexed indirect adressing mode
        program[index] = OPCODES[op].a_idx_ind;
        stripped_addr = addr + 2;
        stripped_addr[strlen(stripped_addr) - 3] = 0;
    }

    if (!is_hex_str(stripped_addr))
    {
        print_syntax_error("Invalid address format", addr, 0);
        exit(EXIT_FAILURE);
    }

    int hex_int = (int) strtol(stripped_addr, NULL, 16);

    if (hex_int < 0 || hex_int > PROGRAM_MEMORY_SIZE - 1)
    {
        print_syntax_error("Invalid address range", addr, 0);
        exit(EXIT_FAILURE);
    }

    program[index + 1] = hex_int;
}


/*
 * Function: print_and_save_program
 * ------
 * Writes the compiled program to a file and
 * prints the result to the screen.
 *
 * file: Write file
 * program: The compiled program
 * size: Length of the program (in bytes)
 */

void print_and_save_program (FILE *file, int *program, int size)
{
    int i;

    // Header for Logisim
    fprintf(file, "v2.0 raw\n");

    for (i = 0; i < size; i++)
    {
        fprintf(file, "%02x", program[i]);
        printf("%02x", program[i]);

        // Pretty print for screen
        if (i % 16 == 15)
        {
            fprintf(file, "\n");
            printf("\n");
        }
        else
        {
            fprintf(file, " ");
            printf(" ");
        }
    }
}


/*
 * Function: main
 * ------
 * Reads a 8bit cpu assembler file and writes a compiled executable
 * for our architecture.
 *
 * Usage: ./assembler program.asm program.out
 */

int main (int argc, char **argv)
{
    printf("===============================================\n");
    printf("            8bit cpu assembler v%s\n", VERSION);
    printf("===============================================\n");

    // Get input file
    char *file_path = argv[1];
    FILE *file;

    file = fopen(file_path, "r");

    if (file == 0)
    {
        printf("Error: Please specify a valid file path.\n");
        exit(EXIT_FAILURE);
    }

    // Check output file
    char *write_file_path = argv[2];

    if (!write_file_path)
    {
        printf("Error: Please specify a valid out file path.\n");
        exit(EXIT_FAILURE);
    }

    // Variables for stream reading
    char line[MAX_LINE_LENGTH];
    char *token = NULL;

    // Variables for parsing and error output
    int opcode_index = -1;
    int current_line_index = 1;

    // The compiled program variables
    int program[PROGRAM_MEMORY_SIZE];
    int program_adr = 0;

    // Lookup table for labels
    struct label label_table[MAX_LABEL_COUNT];
    int label_table_count = 0;

    // Lookup table for labelled jump instructions
    struct label jump_table[MAX_JUMP_COUNT];
    int jump_table_count = 0;

    // Read file line by line
    while (fgets(line, sizeof line, file) != NULL)
    {
        remove_comments(line);
        token = strtok(line, "\n\t\r ");

        while (token)
        {
            // Expects operand (address or label) of two-byte instruction
            if (opcode_index > -1)
            {
                if (is_valid_address(opcode_index, token))
                {
                    // Found an address
                    handle_opcode_and_operand(
                        program,
                        program_adr,
                        opcode_index,
                        token
                    );
                }
                else if (OPCODES[opcode_index].a_label)
                {
                    // Found a label
                    if (jump_table_count > MAX_JUMP_COUNT)
                    {
                        print_syntax_error(
                            "Exceeded jump count",
                            token,
                            current_line_index
                        );

                        exit(EXIT_FAILURE);
                    }
                    else if (strlen(token) > MAX_LABEL_LENGTH)
                    {
                        print_syntax_error(
                            "Label name is too long",
                            token,
                            current_line_index
                        );

                        exit(EXIT_FAILURE);
                    }

                    // Write instruction to program with placeholder
                    program[program_adr] = OPCODES[opcode_index].a_label;
                    program[program_adr + 1] = 0x00;

                    // Store position for final processing
                    jump_table[jump_table_count].memory_addr = program_adr + 1;
                    strcpy(jump_table[jump_table_count].label, token);
                    jump_table_count += 1;
                }
                else
                {
                    print_syntax_error(
                        "Invalid or missing operand",
                        token,
                        current_line_index
                    );

                    exit(EXIT_FAILURE);
                }

                opcode_index = -1;
                program_adr += 2;
            }
            else
            {
                // Handle label or opcode
                int op = find_opcode_index(token);

                // Is token an opcode?
                if (op > -1)
                {
                    // Will an operand be expected in next token?
                    if (!OPCODES[op].a)
                    {
                        // Prepare two-byte instruction
                        opcode_index = op;
                    }
                    else
                    {
                        // Handle single byte instruction
                        handle_opcode(
                            program,
                            program_adr,
                            op
                        );

                        program_adr += 2;
                    }
                }
                else if (token[(strlen(token) - 1)] == ':')
                {
                    // Token is a label, strip the last character (:)
                    token[strlen(token) - 1] = 0;

                    if (label_table_count > MAX_LABEL_COUNT)
                    {
                        print_syntax_error(
                            "Exceeded label count",
                            token,
                            current_line_index
                        );

                        exit(EXIT_FAILURE);
                    }
                    else if (strlen(token) > MAX_LABEL_LENGTH)
                    {
                        print_syntax_error(
                            "Label name is too long",
                            token,
                            current_line_index
                        );

                        exit(EXIT_FAILURE);
                    }

                    // Store it in table for later processing
                    label_table[label_table_count].memory_addr = program_adr;
                    strcpy(label_table[label_table_count].label, token);

                    label_table_count += 1;
                }
                else
                {
                    print_syntax_error(
                        "Unkown opcode",
                        token,
                        current_line_index
                    );

                    exit(EXIT_FAILURE);
                }
            }

            if (program_adr > PROGRAM_MEMORY_SIZE)
            {
                print_syntax_error(
                    "Program exceeds memory size",
                    token,
                    current_line_index
                );

                exit(EXIT_FAILURE);
            }

            token = strtok(NULL, "\n\t\r ");
        }

        current_line_index += 1;
    }

    fclose(file);

    // Finally replace labels with memory addresses
    int i, n;
    bool found_label;

    for (i = 0; i < jump_table_count; i++)
    {
        found_label = 0;

        for (n = 0; n < label_table_count; n++)
        {
            if (strcmp(jump_table[i].label, label_table[n].label) == 0)
            {
                // Replace label with actual memory location in program
                program[jump_table[i].memory_addr] = label_table[n].memory_addr;
                found_label = 1;
                break;
            }
        }

        if (!found_label)
        {
            print_syntax_error("Could not find label", jump_table[i].label, -1);
            exit(EXIT_FAILURE);
        }
    }

    // Save binary to file and print result
    printf(
        "Successfully compiled program (%i bytes):\n\n",
        program_adr
    );

    FILE * write_file = fopen(write_file_path, "w+");
    print_and_save_program(write_file, program, program_adr);
    fclose(write_file);

    printf("\n\nWrite output to '%s'.\n", write_file_path);

    return 0;
}
