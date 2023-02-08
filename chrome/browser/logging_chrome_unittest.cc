// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
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
#if BUILDFLAG(IS_WIN)
  ASSERT_TRUE(filename.IsAbsolute());
#endif  // BUILDFLAG(IS_WIN)

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with an environment variable.
#if BUILDFLAG(IS_WIN)
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable("c:\\path\\test env value");
  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("test env value")));
  ASSERT_TRUE(filename.IsAbsolute());
  RestoreEnvironmentVariable();
}
#else
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable("test env value");
  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("test env value")));
  RestoreEnvironmentVariable();
}
#endif  // BUILDFLAG(IS_WIN)

// Tests the log file name getter with a command-line flag.
#if BUILDFLAG(IS_WIN)
TEST_F(ChromeLoggingTest, FlagLogFileName) {
  SetLogFileFlag("c:\\path\\test flag value");
  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("test flag value")));
  ASSERT_TRUE(filename.IsAbsolute());
}
// Non-absolute path falls back to default.
TEST_F(ChromeLoggingTest, FlagLogFileNameNonAbsolute) {
  SetLogFileFlag("test file value");
  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("chrome_debug.log")));
  ASSERT_TRUE(filename.IsAbsolute());
}
#else
TEST_F(ChromeLoggingTest, FlagLogFileName) {
  SetLogFileFlag("test flag value");
  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("test flag value")));
}
#endif  // BUILDFLAG(IS_WIN)

// Tests the log file name getter with with an environment variable and a
// command-line flag. The flag takes precedence.
#if BUILDFLAG(IS_WIN)
TEST_F(ChromeLoggingTest, EnvironmentAndFlagLogFileName) {
  SaveEnvironmentVariable("c:\\path\\test env value");
  SetLogFileFlag("d:\\path\\test flag value");

  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("test flag value")));
  ASSERT_TRUE(filename.IsAbsolute());
  RestoreEnvironmentVariable();
}
#else
TEST_F(ChromeLoggingTest, EnvironmentAndFlagLogFileName) {
  SaveEnvironmentVariable("test env value");
  SetLogFileFlag("test flag value");

  base::FilePath filename = logging::GetLogFileName(cmd_line());
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("test flag value")));
  RestoreEnvironmentVariable();
}
#endif  // BUILDFLAG(IS_WIN)

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
      temp_dir_path.AppendASCII("chrome-test-log");
  base::FilePath latest_symlink_path =
      temp_dir_path.AppendASCII("chrome-test-log.LATEST");
  base::FilePath previous_symlink_path =
      temp_dir_path.AppendASCII("chrome-test-log.PREVIOUS");

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

// Test the case of the normal rotation.
TEST_F(ChromeLoggingTest, RotateLogFiles) {
  constexpr char kLog1Content[] = "log#1\n";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  base::FilePath log_path_latest = temp_dir_path.AppendASCII("chrome-test-log");

  // Prepare the latest log file.
  ASSERT_TRUE(base::WriteFile(log_path_latest, kLog1Content));
  base::File::Info file_info;
  base::File(log_path_latest, base::File::FLAG_OPEN | base::File::FLAG_READ)
      .GetInfo(&file_info);
  base::Time creation_time = file_info.creation_time;

  // Generate the log file path which is rotated to.
  base::FilePath expected_rotated_path =
      logging::GenerateTimestampedName(log_path_latest, creation_time);

  // Check the condition before rotation.
  {
    EXPECT_TRUE(base::PathExists(log_path_latest));
    EXPECT_FALSE(base::PathExists(expected_rotated_path));
  }

  // Simulate the rotation.
  ASSERT_TRUE(logging::RotateLogFile(log_path_latest));

  // Check the conditions after rotation: the log file and the rotated log file.
  {
    EXPECT_FALSE(base::PathExists(log_path_latest));
    EXPECT_TRUE(base::PathExists(expected_rotated_path));

    std::string buffer;
    base::ReadFileToString(expected_rotated_path, &buffer);
    EXPECT_EQ(buffer, kLog1Content);
  }
}

// Test the case that chrome tries the rotation but there is no files.
TEST_F(ChromeLoggingTest, RotateLogFilesNoFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp_dir_path = temp_dir.GetPath();

  base::FilePath log_path_latest = temp_dir_path.AppendASCII("chrome-test-log");

  // Check the condition before rotation.
  {
    EXPECT_FALSE(base::PathExists(log_path_latest));

    // Ensure no file in the directory.
    base::FileEnumerator enumerator(temp_dir_path, true,
                                    base::FileEnumerator::FILES);
    EXPECT_TRUE(enumerator.Next().empty());
  }

  // Simulate the rotation.
  ASSERT_TRUE(logging::RotateLogFile(log_path_latest));

  // Check the condition after rotation: nothing happens.
  {
    EXPECT_FALSE(base::PathExists(log_path_latest));

    // Ensure still no file in the directory.
    base::FileEnumerator enumerator(temp_dir_path, true,
                                    base::FileEnumerator::FILES);
    EXPECT_TRUE(enumerator.Next().empty());
  }
}

// Test the case that chrome tries the rotation but the target path already
// exists. The logic should use the altanate target path.
TEST_F(ChromeLoggingTest, RotateLogFilesExisting) {
  constexpr char kLatestLogContent[] = "log#1\n";
  constexpr char kOldLogContent[] = "log#2\n";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  base::FilePath log_path_latest = temp_dir_path.AppendASCII("chrome-test-log");

  // Prepare the latest log file.
  ASSERT_TRUE(base::WriteFile(log_path_latest, kLatestLogContent));
  base::File::Info file_info;
  {
    base::File(log_path_latest, base::File::FLAG_OPEN | base::File::FLAG_READ)
        .GetInfo(&file_info);
  }
  base::Time creation_time = file_info.creation_time;

  base::FilePath exist_log_path =
      logging::GenerateTimestampedName(log_path_latest, creation_time);
  ASSERT_TRUE(base::WriteFile(exist_log_path, kOldLogContent));

  base::FilePath rotated_log_path = logging::GenerateTimestampedName(
      log_path_latest, creation_time + base::Seconds(1));

  // Check the condition before rotation.
  {
    // The latest log file exists.
    EXPECT_TRUE(base::PathExists(log_path_latest));
    // First candidate does already exist.
    EXPECT_TRUE(base::PathExists(exist_log_path));
    // Second candidate does already exist.
    EXPECT_FALSE(base::PathExists(rotated_log_path));
  }

  // Simulate one more session cycle.
  ASSERT_TRUE(logging::RotateLogFile(log_path_latest));

  // Check the condition after rotation: the log file is renamed to the second
  // candidate.
  {
    EXPECT_FALSE(base::PathExists(log_path_latest));
    EXPECT_TRUE(base::PathExists(exist_log_path));
    EXPECT_TRUE(base::PathExists(rotated_log_path));

    std::string buffer;
    // The first candidate is kept.
    base::ReadFileToString(exist_log_path, &buffer);
    EXPECT_EQ(buffer, kOldLogContent);
    // The second candidate is the previous latest log.
    base::ReadFileToString(rotated_log_path, &buffer);
    EXPECT_EQ(buffer, kLatestLogContent);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
