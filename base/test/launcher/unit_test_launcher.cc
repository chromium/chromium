// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/unit_test_launcher.h"

#include <map>
#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

namespace base {

namespace {

// This constant controls how many tests are run in a single batch by default.
const size_t kDefaultTestBatchLimit = 10;

void PrintUsage() {
  fprintf(stdout,
          "Runs tests using the gtest framework, each batch of tests being\n"
          "run in their own process. Supported command-line flags:\n"
          "\n"
          " Common flags:\n"
          "  --gtest_filter=...\n"
          "    Runs a subset of tests (see --gtest_help for more info).\n"
          "\n"
          "  --help\n"
          "    Shows this message.\n"
          "\n"
          "  --gtest_help\n"
          "    Shows the gtest help message.\n"
          "\n"
          "  --test-launcher-jobs=N\n"
          "    Sets the number of parallel test jobs to N.\n"
          "\n"
          "  --single-process-tests\n"
          "    Runs the tests and the launcher in the same process. Useful\n"
          "    for debugging a specific test in a debugger.\n"
          "\n"
          " Other flags:\n"
          "  --test-launcher-filter-file=PATH\n"
          "    Like --gtest_filter, but read the test filter from PATH.\n"
          "    Supports multiple filter paths separated by ';'.\n"
          "    One pattern per line; lines starting with '-' are exclusions.\n"
          "    See also //testing/buildbot/filters/README.md file.\n"
          "\n"
          "  --test-launcher-batch-limit=N\n"
          "    Sets the limit of test batch to run in a single process to N.\n"
          "\n"
          "  --test-launcher-debug-launcher\n"
          "    Disables autodetection of debuggers and similar tools,\n"
          "    making it possible to use them to debug launcher itself.\n"
          "\n"
          "  --test-launcher-retry-limit=N\n"
          "    Sets the limit of test retries on failures to N.\n"
          "\n"
          "  --test-launcher-summary-output=PATH\n"
          "    Saves a JSON machine-readable summary of the run.\n"
          "\n"
          "  --test-launcher-print-test-stdio=auto|always|never\n"
          "    Controls when full test output is printed.\n"
          "    auto means to print it when the test failed.\n"
          "\n"
          "  --test-launcher-test-part-results-limit=N\n"
          "    Sets the limit of failed EXPECT/ASSERT entries in the xml and\n"
          "    JSON outputs per test to N (default N=10). Negative value \n"
          "    will disable this limit.\n"
          "\n"
          "  --test-launcher-total-shards=N\n"
          "    Sets the total number of shards to N.\n"
          "\n"
          "  --test-launcher-shard-index=N\n"
          "    Sets the shard index to run to N (from 0 to TOTAL - 1).\n"
          "\n"
          "  --dont-use-job-objects\n"
          "    Avoids using job objects in Windows.\n");
  fflush(stdout);
}

bool GetSwitchValueAsInt(const std::string& switch_name, int* result) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switch_name))
    return true;

  std::string switch_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name);
  if (!StringToInt(switch_value, result) || *result < 0) {
    LOG(ERROR) << "Invalid value for " << switch_name << ": " << switch_value;
    return false;
  }

  return true;
}

int LaunchUnitTestsInternal(RunTestSuiteCallback run_test_suite,
                            size_t parallel_jobs,
                            int default_batch_limit,
                            size_t retry_limit,
                            bool use_job_objects,
                            OnceClosure gtest_init) {
#if defined(OS_ANDROID)
  // We can't easily fork on Android, just run the test suite directly.
  return std::move(run_test_suite).Run();
#else
  bool force_single_process = false;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherDebugLauncher)) {
    fprintf(stdout, "Forcing test launcher debugging mode.\n");
    fflush(stdout);
  } else {
    if (base::debug::BeingDebugged()) {
      fprintf(stdout,
              "Debugger detected, switching to single process mode.\n"
              "Pass --test-launcher-debug-launcher to debug the launcher "
              "itself.\n");
      fflush(stdout);
      force_single_process = true;
    }
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(kGTestHelpFlag) ||
      CommandLine::ForCurrentProcess()->HasSwitch(kGTestListTestsFlag) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcessTests) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestChildProcess) ||
      force_single_process) {
    return std::move(run_test_suite).Run();
  }
#endif

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kHelpFlag)) {
    PrintUsage();
    return 0;
  }

  TimeTicks start_time(TimeTicks::Now());

  std::move(gtest_init).Run();
  TestTimeouts::Initialize();

  int batch_limit = default_batch_limit;
  if (!GetSwitchValueAsInt(switches::kTestLauncherBatchLimit, &batch_limit))
    return 1;

  fprintf(stdout,
          "IMPORTANT DEBUGGING NOTE: batches of tests are run inside their\n"
          "own process. For debugging a test inside a debugger, use the\n"
          "--gtest_filter=<your_test_name> flag along with\n"
          "--single-process-tests.\n");
  fflush(stdout);

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);
#if defined(OS_POSIX)
  FileDescriptorWatcher file_descriptor_watcher(executor.task_runner());
#endif
  use_job_objects =
      use_job_objects &&
      !CommandLine::ForCurrentProcess()->HasSwitch(kDontUseJobObjectFlag);

  DefaultUnitTestPlatformDelegate platform_delegate;
  UnitTestLauncherDelegate delegate(&platform_delegate, batch_limit,
                                    use_job_objects);
  TestLauncher launcher(&delegate, parallel_jobs, retry_limit);
  bool success = launcher.Run();

  fprintf(stdout, "Tests took %" PRId64 " seconds.\n",
          (TimeTicks::Now() - start_time).InSeconds());
  fflush(stdout);

  return (success ? 0 : 1);
}

void InitGoogleTestChar(int* argc, char** argv) {
  testing::InitGoogleTest(argc, argv);
}

#if defined(OS_WIN)
void InitGoogleTestWChar(int* argc, wchar_t** argv) {
  testing::InitGoogleTest(argc, argv);
}
#endif  // defined(OS_WIN)

}  // namespace

// Flag to avoid using job objects
const char kDontUseJobObjectFlag[] = "dont-use-job-objects";

int LaunchUnitTests(int argc,
                    char** argv,
                    RunTestSuiteCallback run_test_suite,
                    size_t retry_limit) {
  CommandLine::Init(argc, argv);
  size_t parallel_jobs = NumParallelJobs();
  if (parallel_jobs == 0U) {
    return 1;
  }
  return LaunchUnitTestsInternal(std::move(run_test_suite), parallel_jobs,
                                 kDefaultTestBatchLimit, retry_limit, true,
                                 BindOnce(&InitGoogleTestChar, &argc, argv));
}

int LaunchUnitTestsSerially(int argc,
                            char** argv,
                            RunTestSuiteCallback run_test_suite) {
  CommandLine::Init(argc, argv);
  return LaunchUnitTestsInternal(std::move(run_test_suite), 1U,
                                 kDefaultTestBatchLimit, 1U, true,
                                 BindOnce(&InitGoogleTestChar, &argc, argv));
}

int LaunchUnitTestsWithOptions(int argc,
                               char** argv,
                               size_t parallel_jobs,
                               int default_batch_limit,
                               bool use_job_objects,
                               RunTestSuiteCallback run_test_suite) {
  CommandLine::Init(argc, argv);
  return LaunchUnitTestsInternal(std::move(run_test_suite), parallel_jobs,
                                 default_batch_limit, 1U, use_job_objects,
                                 BindOnce(&InitGoogleTestChar, &argc, argv));
}

#if defined(OS_WIN)
int LaunchUnitTests(int argc,
                    wchar_t** argv,
                    bool use_job_objects,
                    RunTestSuiteCallback run_test_suite) {
  // Windows CommandLine::Init ignores argv anyway.
  CommandLine::Init(argc, NULL);
  size_t parallel_jobs = NumParallelJobs();
  if (parallel_jobs == 0U) {
    return 1;
  }
  return LaunchUnitTestsInternal(std::move(run_test_suite), parallel_jobs,
                                 kDefaultTestBatchLimit, 1U, use_job_objects,
                                 BindOnce(&InitGoogleTestWChar, &argc, argv));
}
#endif  // defined(OS_WIN)

DefaultUnitTestPlatformDelegate::DefaultUnitTestPlatformDelegate() = default;

bool DefaultUnitTestPlatformDelegate::GetTests(
    std::vector<TestIdentifier>* output) {
  *output = GetCompiledInTests();
  return true;
}

bool DefaultUnitTestPlatformDelegate::CreateResultsFile(
    const base::FilePath& temp_dir,
    base::FilePath* path) {
  if (!CreateTemporaryDirInDir(temp_dir, FilePath::StringType(), path))
    return false;
  *path = path->AppendASCII("test_results.xml");
  return true;
}

bool DefaultUnitTestPlatformDelegate::CreateTemporaryFile(
    const base::FilePath& temp_dir,
    base::FilePath* path) {
  if (temp_dir.empty())
    return false;
  return CreateTemporaryFileInDir(temp_dir, path);
}

CommandLine DefaultUnitTestPlatformDelegate::GetCommandLineForChildGTestProcess(
    const std::vector<std::string>& test_names,
    const base::FilePath& output_file,
    const base::FilePath& flag_file) {
  CommandLine new_cmd_line(*CommandLine::ForCurrentProcess());

  CHECK(base::PathExists(flag_file));

  std::string long_flags(
      StrCat({"--", kGTestFilterFlag, "=", JoinString(test_names, ":")}));
  CHECK_EQ(static_cast<int>(long_flags.size()),
           WriteFile(flag_file, long_flags.data(),
                     static_cast<int>(long_flags.size())));

  new_cmd_line.AppendSwitchPath(switches::kTestLauncherOutput, output_file);
  new_cmd_line.AppendSwitchPath(kGTestFlagfileFlag, flag_file);
  new_cmd_line.AppendSwitch(switches::kSingleProcessTests);

  return new_cmd_line;
}

std::string DefaultUnitTestPlatformDelegate::GetWrapperForChildGTestProcess() {
  return std::string();
}

UnitTestLauncherDelegate::UnitTestLauncherDelegate(
    UnitTestPlatformDelegate* platform_delegate,
    size_t batch_limit,
    bool use_job_objects)
    : platform_delegate_(platform_delegate),
      batch_limit_(batch_limit),
      use_job_objects_(use_job_objects) {}

UnitTestLauncherDelegate::~UnitTestLauncherDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool UnitTestLauncherDelegate::GetTests(std::vector<TestIdentifier>* output) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return platform_delegate_->GetTests(output);
}

CommandLine UnitTestLauncherDelegate::GetCommandLine(
    const std::vector<std::string>& test_names,
    const FilePath& temp_dir,
    FilePath* output_file) {
  CHECK(!test_names.empty());

  // Create a dedicated temporary directory to store the xml result data
  // per run to ensure clean state and make it possible to launch multiple
  // processes in parallel.
  CHECK(platform_delegate_->CreateResultsFile(temp_dir, output_file));
  FilePath flag_file;
  platform_delegate_->CreateTemporaryFile(temp_dir, &flag_file);

  return CommandLine(platform_delegate_->GetCommandLineForChildGTestProcess(
      test_names, *output_file, flag_file));
}

std::string UnitTestLauncherDelegate::GetWrapper() {
  return platform_delegate_->GetWrapperForChildGTestProcess();
}

int UnitTestLauncherDelegate::GetLaunchOptions() {
  return use_job_objects_ ? TestLauncher::USE_JOB_OBJECTS : 0;
}

TimeDelta UnitTestLauncherDelegate::GetTimeout() {
  return TestTimeouts::test_launcher_timeout();
}

size_t UnitTestLauncherDelegate::GetBatchSize() {
  return batch_limit_;
}

}  // namespace base
