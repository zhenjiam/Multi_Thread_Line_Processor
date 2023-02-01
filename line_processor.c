#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define LINE_COUNT 50      // 49 lines + 1 stop processing line
#define LINE_CHAR 1000

int total = 0;      // number of lines to process before STOP\n

// Buffer 1, shared resource between input thread and line separator thread
char buf_1[LINE_COUNT][LINE_CHAR];
int count_1= 0;
pthread_mutex_t mutex_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_1 = PTHREAD_COND_INITIALIZER;

// Buffer 2, shared resource between line separator thread and plus sign thread
char buf_2[LINE_COUNT][LINE_CHAR];
int count_2= 0;
pthread_mutex_t mutex_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_2 = PTHREAD_COND_INITIALIZER;

// Buffer 3, shared resource between plus sign thread and output thread
char buf_3[LINE_COUNT][LINE_CHAR];
int count_3= 0;
pthread_mutex_t mutex_3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_3 = PTHREAD_COND_INITIALIZER;

/* Function for replacing every occurrence of "++" with "^". Used tutorial code on substring replacement 
from URL: https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c
*/
void replace(char *target) {
    const char pluses[] = "++";
    const char new[] = "^";
    char buffer[LINE_CHAR] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t old_len = strlen(pluses);
    size_t repl_len = strlen(new);

    while (1) {
        const char *p = strstr(tmp, pluses);

        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }

        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;

        memcpy(insert_point, new, repl_len);
        insert_point += repl_len;

        tmp = p + old_len;
    }
    strcpy(target, buffer);
}

// Function for finding STOP\n for the terminating line
int find_stop(char *input) {
    const char *stop = "STOP";
    char *result = strstr(input, stop);
    int position = result - input;

    if (position == 0 && input[4] == '\n') {
        if (!isalnum(input[5]) && !ispunct(input[5])) {
            return 1;
        }
    }
    return 0;
}

// Function for putting processed line from Input Thread into buffer 1
void put_buf_1(char *input, int line) {
    pthread_mutex_lock(&mutex_1);
    strcpy(buf_1[line], input);
    count_1++;
    pthread_cond_signal(&full_1);
    pthread_mutex_unlock(&mutex_1);
}

// Function for Input Thread
void *input_thread(void *args) {
    char input[LINE_COUNT][LINE_CHAR];
    int *cnt = &total;

    // Get input and store in 2d array.
    for (int i = 0; i < LINE_COUNT; ++i){
        if (fgets(input[i], sizeof(*input), stdin) == NULL) {
            break;
        }
        // Find stop command and only deal with lines before that.
        int found = find_stop(input[i]);
        if (found == 1) {
            break;
        }
        // Put the lines in buffer 1
        put_buf_1(input[i], i);
        *cnt = i+1;
    }
    return NULL;
}

// Function for getting line from buffer 1 for Line Separator Thread
void get_buf_1(char *output, int line) {
    pthread_mutex_lock(&mutex_1);
    while (count_1 == 0) {
        pthread_cond_wait(&full_1, &mutex_1);
    }
    strcpy(output, buf_1[line]);
    count_1--;
    pthread_mutex_unlock(&mutex_1);
}

// Function for putting line processed from Line Separator Thread into buffer 2
void put_buf_2(char *input, int line) {
    pthread_mutex_lock(&mutex_2);
    strcpy(buf_2[line], input);
    count_2++;
    pthread_cond_signal(&full_2);
    pthread_mutex_unlock(&mutex_2);
}

// Function for Line Separator Thread
void *line_separator_thread(void *args){
    while (total == 0){         // necessary because this thread is faster than input_thread
        continue;
    }
    char input[LINE_CHAR];
    for (int i = 0; i < total; ++i) {
        // get line from buffer 1
        get_buf_1(input, i);
        // replace newline with space
        input[strcspn(input, "\n")] = ' ';
        // put processed line to buffer 2
        put_buf_2(input, i);
    }
    return NULL;
}

// Function for getting line from buffer 2 for Plus Sign Thread
void get_buf_2(char *output, int line) {
    pthread_mutex_lock(&mutex_2);
    while (count_2 == 0) {
        pthread_cond_wait(&full_2, &mutex_2);
    }
    strcpy(output, buf_2[line]);
    count_2--;
    pthread_mutex_unlock(&mutex_2);
}

// Function for putting line processed by Plus Sign Thread to buffer 3
void put_buf_3(char *input, int line) {
    pthread_mutex_lock(&mutex_3);
    strcpy(buf_3[line], input);
    count_3++;
    pthread_cond_signal(&full_3);
    pthread_mutex_unlock(&mutex_3);
}

// Function for Plus Sign Thread
void *plus_sign_thread(void *args) {
    while (total == 0){         // necessary because this thread is faster than input_thread
        continue;
    }
    char input[LINE_CHAR];
    for (int i = 0; i < total; ++i) {
        get_buf_2(input, i);
        replace(input);
        put_buf_3(input, i);
    }
    return NULL;
}

// Function for getting line from buffer 3 for Output Thread
void get_buf_3(char *output, int line) {
    pthread_mutex_lock(&mutex_3);
    while (count_3 == 0) {
        pthread_cond_wait(&full_3, &mutex_3);
    }
    strcpy(output, buf_3[line]);
    count_3--;
    pthread_mutex_unlock(&mutex_3);
}

// Function for Output Thread
void *output_thread(void *args) {
    while (total == 0){         // necessary because this thread is faster than input_thread
        continue;
    }
    char input[LINE_CHAR];
    int str_size = 80;
    char substring[str_size];   // to hold lines of 80 characters for printing

    int ii = 0;      // keeps track of the placement of characters from input to substring.
    for (int i = 0; i < total; ++i) {
        get_buf_3(input, i);
        int idx = 0;
        int print_flag = 0;
        
        while (input[idx] != '\0') {
            for (ii; ii < str_size; ++ii) {
                // if haven't reached end of line for input
                if (input[idx] != '\0') {
                    substring[ii] = input[idx];
                    idx++;
                }
                // reached end of line for input so break from iterating
                else {
                    break;
                }
                // if have enough characters to print string
                if (ii == str_size-1) {
                    print_flag = 1;
                }
                else {
                    print_flag = 0;
                }
            }
            if (print_flag == 1) {
                ii = 0;
                printf("%s\n", substring);
            }         
        }
        
    }
    fflush(stdout);
    return NULL;
}


int main(){
    pthread_t input_t, line_sep_t, plus_sign_t, output_t;
    // Create the threads
    pthread_create(&input_t, NULL, input_thread, NULL);
    pthread_create(&line_sep_t, NULL, line_separator_thread, NULL);
    pthread_create(&plus_sign_t, NULL, plus_sign_thread, NULL);
    pthread_create(&output_t, NULL, output_thread, NULL);
    // Wait for the threads to terminate
    pthread_join(input_t, NULL);
    pthread_join(line_sep_t, NULL);
    pthread_join(plus_sign_t, NULL);
    pthread_join(output_t, NULL);
    return EXIT_SUCCESS;
}


