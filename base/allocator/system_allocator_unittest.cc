// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SystemAllocatorTest, HelperBinary) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
#if BUILDFLAG(IS_WIN)
  exe_path = exe_path.AppendASCII("system_allocator_test_helper.exe");
#else
  exe_path = exe_path.AppendASCII("system_allocator_test_helper");
#endif

  base::LaunchOptions options;
  base::Process process =
      base::LaunchProcess(base::CommandLine(exe_path), options);
  ASSERT_TRUE(process.IsValid());

  int exit_code = -1;
  ASSERT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, 0);
}

}  // namespace base
