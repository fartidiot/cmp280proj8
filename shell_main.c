#include "command_line.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_LINE_LENGTH 512

char cwd[MAX_LINE_LENGTH];

volatile sig_atomic_t terminated = 0;

void handle_sig(int sig) {
    if(sig == SIGCHLD)
    {
      int saved_errno = errno;
      int status;
      pid_t pid;
      while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
          printf("[proc %d exited with code %d]\n",
                 pid, WEXITSTATUS(status));
          if (WIFSIGNALED(status)) {
              printf("Exit signal %d", WTERMSIG(status));
        }
      }

      errno = saved_errno;
    }

    else if(sig == SIGTERM)
    {
      terminated = 1;
    }

}

void setCwd() {
    char full_cwd[PATH_MAX];
    if (getcwd(full_cwd, PATH_MAX) == NULL) {
        printf("could not get directory name");
        strcpy(cwd, "");
    } else {
        char short_cwd[PATH_MAX];
        int cwdIndex = 0;
        int i;
        int lastSlashIndex = 0;
        for (i = 0; full_cwd[i] != 0; i++) {
            char c = full_cwd[i];
            if (c == '/') {
                lastSlashIndex = i;
                short_cwd[cwdIndex++] = '/';
                short_cwd[cwdIndex++] = full_cwd[i + 1];
            }
        }
        cwdIndex -= 1;
        for (int j = lastSlashIndex + 1; j < i; j++) {
            short_cwd[cwdIndex++] = full_cwd[j];
        }

        short_cwd[cwdIndex] = 0;
        strncpy(cwd, short_cwd, MAX_LINE_LENGTH);

    }
}

void changeDir(char *newDirectory) {
    int response = chdir(newDirectory);
    if (response == -1) {
        switch (errno) {
            case ENOENT:
                printf("location not found!\n");
                break;
            default:
                printf("cd failed due to an error\n");
                break;
        }
    } else {
        setCwd();
    }
}

void execCommand(int argCount, char **arguments) {
    char *first = arguments[0];
    for(int i = 0; i < arguments.length - 1; i++)
    {
      char *curr = arguments[i];
      if(strcmp(curr, "<"))
      {
        FILE *input;
        curr = arguments[i + 1];
        input = open(curr,O_RDONLY);
        if(input == NULL)
        {
          printf("Input file does not exist");
          return;
        }
        dup2(input,STDIN_FILENO);
        close(input);
      }
      else if(strcmp(curr, ">"))
      {
        curr = arguments[i+1];
        FILE *output;
        output = open(curr,O_WRONLY|O_CREAT);
        if(output == NULL)
        {
          printf("Could not create output file");
          return;
        }
        dup2(output,STDOUT_FILENO);
        close(output);
      }
    }
    if (strcmp(first, "cd") == 0) {
        if (argCount < 2) {
            changeDir(getenv("HOME"));
        } else {
            changeDir(arguments[1]);
        }
    } else {

        execvp(first, arguments);

    }
}

void executeLineForeground(struct CommandLine command) {

    pid_t child_pid = fork();
    if (child_pid == 0) {
        execCommand(command.argCount, command.arguments);
    } else {
        sigset_t prev_one, mask_one;
        sigemptyset(&mask_one);
        sigaddset(&mask_one, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        waitpid(child_pid, 0, 0);
        sigprocmask(SIG_SETMASK, &prev_one, NULL);
    }

}


void executeLineBackground(struct CommandLine command) {
    if (strcmp(command.arguments[0], "cd") == 0) {
        printf("please change directories in the foreground\n");
    }
    int child_pid = fork();
    if (child_pid == 0) {
        execCommand(command.argCount, command.arguments);
    }
}


int main(int argc, const char **argv) {
    char cmdline[MAX_LINE_LENGTH];
    struct sigaction sa;

    sa.sa_handler = &handle_sig;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);


    setCwd();

    struct CommandLine command;
    while(!terminated) {
        printf("%s> ", cwd);

        fgets(cmdline, MAX_LINE_LENGTH, stdin);
        if (feof(stdin)) {
            exit(0);
        }

        bool gotLine = parseLine(&command, cmdline);
        if (gotLine) {
            if (command.background) {
                executeLineBackground(command);
            } else {
                executeLineForeground(command);
            }

            freeCommand(&command);
        }
        if (sigaction(SIGCHLD, &sa, 0) == -1) {

            perror(0);

            exit(1);

        }

    }
}

