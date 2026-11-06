// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/logging_chrome.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/common/env_vars.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeLoggingTest : public testing::Test {
 public:
  // Stores the current value of the log file name environment
  // variable and sets the variable to new_value.
  void SaveEnvironmentVariable(const std::string& new_value) {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    environment_filename_ = env->GetVar(env_vars::kLogFileName).value_or("");

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
