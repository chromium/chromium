// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/common/content_switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeLoggingTest : public testing::Test {
 public:
  // Stores the current value of the log file name environment
  // variable and sets the variable to new_value.
  void SaveEnvironmentVariable(const std::string& new_value) {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    if (!env->GetVar(env_vars::kLogFileName, &environment_filename_))
      environment_filename_ = "";

    env->SetVar(env_vars::kLogFileName, new_value);
  }

  // Restores the value of the log file nave environment variable
  // previously saved by SaveEnvironmentVariable().
  void RestoreEnvironmentVariable() {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar(env_vars::kLogFileName, environment_filename_);
  }

  void SetLogFileFlag(const std::string& value) {
    cmd_line_.AppendSwitchASCII(switches::kLogFile, value);
  }

  const base::CommandLine& cmd_line() { return cmd_line_; }

 private:
  std::string environment_filename_;  // Saves real environment value.
  base::CommandLine cmd_line_ =
      base::CommandLine(base::CommandLine::NO_PROGRAM);
};

// Tests the log file name getter without an environment variable.
TEST_F(ChromeLoggingTest, LogFileName) {
  SaveEnvironmentVariable(std::string());

  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("chrome_debug.log")));

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with an environment variable.
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable("test env value");

  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("test env value")).value(),
            filename.value());

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with a command-line flag.
TEST_F(ChromeLoggingTest, FlagLogFileName) {
  SetLogFileFlag("test flag value");

  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("test flag value")).value(),
            filename.value());
}

// Tests the log file name getter with with an environment variable and a
// command-line flag. The flag takes precedence.
TEST_F(ChromeLoggingTest, EnvironmentAndFlagLogFileName) {
  SaveEnvironmentVariable("test env value");
  SetLogFileFlag("test flag value");

  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("test flag value")).value(),
            filename.value());

  RestoreEnvironmentVariable();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeLoggingTest, TimestampedName) {
  base::FilePath path = base::FilePath(FILE_PATH_LITERAL("xy.zzy"));
  base::FilePath timestamped_path =
      logging::GenerateTimestampedName(path, base::Time::Now());
  EXPECT_THAT(timestamped_path.value(),
              ::testing::MatchesRegex("^xy_\\d+-\\d+\\.zzy$"));
}

TEST_F(ChromeLoggingTest, SetUpSymlink) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  base::FilePath bare_symlink_path =
      temp_dir_path.Append(FILE_PATH_LITERAL("chrome-test-log"));
  base::FilePath latest_symlink_path =
      temp_dir_path.Append(FILE_PATH_LITERAL("chrome-test-log.LATEST"));
  base::FilePath previous_symlink_path =
      temp_dir_path.Append(FILE_PATH_LITERAL("chrome-test-log.PREVIOUS"));

  // Start from a legacy situation, where "chrome-test-log" is a symlink
  // pointing to the latest log, which has a time-stamped name from a while
  // ago.
  base::FilePath old_target_path = logging::GenerateTimestampedName(
      bare_symlink_path, base::Time::UnixEpoch());

  ASSERT_TRUE(base::CreateSymbolicLink(old_target_path, bare_symlink_path));

  // Call the testee with the new symlink path, as if starting a new session.
  logging::SetUpSymlinkIfNeeded(latest_symlink_path,
                                /*start_new_log=*/true);

  // We now expect:
  //
  // chrome-test-log --> chrome-test-log.LATEST
  // chrome-test-log.LATEST --> <new time-stamped path>
  // no chrome-test-log.PREVIOUS on the legacy transition.
  base::FilePath target_path;
  ASSERT_TRUE(base::ReadSymbolicLink(bare_symlink_path, &target_path));
  EXPECT_EQ(target_path.value(), latest_symlink_path.value());

  base::FilePath latest_target_path;
  ASSERT_TRUE(base::ReadSymbolicLink(latest_symlink_path, &latest_target_path));
  EXPECT_NE(latest_target_path.value(), old_target_path.value());
  EXPECT_THAT(latest_target_path.value(),
              ::testing::MatchesRegex("^.*chrome-test-log_\\d+-\\d+$"));

  // Simulate one more session cycle.
  logging::SetUpSymlinkIfNeeded(latest_symlink_path, /*start_new_log=*/true);

  // We now expect:
  //
  // chrome-test-log.PREVIOUS --> <previous target of chrome-test-log.LATEST>
  //
  // We also expect that the .LATEST file is now pointing to a file with a
  // newer time stamp.  Unfortunately it's probably not newer enough to tell
  // the difference since the time stamp granularity is 1 second.
  ASSERT_TRUE(base::ReadSymbolicLink(previous_symlink_path, &target_path));
  EXPECT_EQ(target_path.value(), latest_target_path.value());

  ASSERT_TRUE(base::ReadSymbolicLink(latest_symlink_path, &latest_target_path));
  EXPECT_THAT(latest_target_path.value(),
              ::testing::MatchesRegex("^.*chrome-test-log_\\d+-\\d+$"));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
