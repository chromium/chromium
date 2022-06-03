// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/unit_test_launcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"

namespace {

int WriteCompiledInTestsToFileAndLog(const base::FilePath& list_path) {
  if (WriteCompiledInTestsToFile(list_path)) {
    LOG(INFO) << "Wrote compiled tests to file: " << list_path.value();
    return 0;
  }
  LOG(ERROR) << "Failed to write compiled tests to file: " << list_path.value();
  return 1;
}

}  // namespace

namespace base {

int LaunchUnitTests(int argc,
                    char** argv,
                    RunTestSuiteCallback run_test_suite,
                    size_t retry_limit) {
  return LaunchUnitTestsSerially(argc, argv, std::move(run_test_suite));
}

int LaunchUnitTestsSerially(int argc,
                            char** argv,
                            RunTestSuiteCallback run_test_suite) {
  CHECK(CommandLine::InitializedForCurrentProcess() ||
        CommandLine::Init(argc, argv));
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool only_write_tests =
      command_line->HasSwitch(switches::kTestLauncherListTests);
  bool write_and_run_tests =
      command_line->HasSwitch(switches::kWriteCompiledTestsJsonToWritablePath);
  if (only_write_tests || write_and_run_tests) {
    FilePath list_path =
        only_write_tests
            ? (command_line->GetSwitchValuePath(
                  switches::kTestLauncherListTests))
            : mac::GetUserLibraryPath().Append("compiled_tests.json");
    int write_result = WriteCompiledInTestsToFileAndLog(list_path);
    if (only_write_tests) {
      return write_result;
    }
  } else if (command_line->HasSwitch(
                 switches::kTestLauncherPrintWritablePath)) {
    fprintf(stdout, "%s", mac::GetUserLibraryPath().value().c_str());
    fflush(stdout);
    return 0;
  }

  return std::move(run_test_suite).Run();
}

}  // namespace base
