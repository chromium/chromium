// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(LaunchWinTest, GetAppOutputWithExitCodeShouldReturnExitCode) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("cmd")));
  cl.AppendArg("/c");
  cl.AppendArg("this-is-not-an-application");
  std::string output;
  int exit_code = 0;
  ASSERT_TRUE(GetAppOutputWithExitCode(cl, &output, &exit_code));
  ASSERT_TRUE(output.empty());
  ASSERT_EQ(exit_code, 1);
}

TEST(LaunchWinTest, GetAppOutputWithExitCodeAndTimeout_SuccessStdErrOutput) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("cmd")));
  cl.AppendArg("/c");
  cl.AppendArg("this-is-not-an-application");
  std::string output;
  int exit_code = 0;
  ASSERT_TRUE(GetAppOutputWithExitCodeAndTimeout(
      cl.GetCommandLineString(), true, &output, &exit_code, base::Seconds(5)));
  ASSERT_GT(output.length(), 0);
  ASSERT_EQ(exit_code, 1);
}

TEST(LaunchWinTest, GetAppOutputWithExitCodeAndTimeout_SuccessOutput) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("cmd")));
  cl.AppendArg("/c");
  cl.AppendArg("echo hello");
  std::string partial_outputs;
  std::string output;
  int exit_code = 0;
  TerminationStatus final_status = TERMINATION_STATUS_MAX_ENUM;
  int count = 0;
  base::LaunchOptions options;
  options.start_hidden = true;
  ASSERT_TRUE(GetAppOutputWithExitCodeAndTimeout(
      cl.GetCommandLineString(), true, &output, &exit_code, base::Seconds(2),
      options,
      [&](const Process& process, std::string_view partial_output) {
        ASSERT_TRUE(process.IsValid());
        ++count;
        partial_outputs.append(partial_output);
      },
      &final_status));
  ASSERT_GT(count, 0);
  ASSERT_EQ(partial_outputs, output);
  ASSERT_EQ(partial_outputs, "hello\r\n");
  ASSERT_EQ(final_status, TERMINATION_STATUS_NORMAL_TERMINATION);
}

TEST(LaunchWinTest, GetAppOutputWithExitCodeAndTimeout_TimeoutOutput) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("cmd")));
  cl.AppendArg("/c");
  cl.AppendArg("echo hello && start /wait /min %windir%\\System32\\timeout 5");
  std::string partial_outputs;
  std::string output;
  int exit_code = 0;
  TerminationStatus final_status = TERMINATION_STATUS_MAX_ENUM;
  int count = 0;
  base::LaunchOptions options;
  options.start_hidden = true;
  ASSERT_FALSE(GetAppOutputWithExitCodeAndTimeout(
      cl.GetCommandLineString(), true, &output, &exit_code, base::Seconds(1),
      options,
      [&](const Process& process, std::string_view partial_output) {
        ASSERT_TRUE(process.IsValid());
        ++count;
        partial_outputs.append(partial_output);
      },
      &final_status));
  ASSERT_GT(count, 0);
  ASSERT_EQ(partial_outputs, output);
  ASSERT_EQ(partial_outputs, "hello \r\n");
  ASSERT_EQ(final_status, TERMINATION_STATUS_STILL_RUNNING);
}

TEST(LaunchWinTest, GetAppOutputWithExitCodeAndTimeout_InvalidApplication) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("this-is-an-invalid-application")));
  std::string output;
  int exit_code = 0;
  TerminationStatus final_status = TERMINATION_STATUS_MAX_ENUM;
  int count = 0;
  base::LaunchOptions options;
  options.start_hidden = true;
  ASSERT_FALSE(GetAppOutputWithExitCodeAndTimeout(
      cl.GetCommandLineString(), true, &output, &exit_code, TimeDelta::Max(),
      options,
      [&](const Process& process, std::string_view partial_output) {
        ASSERT_FALSE(process.IsValid());
        ++count;
      },
      &final_status));
  ASSERT_EQ(count, 0);
  ASSERT_EQ(output, "");
  ASSERT_EQ(final_status, TERMINATION_STATUS_LAUNCH_FAILED);
}

TEST(LaunchWinTest, GetAppOutputWithExitCodeAndTimeout_NoOutput) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("cmd")));
  cl.AppendArg("/q");
  cl.AppendArg("/c");
  std::string output;
  int exit_code = 0;
  TerminationStatus final_status = TERMINATION_STATUS_MAX_ENUM;
  int count = 0;
  base::LaunchOptions options;
  options.start_hidden = true;
  ASSERT_TRUE(GetAppOutputWithExitCodeAndTimeout(
      cl.GetCommandLineString(), true, &output, &exit_code, TimeDelta::Max(),
      options,
      [&](const Process& process, std::string_view partial_output) {
        ASSERT_TRUE(process.IsValid());
        ++count;
      },
      &final_status));
  ASSERT_EQ(count, 1);
  ASSERT_EQ(output, "");
  ASSERT_EQ(final_status, TERMINATION_STATUS_NORMAL_TERMINATION);
}

TEST(LaunchWinTest, GetAppOutputWithExitCodeAndTimeout_StreamingOutput) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("powershell")));
  cl.AppendArg("-command");
  cl.AppendArg("'helloworld'*1000");

  std::string partial_outputs;
  std::string output;
  int exit_code = 0;
  TerminationStatus final_status = TERMINATION_STATUS_MAX_ENUM;
  int count = 0;
  base::LaunchOptions options;
  options.start_hidden = true;
  ASSERT_TRUE(GetAppOutputWithExitCodeAndTimeout(
      cl.GetCommandLineString(), true, &output, &exit_code, TimeDelta::Max(),
      options,
      [&](const Process& process, std::string_view partial_output) {
        ASSERT_TRUE(process.IsValid());
        ++count;
        partial_outputs.append(partial_output);
      },
      &final_status));
  ASSERT_GT(count, 0);
  ASSERT_EQ(partial_outputs, output);
  ASSERT_EQ(
      [&](const std::string& word) {
        int word_count = 0;
        size_t position = 0;
        while ((position = partial_outputs.find(word, position)) !=
               std::string::npos) {
          ++word_count;
          position += word.length();
        }
        return word_count;
      }("helloworld"),
      1000);
  ASSERT_EQ(final_status, TERMINATION_STATUS_NORMAL_TERMINATION);
}

}  // namespace base
