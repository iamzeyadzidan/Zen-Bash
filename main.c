/* LIBRARIES */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <readline/readline.h>

/* DEFINITIONS */
#define COMMAND_LINE_SIZE 1024
#define WORDS_COUNT 128
#define MAX_VARS 16

/* FUNCTIONS DECLARATION */
void proc_exit();
void setup_environment();
void shell();
int readCommandLine(char *input);
int parseCommandLine(char *commandLine, char **parsedCommandLine, int i);
void cleanParsed(char **parsedCommandLine);
void clean(char *toBeCleaned);
int execute(char **parsedCommandLine);
int execute_cd(char **parsedCommandLine);
int execute_export(char **parsedCommandLine);
int execute_echo(char **parsedCommandLine);
void execute_command(char **parsedCommandLine);

/* GLOBAL VARIABLES */
FILE *fptr;
int vars = 0;                   // indicator to number of stored variables
char variables[MAX_VARS][1024]; // variable names storage
char values[MAX_VARS][1024];    // variable values storage
                                // where 16 is the limit of a variable naming.

void main()
{
    fptr = fopen("log.txt", "w+"); // open the file in write mode.
    // check zombie processes
    signal(SIGCHLD, proc_exit);
    // initiate shell
    shell();
}

void proc_exit()
{
    int wstat;
    pid_t pid;

    while (1)
    {
        pid = wait3(&wstat, WNOHANG, (struct rusage *)NULL);
        if (pid == 0)
            return;
        else if (pid == -1)
            return;
        else
        {
            fprintf(fptr, "Child Terminated: %d\n", wstat);
            fflush(fptr);
            printf("\nChild Terminated: %d\n", wstat);
        }
    }
}

void setup_environment()
{
    // prints directory each time it is called
    char cwd[512];
    getcwd(cwd, sizeof(cwd));
    printf("\n%s", cwd);
}

void shell()
{
    printf("\n\t ---------SIMPLE SHELL---------");

    // simple shell lifecycle is scan, parse, and execute.
    char commandLine[WORDS_COUNT];              // to scan
    char *parsedCommandLine[COMMAND_LINE_SIZE]; // to parse
    int status = 1;
    // to execute
    while (status)
    {
        setup_environment();
        // scanstdout
        if (readCommandLine(commandLine))
            continue;
        // parse
        parseCommandLine(commandLine, parsedCommandLine, 0);
        // execute
        status = execute(parsedCommandLine);
        for (int i = 0; i < WORDS_COUNT; i++)
        {
            parsedCommandLine[i] = NULL;
        }
        signal(SIGCHLD, proc_exit);
    }
}

/**
 * Reading user commands.
 */
int readCommandLine(char *input)
{
    // scan a line and store it in the buffer
    char *buffer = readline("\n# ");

    // if the buffer has no input, return 1.
    // if it has an input, store it in the associated string via strcpy and return 0.
    if (strlen(buffer) != 0)
    {
        strcpy(input, buffer);
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * Parsing user commands after reading it into data structure
 * that the OS would understand and operate on.
 */

// given a line, this function parses it starting from a given index.
// would be used to parse commands taken from user and parse commands from exports.
int parseCommandLine(char *commandLine, char **parsedCommandLine, int i)
{
    for (i; i < WORDS_COUNT; i++)
    {
        // separate each sequence of characters to individual arrays.
        parsedCommandLine[i] = strsep(&commandLine, " ");
        if (parsedCommandLine[i] == NULL)
        {
            break;
        }
        if (strlen(parsedCommandLine[i]) == 0)
        {
            i--;
        }
        if (parsedCommandLine[i][0] == '$')
        {
            char *expression = parsedCommandLine[i];
            expression = strtok(expression, "$"); // Gets whatever after the dollar sign
            int j = 0;
            int flag = -1;
            for (j; j < MAX_VARS; j++)
            {
                if (strcmp(variables[j], expression) == 0)
                {
                    flag = j;
                }
            }
            if (flag >= 0) // IF FOUND
            {
                i = parseCommandLine(values[flag], parsedCommandLine, i) - 1;
            }
        }
    }
    return i;
}

void cleanParsed(char **parsedCommandLine)
{
    for (int i = 0; i < WORDS_COUNT; i++)
    {
        if (parsedCommandLine[i] == NULL)
        {
            break;
        }
        clean(parsedCommandLine[i]); // cleans the W from quotes
    }
}

/**
 * Cleans a given string (array of characters) from all occurrences
 * of double quotation marks.
 */
void clean(char *toBeCleaned)
{
    int length = strlen(toBeCleaned);
    for (int j = 0; j < length; j++)
    {
        if (toBeCleaned[j] == '\"')
        {
            for (int k = j; k < length; k++)
            {
                toBeCleaned[k] = toBeCleaned[k + 1];
            }
            length--;
            j--;
        }
    }
}

/**
 * Executing user commands after parsing
 */
int execute(char **parsedCommandLine)
{
    if (parsedCommandLine[0] == NULL) // Input is --> (# \n)
    {
        printf("\nERROR, YOU MUST PROVIDE A VALID INPUT");
        fprintf(fptr, "\nERROR, YOU MUST PROVIDE A VALID INPUT");
        fflush(fptr);
        return 1;
    }
    // these cases are much self-explanatory.
    else if (strcmp(parsedCommandLine[0], "cd") == 0)
    {
        cleanParsed(parsedCommandLine);
        return execute_cd(parsedCommandLine);
    }
    else if (strcmp(parsedCommandLine[0], "echo") == 0)
    {
        cleanParsed(parsedCommandLine);
        return execute_echo(parsedCommandLine);
    }
    else if (strcmp(parsedCommandLine[0], "export") == 0)
    {
        return execute_export(parsedCommandLine);
    }
    else if (strcmp(parsedCommandLine[0], "exit") == 0)
    {
        // close the file that is being logged into upon exit.
        printf("EXIT COMMAND HAS BEEN INVOKED\n");
        fprintf(fptr, "EXIT COMMAND HAS BEEN INVOKED");
        fflush(fptr);
        fclose(fptr);
        exit(0);
    }
    /**
     * If no command matches the previous cases, this means that the user
     * has issued a command that needs to be executed via execvp(command, parsed) function.
     */
    cleanParsed(parsedCommandLine);
    execute_command(parsedCommandLine); // execvp()
    return 1;
}

/**
 * executing commands issued to cd
 * cd SPACE or cd tilda (~) redirects to home
 * otherwise redirect normally
 */
int execute_cd(char **parsedCommandLine)
{
    if (parsedCommandLine[1] == NULL)
    {
        printf("\nREDIRECTED TO HOME BY \" \".");
        fprintf(fptr, "\nREDIRECTED TO HOME BY \" \".");
        fflush(fptr);
        chdir(getenv("HOME"));
        return 1;
    }
    else if (strcmp(parsedCommandLine[1], "~") == 0)
    {
        printf("\nREDIRECTED TO HOME BY \"~\".");
        fprintf(fptr, "\nREDIRECTED TO HOME BY \"~\".");
        fflush(fptr);
        chdir(getenv("HOME"));
        return 1;
    }
    else
    {
        printf("\nREDIRECTED TO %s.", parsedCommandLine[1]);
        fprintf(fptr, "\nREDIRECTED TO %s.", parsedCommandLine[1]);
        fflush(fptr);
        chdir(parsedCommandLine[1]);
        return 1;
    }
}

/**
 * If the maximum number of variables reached (16) --> print an alert and return.
 * Otherwise, scan till = is encountered and save the scanned to variables array.
 * Next, scan till the end of the line. (Space is encountered)
 *
 * But what if a quotation mark is encountered? --> Keep scanning till another one is encountered.
 * Eventually store the scanned value after (=) in values array.
 *
 * vars is considered a pointer to the next variable to be exported.
 * getting a variable[i] value must invoke values[i]
 *
 * Please note that I handled the case of re-exporting by considering the latest value of the variable.
 * [export x="1"] then issuing [export x="2"] will not update x but will insert another variable with
 * the name x and value 2. Also when x is invoked as $x, 2 in this case would be the output.
 */
int execute_export(char **parsedCommandLine)
{
    if (vars == MAX_VARS)
    {
        printf("\nMAXIMUM VARIABLES EXCEEDED, CANNOT ADD ANOTHER.");
        fprintf(fptr, "\nMAXIMUM VARIABLES EXCEEDED, CANNOT ADD ANOTHER.");
        fflush(fptr);
        return 1;
    }
    int varsIndex = 0;
    int valsIndex = 0;
    int i = 0;
    // scan till = is encountered and save the scanned to variables
    for (i; i < strlen(parsedCommandLine[1]); i++)
    {
        if (parsedCommandLine[1][i] == '=')
        {
            i++;
            break;
        }
        variables[vars][varsIndex++] = parsedCommandLine[1][i];
    }
    // scan after = and store the scanned in values.
    for (i; i < strlen(parsedCommandLine[1]); i++)
    {
        values[vars][valsIndex++] = parsedCommandLine[1][i];
    }

    /**
     * If the first scanned word had a ", then we must fulfill it by scanning
     * the whole line and encounter another double quotation mark but ends with \0
     */
    if (parsedCommandLine[1][varsIndex + 1] == '\"')
    {
        // scanning a the rest of W inserted between "" or ''
        for (int index = 2; index < WORDS_COUNT; index++)
        {
            if (parsedCommandLine[index] == NULL)
            {
                break;
            }
            values[vars][valsIndex++] = ' ';
            for (i = 0; i < strlen(parsedCommandLine[index]); i++)
            {
                values[vars][valsIndex++] = parsedCommandLine[index][i];
            }
            if (values[vars][valsIndex] == '\"' && parsedCommandLine[index][i + 1] == '\0')
            {
                break;
            }
        }
    }
    clean(values[vars]);
    printf("EXPORTED %s, WITH VALUE = %s", variables[vars], values[vars]);
    fprintf(fptr, "\nEXPORTED %s, WITH VALUE = %s", variables[vars], values[vars]);
    fflush(fptr);
    vars++;
    return 1;
}

/**
 * echo just prints whatever goes after the word echo after
 * parsing the user command.
 */
int execute_echo(char **parsedCommandLine)
{
    for (int i = 1; i < WORDS_COUNT; i++)
    {
        if (parsedCommandLine[i] == NULL)
        {
            break;
        }
        printf("%s ", parsedCommandLine[i]);
    }
    return 1;
}

void execute_command(char **parsedCommandLine)
{
    pid_t pid, wpid;
    int status;
    int count = 0;
    for (count; count < WORDS_COUNT; count++)
    {
        if (parsedCommandLine[count] == NULL)
        {
            break;
        }
    }
    int last_letter = parsedCommandLine[count - 1][strlen(parsedCommandLine[count - 1]) - 1];
    int shouldnt_wait = (last_letter == '&');

    pid = fork();
    if (pid == 0)
    {
        // Child process
        if (execvp(parsedCommandLine[0], parsedCommandLine) == -1)
        {
            printf("COULDN\'T EXECUTE COMMAND [EXECVP() ERROR TRACE]");
            fprintf(fptr, "\nCOULDN\'T EXECUTE COMMAND [EXECVP() ERROR TRACE]");
            fflush(fptr);
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        // Error forking
        printf("FAILED FORKING A CHILD");
        fprintf(fptr, "\nFAILED FORKING A CHILD");
        fflush(fptr);
    }
    else
    {
        if (!shouldnt_wait)
        {
            wpid = waitpid(pid, &status, WUNTRACED);
        }
    }
}