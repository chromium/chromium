// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Run a Rust executable that attempts to call a function through an invalid
// pointer. This triggers a control flow guard exception and exits the process
// with STATUS_STACK_BUFFER_OVERRUN.
TEST(RustCfgWin, CfgCatchesInvalidIndirectCall) {
  base::LaunchOptions o;
  o.start_hidden = true;
  // From //build/rust/tests/test_control_flow_guard.
  base::CommandLine cmd(base::FilePath(
      FILE_PATH_LITERAL("test_control_flow_guard.exe")));
  base::Process proc = base::LaunchProcess(cmd, o);
  int exit_code;
  EXPECT_TRUE(proc.WaitForExit(&exit_code));
  const auto u_exit_code = static_cast<unsigned long>(exit_code);
  EXPECT_EQ(u_exit_code, STATUS_STACK_BUFFER_OVERRUN);
}

}  // namespace
