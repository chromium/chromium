// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/perf_test_suite.h"

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/perf_log.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/file_utils.h"
#endif

namespace base {

PerfTestSuite::PerfTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

void PerfTestSuite::Initialize() {
  TestSuite::Initialize();

  test::AllowCheckIsTestForTesting();

  // Initialize the perf timer log
  FilePath log_path =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath("log-file");
  if (log_path.empty()) {
    PathService::Get(FILE_EXE, &log_path);
#if BUILDFLAG(IS_ANDROID)
    FilePath tmp_dir;
    PathService::Get(DIR_CACHE, &tmp_dir);
    log_path = tmp_dir.Append(log_path.BaseName());
#elif BUILDFLAG(IS_FUCHSIA)
    log_path =
        FilePath(kPersistedDataDirectoryPath).Append(log_path.BaseName());
#endif
    log_path = log_path.ReplaceExtension(FILE_PATH_LITERAL("log"));
    log_path = log_path.InsertBeforeExtension(FILE_PATH_LITERAL("_perf"));
  }
  ASSERT_TRUE(InitPerfLog(log_path));

  // Raise to high priority to have more precise measurements. Since we don't
  // aim at 1% precision, it is not necessary to run at realtime level.
  if (!debug::BeingDebugged())
    RaiseProcessToHighPriority();
}

void PerfTestSuite::InitializeFromCommandLine(int* argc, char** argv) {
  TestSuite::InitializeFromCommandLine(argc, argv);
  ::benchmark::Initialize(argc, argv);
}

int PerfTestSuite::RunAllTests() {
  const int result = TestSuite::RunAllTests();
  ::benchmark::RunSpecifiedBenchmarks();
  return result;
}

void PerfTestSuite::Shutdown() {
  TestSuite::Shutdown();
  ::benchmark::Shutdown();
  FinalizePerfLog();
}

}  // namespace base
