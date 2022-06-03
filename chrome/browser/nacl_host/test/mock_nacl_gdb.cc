// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstring>
#include <memory>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/notreached.h"

static const char kEvalCommand[] = "--eval-command";
static const char kCommand[] = "--command";
static const char kNaClIrt[] = "nacl-irt \"";
static const char kPass[] = "PASS";
static const char kAttach[] = "target remote :4014";

int main(int argc, char** argv) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string mock_nacl_gdb_file;
  env->GetVar("MOCK_NACL_GDB", &mock_nacl_gdb_file);
  CHECK_GE(argc, 5);
  // First argument should be --eval-command.
  CHECK_EQ(strcmp(argv[1], kEvalCommand), 0);
  // Second argument should start with nacl-irt.
  CHECK_GE(strlen(argv[2]), strlen(kNaClIrt));
  CHECK_EQ(strncmp(argv[2], kNaClIrt, strlen(kNaClIrt)), 0);
  char* irt_file_name = strdup(&argv[2][strlen(kNaClIrt)]);
  CHECK_GE(strlen(irt_file_name), 1u);
  // Remove closing quote.
  irt_file_name[strlen(irt_file_name) - 1] = 0;
  FILE* irt_file = fopen(irt_file_name, "r");
  free(irt_file_name);
  // nacl-irt parameter must be a file name.
  PCHECK(irt_file);
  fclose(irt_file);
  CHECK_EQ(strcmp(argv[3], kEvalCommand), 0);
  CHECK_EQ(strcmp(argv[4], kAttach), 0);
  int i = 5;
  // Skip additional --eval-command parameters.
  while (i < argc) {
    if (strcmp(argv[i], kEvalCommand) == 0) {
      i += 2;
      continue;
    }
    if (strcmp(argv[i], kCommand) == 0) {
      // Command line shouldn't end with --command switch without value.
      i += 2;
      CHECK_LE(i, argc);
      std::string nacl_gdb_script(argv[i - 1]);
      base::WriteFile(base::FilePath::FromUTF8Unsafe(nacl_gdb_script),
                      kPass, sizeof(kPass) - 1);
      continue;
    }
    // Unknown argument.
    NOTREACHED() << "Invalid argument " << argv[i];
  }
  CHECK_EQ(i, argc);
  base::WriteFile(base::FilePath::FromUTF8Unsafe(mock_nacl_gdb_file),
                  kPass, sizeof(kPass) - 1);
  return 0;
}
