// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(LaunchTest, GetAppOutputWithInvalidExecutableShouldFail) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("executable_does_not_exist")));
  std::string output;
  ASSERT_FALSE(GetAppOutput(cl, &output));

#if !BUILDFLAG(IS_IOS)
  // iOS does not support `GetAppOutputWithExitCode`.
  int exit_code = {};
  const bool succeeded = GetAppOutputWithExitCode(cl, &output, &exit_code);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
  ASSERT_FALSE(succeeded);
#else
  // Other platforms return code `127` for an executable that does not exist.
  ASSERT_TRUE(succeeded);
  ASSERT_EQ(exit_code, 127);
#endif  // #if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
#endif  // #if !BUILDFLAG(IS_IOS)
}

}  // namespace base
