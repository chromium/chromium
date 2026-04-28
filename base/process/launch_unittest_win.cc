// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <io.h>
#include <stdio.h>
#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

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

MULTIPROCESS_TEST_MAIN(RouteStdioToConsoleChild) {
  // Detach from the parent's console and invalidate stdout/stderr so that
  // RouteStdioToConsole doesn't return early.
  FreeConsole();
  fclose(stdout);
  fclose(stderr);

  // RouteStdioToConsole will allocate a new console and reopen stdout/stderr
  // on CONOUT$.
  base::RouteStdioToConsole(/*create_console_if_not_found=*/true);

  // stdout may still be invalid if AllocConsole() failed (e.g. in a
  // sandboxed CI environment).
  if (_fileno(stdout) < 0) {
    return 1;  // No console available.
  }

  HANDLE stdout_handle =
      reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)));
  if (stdout_handle == INVALID_HANDLE_VALUE) {
    return 1;  // No console available.
  }

  // GetConsoleMode() requires GENERIC_READ on the handle. If
  // RouteStdioToConsole opens CONOUT$ with "w" (write-only) instead of "w+"
  // (read+write), this call will fail.
  DWORD mode;
  if (!GetConsoleMode(stdout_handle, &mode)) {
    return 2;  // GetConsoleMode failed — handle lacks read access.
  }

  return 0;
}

TEST(LaunchWinTest, RouteStdioToConsoleEnablesGetConsoleMode) {
  // Spawn a child process without inheriting the parent's console handles.
  // The child calls RouteStdioToConsole and verifies that GetConsoleMode()
  // succeeds on the resulting stdout handle. This is a regression test:
  // opening CONOUT$ with "w" (write-only) causes GetConsoleMode() to fail
  // because it requires GENERIC_READ access.
  base::LaunchOptions options;
  options.start_hidden = true;

  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::Process process = base::SpawnMultiProcessTestChild(
      "RouteStdioToConsoleChild", command_line, options);
  ASSERT_TRUE(process.IsValid());

  int exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_max_timeout(), &exit_code));
  if (exit_code == 1) {
    GTEST_SKIP() << "No console available in this environment.";
  }
  EXPECT_EQ(0, exit_code)
      << "GetConsoleMode() failed — CONOUT$ likely opened without read access";
}

}  // namespace base
