// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple testing command, used to exercise child process launcher calls.
//
// Usage:
//        echo_test_helper [-x exit_code] arg0 arg1 arg2...
//        Prints arg0..n to stdout with space delimiters between args,
//        returning "exit_code" if -x is specified.
//
//        echo_test_helper -e env_var
//        Prints the environmental variable |env_var| to stdout.
int main(int argc, char** argv) {
  if (strcmp(argv[1], "-e") == 0) {
    if (argc != 3) {
      return 1;
    }

    const char* env = getenv(argv[2]);
    if (env != NULL) {
      printf("%s", env);
    }
  } else {
    int return_code = 0;
    int start_idx = 1;

    if (strcmp(argv[1], "-x") == 0) {
      return_code = atoi(argv[2]);
      start_idx = 3;
    }

    for (int i = start_idx; i < argc; ++i) {
      printf((i < argc - 1 ? "%s " : "%s"), argv[i]);
    }

    return return_code;
  }
}
