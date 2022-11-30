// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(LaunchWinTest, GetAppOutputWithExitCodeShouldReturnExitCode) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("cmd")));
  cl.AppendArg("/c");
  cl.AppendArg("this-is-not-an-application");
  std::string output;
  int exit_code;
  ASSERT_TRUE(GetAppOutputWithExitCode(cl, &output, &exit_code));
  ASSERT_TRUE(output.empty());
  ASSERT_EQ(1, exit_code);
}

}  // namespace
