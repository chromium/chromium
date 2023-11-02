// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "test_variable_static_library.h"

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringType arg = command_line.GetArgs()[0];
#if BUILDFLAG(IS_WIN)
  const std::string arg_narrow = base::WideToUTF8(arg);
#else
  const std::string arg_narrow = arg;
#endif
  do_something_in_sandbox_or_memory_safe_language(arg_narrow);
  return 0;
}
