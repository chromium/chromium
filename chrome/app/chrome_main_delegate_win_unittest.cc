// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_delegate.h"

#include <windows.h>

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Launches chrome.exe with a version switch and verifies that it prints the
// version and exits, rather than starting a browser.
class ChromeVersionSwitchesWinTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    // Ensure chrome.exe and anything it spawns are terminated when the test
    // ends, even if chrome.exe fails to exit on its own.
    job_.Set(::CreateJobObject(nullptr, nullptr));
    ASSERT_TRUE(job_.is_valid());
    ASSERT_TRUE(base::SetJobObjectLimitFlags(
        job_.get(), JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
  }

  // Runs chrome.exe with `version_switch`. Returns true if it exited within
  // the timeout, in which case `output` and `exit_code` are populated.
  bool RunChromeWithSwitch(std::string_view version_switch,
                           std::string* output,
                           int* exit_code) {
    const base::FilePath exe_dir = base::PathService::CheckedGet(base::DIR_EXE);
    base::CommandLine command_line(
        exe_dir.Append(chrome::kBrowserProcessExecutableName));
    command_line.AppendSwitchPath(switches::kUserDataDir,
                                  user_data_dir_.GetPath());
    command_line.AppendSwitch(version_switch);

    base::LaunchOptions options;
    options.job_handle = job_.get();
    return base::GetAppOutputWithExitCodeAndTimeout(
        command_line.GetCommandLineString(), /*include_stderr=*/false, output,
        exit_code, TestTimeouts::action_max_timeout(), options);
  }

 private:
  base::ScopedTempDir user_data_dir_;
  base::win::ScopedHandle job_;
};

TEST_F(ChromeVersionSwitchesWinTest, ProductVersion) {
  std::string output;
  int exit_code = -1;
  ASSERT_TRUE(
      RunChromeWithSwitch(switches::kProductVersion, &output, &exit_code));
  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(base::TrimWhitespaceASCII(output, base::TRIM_ALL),
            version_info::GetVersionNumber());
}

TEST_F(ChromeVersionSwitchesWinTest, Version) {
  std::string output;
  int exit_code = -1;
  ASSERT_TRUE(RunChromeWithSwitch(switches::kVersion, &output, &exit_code));
  EXPECT_EQ(exit_code, 0);
  EXPECT_THAT(output, ::testing::StartsWith(
                          base::StrCat({version_info::GetProductName(), " ",
                                        version_info::GetVersionNumber()})));
}

class ChromeMainDelegateWinDeathTest : public ::testing::Test {
 protected:
  class TestChromeMainDelegate : public ChromeMainDelegate {
   public:
    TestChromeMainDelegate() = default;

    using ChromeMainDelegate::BasicStartupComplete;
  };

  base::test::ScopedCommandLine scoped_command_line_;
  TestChromeMainDelegate chrome_main_delegate_;
};

TEST_F(ChromeMainDelegateWinDeathTest, ChildModuleVersion) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kChildModuleVersion);
  EXPECT_CHECK_DEATH((void)chrome_main_delegate_.BasicStartupComplete());
}

TEST_F(ChromeMainDelegateWinDeathTest, ChildModuleUserPathComponent) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kChildModuleUserPathComponent);
  EXPECT_CHECK_DEATH((void)chrome_main_delegate_.BasicStartupComplete());
}

}  // namespace
