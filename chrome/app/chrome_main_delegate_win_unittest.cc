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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/app/startup_timestamps_win.h"
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

TEST(StartupTimestampsWinTest, GetApplicationStartQpcDeltaValidStubSwitch) {
  const base::TimeDelta current_qpc = base::Hours(2);
  const base::TimeDelta stub_qpc = current_qpc - base::Milliseconds(250);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch(switches::kIsolated);
  cmd.AppendSwitchASCII(switches::kIsolatedStubStartQpcUs,
                        base::NumberToString(stub_qpc.InMicroseconds()));

  EXPECT_EQ(GetApplicationStartQpcDeltaForTesting(cmd, "", current_qpc),
            stub_qpc);

  // Exact boundary values: elapsed == 0 and elapsed == kMaxEntryPointQpcAge.
  cmd.RemoveSwitch(switches::kIsolatedStubStartQpcUs);
  cmd.AppendSwitchASCII(switches::kIsolatedStubStartQpcUs,
                        base::NumberToString(current_qpc.InMicroseconds()));
  EXPECT_EQ(GetApplicationStartQpcDeltaForTesting(cmd, "", current_qpc),
            current_qpc);

  const base::TimeDelta oldest_valid = current_qpc - kMaxEntryPointQpcAge;
  cmd.RemoveSwitch(switches::kIsolatedStubStartQpcUs);
  cmd.AppendSwitchASCII(switches::kIsolatedStubStartQpcUs,
                        base::NumberToString(oldest_valid.InMicroseconds()));
  EXPECT_EQ(GetApplicationStartQpcDeltaForTesting(cmd, "", current_qpc),
            oldest_valid);
}

TEST(StartupTimestampsWinTest, GetApplicationStartQpcDeltaIgnoresNonIsolated) {
  const base::TimeDelta current_qpc = base::Hours(2);
  const base::TimeDelta stub_qpc = current_qpc - base::Milliseconds(250);

  // Missing --isolated switch.
  base::CommandLine non_isolated_cmd(base::CommandLine::NO_PROGRAM);
  non_isolated_cmd.AppendSwitchASCII(
      switches::kIsolatedStubStartQpcUs,
      base::NumberToString(stub_qpc.InMicroseconds()));
  EXPECT_EQ(
      GetApplicationStartQpcDeltaForTesting(non_isolated_cmd, "", current_qpc),
      current_qpc);

  // Non-browser process type even with --isolated.
  base::CommandLine child_cmd(base::CommandLine::NO_PROGRAM);
  child_cmd.AppendSwitch(switches::kIsolated);
  child_cmd.AppendSwitchASCII(switches::kIsolatedStubStartQpcUs,
                              base::NumberToString(stub_qpc.InMicroseconds()));
  EXPECT_EQ(
      GetApplicationStartQpcDeltaForTesting(child_cmd, "renderer", current_qpc),
      current_qpc);

  // Missing stub QPC switch.
  base::CommandLine no_stub_cmd(base::CommandLine::NO_PROGRAM);
  no_stub_cmd.AppendSwitch(switches::kIsolated);
  EXPECT_EQ(GetApplicationStartQpcDeltaForTesting(no_stub_cmd, "", current_qpc),
            current_qpc);
}

TEST(StartupTimestampsWinTest, GetApplicationStartQpcDeltaRejectsInvalid) {
  const base::TimeDelta current_qpc = base::Hours(2);

  const std::string kInvalidValues[] = {
      "",
      "not-a-number",
      "0",
      "-100",
      "99999999999999999999999",
      "9223372036854775807",  // INT64_MAX (future)
      "1",                    // Untrusted caller passing 1us (> 1h old)
      base::NumberToString(
          (current_qpc + base::Microseconds(1)).InMicroseconds()),
      base::NumberToString(
          (current_qpc - kMaxEntryPointQpcAge - base::Microseconds(1))
              .InMicroseconds()),
  };

  for (const std::string& value : kInvalidValues) {
    SCOPED_TRACE(value);
    base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
    cmd.AppendSwitch(switches::kIsolated);
    cmd.AppendSwitchASCII(switches::kIsolatedStubStartQpcUs, value);
    EXPECT_EQ(GetApplicationStartQpcDeltaForTesting(cmd, "", current_qpc),
              current_qpc);
  }
}

TEST(StartupTimestampsWinTest, ResolveEntryPointQpcDeltaHighResolution) {
  const base::TimeTicks now = base::TimeTicks() + base::Seconds(500);
  const base::TimeDelta qpc_delta = base::Seconds(499);

  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                qpc_delta, now, /*is_high_resolution=*/true, base::TimeDelta()),
            base::TimeTicks() + qpc_delta);

  // Zero and negative QPC deltas fall back to `now`.
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(base::TimeDelta(), now,
                                                /*is_high_resolution=*/true,
                                                base::TimeDelta()),
            now);
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(-base::Seconds(1), now,
                                                /*is_high_resolution=*/true,
                                                base::TimeDelta()),
            now);
}

TEST(StartupTimestampsWinTest, ResolveEntryPointQpcDeltaLowResolution) {
  const base::TimeTicks now = base::TimeTicks() + base::Hours(2);
  const base::TimeDelta current_qpc = base::Hours(5);

  // Normal elapsed offset.
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc - base::Milliseconds(250), now,
                /*is_high_resolution=*/false, current_qpc),
            now - base::Milliseconds(250));

  // Zero elapsed and exact max-age boundary.
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc, now, /*is_high_resolution=*/false, current_qpc),
            now);
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc - kMaxEntryPointQpcAge, now,
                /*is_high_resolution=*/false, current_qpc),
            now - kMaxEntryPointQpcAge);

  // Negative elapsed (QPC ahead of current) and > max-age fall back to `now`.
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc + base::Microseconds(1), now,
                /*is_high_resolution=*/false, current_qpc),
            now);
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc - kMaxEntryPointQpcAge - base::Microseconds(1), now,
                /*is_high_resolution=*/false, current_qpc),
            now);

  // Zero and negative QPC deltas fall back to `now`.
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(base::TimeDelta(), now,
                                                /*is_high_resolution=*/false,
                                                base::Seconds(10)),
            now);
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(-base::Seconds(1), now,
                                                /*is_high_resolution=*/false,
                                                base::Seconds(10)),
            now);

  // Low-res `now` shortly after boot or 32-bit timeGetTime() rollover where
  // `elapsed >= now.since_origin()` must fall back to `now` rather than
  // producing a null or negative TimeTicks.
  const base::TimeTicks early_now = base::TimeTicks() + base::Milliseconds(100);
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc - base::Milliseconds(100), early_now,
                /*is_high_resolution=*/false, current_qpc),
            early_now);
  EXPECT_EQ(ResolveEntryPointQpcDeltaForTesting(
                current_qpc - base::Milliseconds(250), early_now,
                /*is_high_resolution=*/false, current_qpc),
            early_now);
}

}  // namespace
