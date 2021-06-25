// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_
#define BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/process/launch.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_result.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

namespace base {

// Constants for GTest command-line flags.
extern const char kGTestFilterFlag[];
extern const char kGTestFlagfileFlag[];
extern const char kGTestHelpFlag[];
extern const char kGTestListTestsFlag[];
extern const char kGTestRepeatFlag[];
extern const char kGTestRunDisabledTestsFlag[];
extern const char kGTestOutputFlag[];
extern const char kGTestShuffleFlag[];
extern const char kGTestRandomSeedFlag[];
extern const char kIsolatedScriptRunDisabledTestsFlag[];
extern const char kIsolatedScriptTestFilterFlag[];
extern const char kIsolatedScriptTestRepeatFlag[];

// Interface for use with LaunchTests that abstracts away exact details
// which tests and how are run.
class TestLauncherDelegate {
 public:
  // Called to get names of tests available for running. The delegate
  // must put the result in |output| and return true on success.
  virtual bool GetTests(std::vector<TestIdentifier>* output) = 0;

  // Additional delegate TestResult processing.
  virtual void ProcessTestResults(std::vector<TestResult>& test_results,
                                  TimeDelta elapsed_time) {}

  // Called to get the command line for the specified tests.
  // |output_file_| is populated with the path to the result file, and must
  // be inside |temp_dir|.
  virtual CommandLine GetCommandLine(const std::vector<std::string>& test_names,
                                     const FilePath& temp_dir,
                                     FilePath* output_file) = 0;

  // Invoked when a test process exceeds its runtime, immediately before it is
  // terminated. |command_line| is the command line used to launch the process.
  // NOTE: this method is invoked on the thread the process is launched on.
  virtual void OnTestTimedOut(const CommandLine& cmd_line) {}

  // Returns the delegate specific wrapper for command line.
  // If it is not empty, it is prepended to the final command line.
  virtual std::string GetWrapper() = 0;

  // Returns the delegate specific flags for launch options.
  // The flags are specified in LaunchChildGTestProcessFlags.
  virtual int GetLaunchOptions() = 0;

  // Returns the delegate specific timeout per test.
  virtual TimeDelta GetTimeout() = 0;

  // Returns the delegate specific batch size.
  virtual size_t GetBatchSize() = 0;

  // Returns true if test should run.
  virtual bool ShouldRunTest(const TestIdentifier& test);

 protected:
  virtual ~TestLauncherDelegate();
};

// Launches tests using a TestLauncherDelegate.
class TestLauncher {
 public:
  // Flags controlling behavior of LaunchChildGTestProcess.
  enum LaunchChildGTestProcessFlags {
    // Allows usage of job objects on Windows. Helps properly clean up child
    // processes.
    USE_JOB_OBJECTS = (1 << 0),

    // Allows breakaway from job on Windows. May result in some child processes
    // not being properly terminated after launcher dies if these processes
    // fail to cooperate.
    ALLOW_BREAKAWAY_FROM_JOB = (1 << 1),
  };

  // Enum for subprocess stdio redirect.
  enum StdioRedirect { AUTO, ALWAYS, NEVER };

  struct LaunchOptions {
    LaunchOptions();
    LaunchOptions(const LaunchOptions& other);
    ~LaunchOptions();

    int flags = 0;
    // These mirror values in base::LaunchOptions, see it for details.
#if defined(OS_WIN)
    base::LaunchOptions::Inherit inherit_mode =
        base::LaunchOptions::Inherit::kSpecific;
    base::HandlesToInheritVector handles_to_inherit;
#else
    FileHandleMappingVector fds_to_remap;
#endif
  };

  // Constructor. |parallel_jobs| is the limit of simultaneous parallel test
  // jobs. |retry_limit| is the default limit of retries for bots or all tests.
  TestLauncher(TestLauncherDelegate* launcher_delegate,
               size_t parallel_jobs,
               size_t retry_limit = 1U);
  // virtual to mock in testing.
  virtual ~TestLauncher();

  // Runs the launcher. Must be called at most once.
  // command_line is null by default.
  // if null, uses command line for current process.
  bool Run(CommandLine* command_line = nullptr) WARN_UNUSED_RESULT;

  // Launches a child process (assumed to be gtest-based binary) which runs
  // tests indicated by |test_names|.
  // |task_runner| is used to post results back to the launcher on the main
  // thread. |task_temp_dir| is used for child process files such as user data,
  // result file, and flag_file. |child_temp_dir|, if not empty, specifies a
  // directory (within task_temp_dir) that the child process will use as its
  // process-wide temporary directory.
  // virtual to mock in testing.
  virtual void LaunchChildGTestProcess(
      scoped_refptr<TaskRunner> task_runner,
      const std::vector<std::string>& test_names,
      const FilePath& task_temp_dir,
      const FilePath& child_temp_dir);

  // Called when a test has finished running.
  void OnTestFinished(const TestResult& result);

  // Returns true if child test processes should have dedicated temporary
  // directories.
  static constexpr bool SupportsPerChildTempDirs() {
#if defined(OS_WIN)
    return true;
#else
    // TODO(https://crbug.com/1038857): Enable for macOS, Linux, and Fuchsia.
    return false;
#endif
  }

 private:
  bool Init(CommandLine* command_line) WARN_UNUSED_RESULT;

  // Gets tests from the delegate, and converts to TestInfo objects.
  // Catches and logs uninstantiated parameterized tests.
  // Returns false if delegate fails to return tests.
  bool InitTests();

  // Some of the TestLauncherDelegate implementations don't call into gtest
  // until they've already split into test-specific processes. This results
  // in gtest's native shuffle implementation attempting to shuffle one test.
  // Shuffling the list of tests in the test launcher (before the delegate
  // gets involved) ensures that the entire shard is shuffled.
  bool ShuffleTests(CommandLine* command_line);

  // Move all PRE_ tests prior to the final test in order.
  // Validate tests names. This includes no name conflict between tests
  // without DISABLED_ prefix, and orphaned PRE_ tests.
  // Add all tests and disabled tests names to result tracker.
  // Filter Disabled tests if not flagged to run.
  // Returns false if any violation is found.
  bool ProcessAndValidateTests();

  // Runs all tests in current iteration.
  void RunTests();

  // Print test names that almost match a filter (matches *<filter>*).
  void PrintFuzzyMatchingTestNames();

  // Retry to run tests that failed during RunTests.
  // Returns false if retry still fails or unable to start.
  bool RunRetryTests();

  void CombinePositiveTestFilters(std::vector<std::string> filter_a,
                                  std::vector<std::string> filter_b);

  // Rest counters, retry tests list, and test result tracker.
  void OnTestIterationStart();

#if defined(OS_POSIX)
  void OnShutdownPipeReadable();
#endif

  // Saves test results summary as JSON if requested from command line.
  void MaybeSaveSummaryAsJSON(const std::vector<std::string>& additional_tags);

  // Called when a test iteration is finished.
  void OnTestIterationFinished();

  // Called by the delay timer when no output was made for a while.
  void OnOutputTimeout();

  // Creates and starts a ThreadPoolInstance with |num_parallel_jobs| dedicated
  // to foreground blocking tasks (corresponds to the traits used to launch and
  // wait for child processes). virtual to mock in testing.
  virtual void CreateAndStartThreadPool(int num_parallel_jobs);

  // Callback to receive result of a test.
  // |result_file| is a path to xml file written by child process.
  // It contains information about test and failed
  // EXPECT/ASSERT/DCHECK statements. Test launcher parses that
  // file to get additional information about test run (status,
  // error-messages, stack-traces and file/line for failures).
  // |leaked_items| is the number of files and/or directories remaining in the
  // child process's temporary directory upon its termination.
  void ProcessTestResults(const std::vector<std::string>& test_names,
                          const FilePath& result_file,
                          const std::string& output,
                          TimeDelta elapsed_time,
                          int exit_code,
                          bool was_timeout,
                          int leaked_items);

  std::vector<std::string> CollectTests();

  // Make sure we don't accidentally call the wrong methods e.g. on the worker
  // pool thread.  Should be the first member so that it's destroyed last: when
  // destroying other members, especially the worker pool, we may check the code
  // is running on the correct thread.
  ThreadChecker thread_checker_;

  TestLauncherDelegate* launcher_delegate_;

  // Support for outer sharding, just like gtest does.
  int32_t total_shards_;  // Total number of outer shards, at least one.
  int32_t shard_index_;   // Index of shard the launcher is to run.

  int cycles_;  // Number of remaining test iterations, or -1 for infinite.

  // Test filters (empty means no filter).
  bool has_at_least_one_positive_filter_;
  std::vector<std::string> positive_test_filter_;
  std::vector<std::string> negative_test_filter_;

  // Class to encapsulate gtest information.
  class TestInfo;

  // Tests to use (cached result of TestLauncherDelegate::GetTests).
  std::vector<TestInfo> tests_;

  // Threshold for number of broken tests.
  size_t broken_threshold_;

  // Number of tests started in this iteration.
  size_t test_started_count_;

  // Number of tests finished in this iteration.
  size_t test_finished_count_;

  // Number of tests successfully finished in this iteration.
  size_t test_success_count_;

  // Number of tests either timing out or having an unknown result,
  // likely indicating a more systemic problem if widespread.
  size_t test_broken_count_;

  // How many retries are left.
  size_t retries_left_;

  // Maximum number of retries per iteration.
  size_t retry_limit_;

  // If true will not early exit nor skip retries even if too many tests are
  // broken.
  bool force_run_broken_tests_;

  // Tests to retry in this iteration.
  std::unordered_set<std::string> tests_to_retry_;

  TestResultsTracker results_tracker_;

  // Watchdog timer to make sure we do not go without output for too long.
  DelayTimer watchdog_timer_;

  // Number of jobs to run in parallel.
  size_t parallel_jobs_;

  // Switch to control tests stdio :{auto, always, never}
  StdioRedirect print_test_stdio_;

  // Skip disabled tests unless explicitly requested.
  bool skip_diabled_tests_;

  // Stop test iterations due to failure.
  bool stop_on_failure_;

  // Path to JSON summary result file.
  FilePath summary_path_;

  // Path to trace file.
  FilePath trace_path_;

  // redirect stdio of subprocess
  bool redirect_stdio_;

  // Number of times all tests should be repeated during each iteration.
  // 1 if gtest_repeat is not specified or gtest_break_on_failure is specified.
  // Otherwise it matches gtest_repeat value.
  int repeats_per_iteration_ = 1;

  DISALLOW_COPY_AND_ASSIGN(TestLauncher);
};

// Return the number of parallel jobs to use, or 0U in case of error.
size_t NumParallelJobs(unsigned int cores_per_job);

// Extract part from |full_output| that applies to |result|.
std::string GetTestOutputSnippet(const TestResult& result,
                                 const std::string& full_output);

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_
