// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#include <stdio.h>

#include <algorithm>
#include <map>
#include <random>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/hash.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_launcher_tracer.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/job.h>
#include "base/atomic_sequence_num.h"
#include "base/base_paths_fuchsia.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#endif

namespace base {

// See https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ .
using ::operator<<;

// The environment variable name for the total number of test shards.
const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard index.
const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

namespace {

// Global tag for test runs where the results are incomplete or unreliable
// for any reason, e.g. early exit because of too many broken tests.
const char kUnreliableResultsTag[] = "UNRELIABLE_RESULTS";

// Maximum time of no output after which we print list of processes still
// running. This deliberately doesn't use TestTimeouts (which is otherwise
// a recommended solution), because they can be increased. This would defeat
// the purpose of this timeout, which is 1) to avoid buildbot "no output for
// X seconds" timeout killing the process 2) help communicate status of
// the test launcher to people looking at the output (no output for a long
// time is mysterious and gives no info about what is happening) 3) help
// debugging in case the process hangs anyway.
constexpr TimeDelta kOutputTimeout = TimeDelta::FromSeconds(15);

// Limit of output snippet lines when printing to stdout.
// Avoids flooding the logs with amount of output that gums up
// the infrastructure.
const size_t kOutputSnippetLinesLimit = 5000;

// Limit of output snippet size. Exceeding this limit
// results in truncating the output and failing the test.
const size_t kOutputSnippetBytesLimit = 300 * 1024;

// Limit of seed values for gtest shuffling. Arbitrary, but based on
// gtest's similarly arbitrary choice.
const uint32_t kRandomSeedUpperBound = 100000;

// Set of live launch test processes with corresponding lock (it is allowed
// for callers to launch processes on different threads).
Lock* GetLiveProcessesLock() {
  static auto* lock = new Lock;
  return lock;
}

std::map<ProcessHandle, CommandLine>* GetLiveProcesses() {
  static auto* map = new std::map<ProcessHandle, CommandLine>;
  return map;
}

// Performance trace generator.
TestLauncherTracer* GetTestLauncherTracer() {
  static auto* tracer = new TestLauncherTracer;
  return tracer;
}

// Creates and starts a TaskScheduler with |num_parallel_jobs| dedicated to
// foreground blocking tasks (corresponds to the traits used to launch and wait
// for child processes).
void CreateAndStartTaskScheduler(int num_parallel_jobs) {
  // These values are taken from TaskScheduler::StartWithDefaultParams(), which
  // is not used directly to allow a custom number of threads in the foreground
  // blocking pool.
  constexpr int kMaxBackgroundThreads = 1;
  constexpr int kMaxBackgroundBlockingThreads = 2;
  const int max_foreground_threads =
      std::max(1, base::SysInfo::NumberOfProcessors());
  constexpr base::TimeDelta kSuggestedReclaimTime =
      base::TimeDelta::FromSeconds(30);
  base::TaskScheduler::Create("TestLauncher");
  base::TaskScheduler::GetInstance()->Start(
      {{kMaxBackgroundThreads, kSuggestedReclaimTime},
       {kMaxBackgroundBlockingThreads, kSuggestedReclaimTime},
       {max_foreground_threads, kSuggestedReclaimTime},
       {num_parallel_jobs, kSuggestedReclaimTime}});
}

#if defined(OS_POSIX)
// Self-pipe that makes it possible to do complex shutdown handling
// outside of the signal handler.
int g_shutdown_pipe[2] = { -1, -1 };

void ShutdownPipeSignalHandler(int signal) {
  HANDLE_EINTR(write(g_shutdown_pipe[1], "q", 1));
}

void KillSpawnedTestProcesses() {
  // Keep the lock until exiting the process to prevent further processes
  // from being spawned.
  AutoLock lock(*GetLiveProcessesLock());

  fprintf(stdout, "Sending SIGTERM to %" PRIuS " child processes... ",
          GetLiveProcesses()->size());
  fflush(stdout);

  for (const auto& pair : *GetLiveProcesses()) {
    // Send the signal to entire process group.
    kill((-1) * (pair.first), SIGTERM);
  }

  fprintf(stdout,
          "done.\nGiving processes a chance to terminate cleanly... ");
  fflush(stdout);

  PlatformThread::Sleep(TimeDelta::FromMilliseconds(500));

  fprintf(stdout, "done.\n");
  fflush(stdout);

  fprintf(stdout, "Sending SIGKILL to %" PRIuS " child processes... ",
          GetLiveProcesses()->size());
  fflush(stdout);

  for (const auto& pair : *GetLiveProcesses()) {
    // Send the signal to entire process group.
    kill((-1) * (pair.first), SIGKILL);
  }

  fprintf(stdout, "done.\n");
  fflush(stdout);
}
#endif  // defined(OS_POSIX)

// Parses the environment variable var as an Int32.  If it is unset, returns
// true.  If it is set, unsets it then converts it to Int32 before
// returning it in |result|.  Returns true on success.
bool TakeInt32FromEnvironment(const char* const var, int32_t* result) {
  std::unique_ptr<Environment> env(Environment::Create());
  std::string str_val;

  if (!env->GetVar(var, &str_val))
    return true;

  if (!env->UnSetVar(var)) {
    LOG(ERROR) << "Invalid environment: we could not unset " << var << ".\n";
    return false;
  }

  if (!StringToInt(str_val, result)) {
    LOG(ERROR) << "Invalid environment: " << var << " is not an integer.\n";
    return false;
  }

  return true;
}

// Unsets the environment variable |name| and returns true on success.
// Also returns true if the variable just doesn't exist.
bool UnsetEnvironmentVariableIfExists(const std::string& name) {
  std::unique_ptr<Environment> env(Environment::Create());
  std::string str_val;
  if (!env->GetVar(name, &str_val))
    return true;
  return env->UnSetVar(name);
}

// Returns true if bot mode has been requested, i.e. defaults optimized
// for continuous integration bots. This way developers don't have to remember
// special command-line flags.
bool BotModeEnabled() {
  std::unique_ptr<Environment> env(Environment::Create());
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTestLauncherBotMode) ||
      env->HasVar("CHROMIUM_TEST_LAUNCHER_BOT_MODE");
}

// Returns command line command line after gtest-specific processing
// and applying |wrapper|.
CommandLine PrepareCommandLineForGTest(const CommandLine& command_line,
                                       const std::string& wrapper) {
  CommandLine new_command_line(command_line.GetProgram());
  CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Handled by the launcher process.
  switches.erase(kGTestRepeatFlag);
  switches.erase(kIsolatedScriptTestRepeatFlag);
  switches.erase(kGTestShuffleFlag);
  switches.erase(kGTestRandomSeedFlag);

  // Don't try to write the final XML report in child processes.
  switches.erase(kGTestOutputFlag);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  // Prepend wrapper after last CommandLine quasi-copy operation. CommandLine
  // does not really support removing switches well, and trying to do that
  // on a CommandLine with a wrapper is known to break.
  // TODO(phajdan.jr): Give it a try to support CommandLine removing switches.
#if defined(OS_WIN)
  new_command_line.PrependWrapper(ASCIIToUTF16(wrapper));
#else
  new_command_line.PrependWrapper(wrapper);
#endif

  return new_command_line;
}

// Launches a child process using |command_line|. If the child process is still
// running after |timeout|, it is terminated and |*was_timeout| is set to true.
// Returns exit code of the process.
int LaunchChildTestProcessWithOptions(const CommandLine& command_line,
                                      const LaunchOptions& options,
                                      int flags,
                                      TimeDelta timeout,
                                      ProcessLifetimeObserver* observer,
                                      bool* was_timeout) {
  TimeTicks start_time(TimeTicks::Now());

#if defined(OS_POSIX)
  // Make sure an option we rely on is present - see LaunchChildGTestProcess.
  DCHECK(options.new_process_group);
#endif

  LaunchOptions new_options(options);

#if defined(OS_WIN)
  DCHECK(!new_options.job_handle);

  win::ScopedHandle job_handle;
  if (flags & TestLauncher::USE_JOB_OBJECTS) {
    job_handle.Set(CreateJobObject(NULL, NULL));
    if (!job_handle.IsValid()) {
      LOG(ERROR) << "Could not create JobObject.";
      return -1;
    }

    DWORD job_flags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    // Allow break-away from job since sandbox and few other places rely on it
    // on Windows versions prior to Windows 8 (which supports nested jobs).
    if (win::GetVersion() < win::VERSION_WIN8 &&
        flags & TestLauncher::ALLOW_BREAKAWAY_FROM_JOB) {
      job_flags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    }

    if (!SetJobObjectLimitFlags(job_handle.Get(), job_flags)) {
      LOG(ERROR) << "Could not SetJobObjectLimitFlags.";
      return -1;
    }

    new_options.job_handle = job_handle.Get();
  }
#elif defined(OS_FUCHSIA)
  DCHECK(!new_options.job_handle);

  // Set the clone policy, deliberately omitting FDIO_SPAWN_CLONE_NAMESPACE so
  // that we can install a different /data.
  new_options.spawn_flags = FDIO_SPAWN_CLONE_STDIO | FDIO_SPAWN_CLONE_JOB;
  new_options.paths_to_clone.push_back(base::FilePath("/config/ssl"));
  new_options.paths_to_clone.push_back(base::FilePath("/dev/null"));
  new_options.paths_to_clone.push_back(base::FilePath("/dev/zero"));
  new_options.paths_to_clone.push_back(base::FilePath("/pkg"));
  new_options.paths_to_clone.push_back(base::FilePath("/svc"));
  new_options.paths_to_clone.push_back(base::FilePath("/tmp"));

  zx::job job_handle;
  zx_status_t result = zx::job::create(*GetDefaultJob(), 0, &job_handle);
  ZX_CHECK(ZX_OK == result, result) << "zx_job_create";
  new_options.job_handle = job_handle.get();

  // Give this test its own isolated /data directory by creating a new temporary
  // subdirectory under data (/data/test-$PID) and binding that to /data on the
  // child process.
  base::FilePath data_path("/data");
  CHECK(base::PathExists(data_path));

  // Create the test subdirectory with a name that is unique to the child test
  // process (qualified by parent PID and an autoincrementing test process
  // index).
  static base::AtomicSequenceNumber child_launch_index;
  base::FilePath nested_data_path = data_path.AppendASCII(
      base::StringPrintf("test-%" PRIuS "-%d", base::Process::Current().Pid(),
                         child_launch_index.GetNext()));
  CHECK(!base::DirectoryExists(nested_data_path));
  CHECK(base::CreateDirectory(nested_data_path));
  DCHECK(base::DirectoryExists(nested_data_path));

  // Bind the new test subdirectory to /data in the child process' namespace.
  new_options.paths_to_transfer.push_back(
      {data_path, base::fuchsia::GetHandleFromFile(
                      base::File(nested_data_path,
                                 base::File::FLAG_OPEN | base::File::FLAG_READ |
                                     base::File::FLAG_DELETE_ON_CLOSE))
                      .release()});

  // The test launcher can use a shared data directory for providing tests with
  // files deployed at runtime. The files are located under the directory
  // "/data/shared". They will be mounted at "/test-shared" under the child
  // process' namespace.
  const base::FilePath kSharedDataSourcePath("/data/shared");
  const base::FilePath kSharedDataTargetPath("/test-shared");
  if (base::PathExists(kSharedDataSourcePath)) {
    zx::handle shared_directory_handle = base::fuchsia::GetHandleFromFile(
        base::File(kSharedDataSourcePath,
                   base::File::FLAG_OPEN | base::File::FLAG_READ |
                       base::File::FLAG_DELETE_ON_CLOSE));
    new_options.paths_to_transfer.push_back(
        {kSharedDataTargetPath, shared_directory_handle.release()});
  }

#endif  // defined(OS_FUCHSIA)

#if defined(OS_LINUX)
  // To prevent accidental privilege sharing to an untrusted child, processes
  // are started with PR_SET_NO_NEW_PRIVS. Do not set that here, since this
  // new child will be privileged and trusted.
  new_options.allow_new_privs = true;
#endif

  Process process;

  {
    // Note how we grab the lock before the process possibly gets created.
    // This ensures that when the lock is held, ALL the processes are registered
    // in the set.
    AutoLock lock(*GetLiveProcessesLock());

#if defined(OS_WIN)
    // Allow the handle used to capture stdio and stdout to be inherited by the
    // child. Note that this is done under GetLiveProcessesLock() to ensure that
    // only the desired child receives the handle.
    if (new_options.stdout_handle) {
      ::SetHandleInformation(new_options.stdout_handle, HANDLE_FLAG_INHERIT,
                             HANDLE_FLAG_INHERIT);
    }
#endif

    process = LaunchProcess(command_line, new_options);

#if defined(OS_WIN)
    // Revoke inheritance so that the handle isn't leaked into other children.
    // Note that this is done under GetLiveProcessesLock() to ensure that only
    // the desired child receives the handle.
    if (new_options.stdout_handle)
      ::SetHandleInformation(new_options.stdout_handle, HANDLE_FLAG_INHERIT, 0);
#endif

    if (!process.IsValid())
      return -1;

    // TODO(rvargas) crbug.com/417532: Don't store process handles.
    GetLiveProcesses()->insert(std::make_pair(process.Handle(), command_line));
  }

  if (observer)
    observer->OnLaunched(process.Handle(), process.Pid());

  int exit_code = 0;
  bool did_exit = false;

  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    did_exit = process.WaitForExitWithTimeout(timeout, &exit_code);
  }

  if (!did_exit) {
    if (observer)
      observer->OnTimedOut(command_line);

    *was_timeout = true;
    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    {
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
      // Ensure that the process terminates.
      process.Terminate(-1, true);
    }
  }

  {
    // Note how we grab the log before issuing a possibly broad process kill.
    // Other code parts that grab the log kill processes, so avoid trying
    // to do that twice and trigger all kinds of log messages.
    AutoLock lock(*GetLiveProcessesLock());

#if defined(OS_FUCHSIA)
    zx_status_t status = job_handle.kill();
    ZX_CHECK(status == ZX_OK, status);

    // The child process' data dir should have been deleted automatically,
    // thanks to the DELETE_ON_CLOSE flag.
    DCHECK(!base::DirectoryExists(nested_data_path));
#elif defined(OS_POSIX)
    if (exit_code != 0) {
      // On POSIX, in case the test does not exit cleanly, either due to a crash
      // or due to it timing out, we need to clean up any child processes that
      // it might have created. On Windows, child processes are automatically
      // cleaned up using JobObjects.
      KillProcessGroup(process.Handle());
    }
#endif

    GetLiveProcesses()->erase(process.Handle());
  }

  GetTestLauncherTracer()->RecordProcessExecution(
      start_time, TimeTicks::Now() - start_time);

  return exit_code;
}

void DoLaunchChildTestProcess(
    const CommandLine& command_line,
    TimeDelta timeout,
    const TestLauncher::LaunchOptions& test_launch_options,
    bool redirect_stdio,
    SingleThreadTaskRunner* task_runner,
    std::unique_ptr<ProcessLifetimeObserver> observer) {
  TimeTicks start_time = TimeTicks::Now();

  ScopedFILE output_file;
  FilePath output_filename;
  if (redirect_stdio) {
    FILE* raw_output_file = CreateAndOpenTemporaryFile(&output_filename);
    output_file.reset(raw_output_file);
    CHECK(output_file);
  }

  LaunchOptions options;
#if defined(OS_WIN)

  options.inherit_mode = test_launch_options.inherit_mode;
  options.handles_to_inherit = test_launch_options.handles_to_inherit;
  if (redirect_stdio) {
    HANDLE handle =
        reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(output_file.get())));
    CHECK_NE(INVALID_HANDLE_VALUE, handle);
    options.stdin_handle = INVALID_HANDLE_VALUE;
    options.stdout_handle = handle;
    options.stderr_handle = handle;
    // See LaunchOptions.stdout_handle comments for why this compares against
    // FILE_TYPE_CHAR.
    if (options.inherit_mode == base::LaunchOptions::Inherit::kSpecific &&
        GetFileType(handle) != FILE_TYPE_CHAR) {
      options.handles_to_inherit.push_back(handle);
    }
  }

#else  // if !defined(OS_WIN)

  options.fds_to_remap = test_launch_options.fds_to_remap;
  if (redirect_stdio) {
    int output_file_fd = fileno(output_file.get());
    CHECK_LE(0, output_file_fd);
    options.fds_to_remap.push_back(
        std::make_pair(output_file_fd, STDOUT_FILENO));
    options.fds_to_remap.push_back(
        std::make_pair(output_file_fd, STDERR_FILENO));
  }

#if !defined(OS_FUCHSIA)
  options.new_process_group = true;
#endif
#if defined(OS_LINUX)
  options.kill_on_parent_death = true;
#endif

#endif  // !defined(OS_WIN)

  bool was_timeout = false;
  int exit_code = LaunchChildTestProcessWithOptions(
      command_line, options, test_launch_options.flags, timeout, observer.get(),
      &was_timeout);

  std::string output_file_contents;
  if (redirect_stdio) {
    fflush(output_file.get());
    output_file.reset();
    // Reading the file can sometimes fail when the process was killed midflight
    // (e.g. on test suite timeout): https://crbug.com/826408. Attempt to read
    // the output file anyways, but do not crash on failure in this case.
    CHECK(ReadFileToString(output_filename, &output_file_contents) ||
          exit_code != 0);

    if (!DeleteFile(output_filename, false)) {
      // This needs to be non-fatal at least for Windows.
      LOG(WARNING) << "Failed to delete " << output_filename.AsUTF8Unsafe();
    }
  }

  // Invoke OnCompleted on the thread it was originating from, not on a worker
  // pool thread.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(&ProcessLifetimeObserver::OnCompleted, std::move(observer),
               exit_code, TimeTicks::Now() - start_time, was_timeout,
               output_file_contents));
}

std::vector<std::string> ExtractTestsFromFilter(const std::string& filter,
                                                bool double_colon_supported) {
  std::vector<std::string> tests;
  if (double_colon_supported) {
    tests =
        SplitString(filter, "::", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  if (tests.size() <= 1) {
    tests =
        SplitString(filter, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  return tests;
}

}  // namespace

const char kGTestBreakOnFailure[] = "gtest_break_on_failure";
const char kGTestFilterFlag[] = "gtest_filter";
const char kGTestFlagfileFlag[] = "gtest_flagfile";
const char kGTestHelpFlag[]   = "gtest_help";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";
const char kGTestShuffleFlag[] = "gtest_shuffle";
const char kGTestRandomSeedFlag[] = "gtest_random_seed";
const char kIsolatedScriptRunDisabledTestsFlag[] =
    "isolated-script-test-also-run-disabled-tests";
const char kIsolatedScriptTestFilterFlag[] = "isolated-script-test-filter";
const char kIsolatedScriptTestRepeatFlag[] = "isolated-script-test-repeat";

TestLauncherDelegate::~TestLauncherDelegate() = default;

TestLauncher::LaunchOptions::LaunchOptions() = default;
TestLauncher::LaunchOptions::LaunchOptions(const LaunchOptions& other) =
    default;
TestLauncher::LaunchOptions::~LaunchOptions() = default;

TestLauncher::TestLauncher(TestLauncherDelegate* launcher_delegate,
                           size_t parallel_jobs)
    : launcher_delegate_(launcher_delegate),
      total_shards_(1),
      shard_index_(0),
      cycles_(1),
      test_found_count_(0),
      test_started_count_(0),
      test_finished_count_(0),
      test_success_count_(0),
      test_broken_count_(0),
      retry_count_(0),
      retry_limit_(0),
      force_run_broken_tests_(false),
      run_result_(true),
      shuffle_(false),
      shuffle_seed_(0),
      watchdog_timer_(FROM_HERE,
                      kOutputTimeout,
                      this,
                      &TestLauncher::OnOutputTimeout),
      parallel_jobs_(parallel_jobs) {}

TestLauncher::~TestLauncher() {
  if (base::TaskScheduler::GetInstance()) {
    base::TaskScheduler::GetInstance()->Shutdown();
  }
}

bool TestLauncher::Run() {
  if (!Init())
    return false;

  // Value of |cycles_| changes after each iteration. Keep track of the
  // original value.
  int requested_cycles = cycles_;

#if defined(OS_POSIX)
  CHECK_EQ(0, pipe(g_shutdown_pipe));

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = &ShutdownPipeSignalHandler;

  CHECK_EQ(0, sigaction(SIGINT, &action, nullptr));
  CHECK_EQ(0, sigaction(SIGQUIT, &action, nullptr));
  CHECK_EQ(0, sigaction(SIGTERM, &action, nullptr));

  auto controller = base::FileDescriptorWatcher::WatchReadable(
      g_shutdown_pipe[0],
      base::Bind(&TestLauncher::OnShutdownPipeReadable, Unretained(this)));
#endif  // defined(OS_POSIX)

  // Start the watchdog timer.
  watchdog_timer_.Reset();

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&TestLauncher::RunTestIteration, Unretained(this)));

  RunLoop().Run();

  if (requested_cycles != 1)
    results_tracker_.PrintSummaryOfAllIterations();

  MaybeSaveSummaryAsJSON(std::vector<std::string>());

  return run_result_;
}

void TestLauncher::LaunchChildGTestProcess(
    const CommandLine& command_line,
    const std::string& wrapper,
    TimeDelta timeout,
    const LaunchOptions& options,
    std::unique_ptr<ProcessLifetimeObserver> observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Record the exact command line used to launch the child.
  CommandLine new_command_line(
      PrepareCommandLineForGTest(command_line, wrapper));

  // When running in parallel mode we need to redirect stdio to avoid mixed-up
  // output. We also always redirect on the bots to get the test output into
  // JSON summary.
  bool redirect_stdio = (parallel_jobs_ > 1) || BotModeEnabled();

  PostTaskWithTraits(
      FROM_HERE, {MayBlock(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
      BindOnce(&DoLaunchChildTestProcess, new_command_line, timeout, options,
               redirect_stdio, RetainedRef(ThreadTaskRunnerHandle::Get()),
               std::move(observer)));
}

void TestLauncher::OnTestFinished(const TestResult& original_result) {
  ++test_finished_count_;

  TestResult result(original_result);

  if (result.output_snippet.length() > kOutputSnippetBytesLimit) {
    if (result.status == TestResult::TEST_SUCCESS)
      result.status = TestResult::TEST_EXCESSIVE_OUTPUT;

    // Keep the top and bottom of the log and truncate the middle part.
    result.output_snippet =
        result.output_snippet.substr(0, kOutputSnippetBytesLimit / 2) + "\n" +
        StringPrintf("<truncated (%" PRIuS " bytes)>\n",
                     result.output_snippet.length()) +
        result.output_snippet.substr(result.output_snippet.length() -
                                     kOutputSnippetBytesLimit / 2) +
        "\n";
  }

  bool print_snippet = false;
  std::string print_test_stdio("auto");
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherPrintTestStdio)) {
    print_test_stdio = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kTestLauncherPrintTestStdio);
  }
  if (print_test_stdio == "auto") {
    print_snippet = (result.status != TestResult::TEST_SUCCESS);
  } else if (print_test_stdio == "always") {
    print_snippet = true;
  } else if (print_test_stdio == "never") {
    print_snippet = false;
  } else {
    LOG(WARNING) << "Invalid value of " << switches::kTestLauncherPrintTestStdio
                 << ": " << print_test_stdio;
  }
  if (print_snippet) {
    std::vector<base::StringPiece> snippet_lines =
        SplitStringPiece(result.output_snippet, "\n", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_ALL);
    if (snippet_lines.size() > kOutputSnippetLinesLimit) {
      size_t truncated_size = snippet_lines.size() - kOutputSnippetLinesLimit;
      snippet_lines.erase(
          snippet_lines.begin(),
          snippet_lines.begin() + truncated_size);
      snippet_lines.insert(snippet_lines.begin(), "<truncated>");
    }
    fprintf(stdout, "%s", base::JoinString(snippet_lines, "\n").c_str());
    fflush(stdout);
  }

  if (result.status == TestResult::TEST_SUCCESS) {
    ++test_success_count_;
  } else {
    tests_to_retry_.insert(result.full_name);
  }

  results_tracker_.AddTestResult(result);

  // TODO(phajdan.jr): Align counter (padding).
  std::string status_line(
      StringPrintf("[%" PRIuS "/%" PRIuS "] %s ",
                   test_finished_count_,
                   test_started_count_,
                   result.full_name.c_str()));
  if (result.completed()) {
    status_line.append(StringPrintf("(%" PRId64 " ms)",
                                    result.elapsed_time.InMilliseconds()));
  } else if (result.status == TestResult::TEST_TIMEOUT) {
    status_line.append("(TIMED OUT)");
  } else if (result.status == TestResult::TEST_CRASH) {
    status_line.append("(CRASHED)");
  } else if (result.status == TestResult::TEST_SKIPPED) {
    status_line.append("(SKIPPED)");
  } else if (result.status == TestResult::TEST_UNKNOWN) {
    status_line.append("(UNKNOWN)");
  } else {
    // Fail very loudly so it's not ignored.
    CHECK(false) << "Unhandled test result status: " << result.status;
  }
  fprintf(stdout, "%s\n", status_line.c_str());
  fflush(stdout);

  // We just printed a status line, reset the watchdog timer.
  watchdog_timer_.Reset();

  // Do not waste time on timeouts. We include tests with unknown results here
  // because sometimes (e.g. hang in between unit tests) that's how a timeout
  // gets reported.
  if (result.status == TestResult::TEST_TIMEOUT ||
      result.status == TestResult::TEST_UNKNOWN) {
    test_broken_count_++;
  }
  size_t broken_threshold =
      std::max(static_cast<size_t>(20), test_found_count_ / 10);
  if (!force_run_broken_tests_ && test_broken_count_ >= broken_threshold) {
    fprintf(stdout, "Too many badly broken tests (%" PRIuS "), exiting now.\n",
            test_broken_count_);
    fflush(stdout);

#if defined(OS_POSIX)
    KillSpawnedTestProcesses();
#endif  // defined(OS_POSIX)

    MaybeSaveSummaryAsJSON({"BROKEN_TEST_EARLY_EXIT", kUnreliableResultsTag});

    exit(1);
  }

  if (test_finished_count_ != test_started_count_)
    return;

  if (tests_to_retry_.empty() || retry_count_ >= retry_limit_) {
    OnTestIterationFinished();
    return;
  }

  if (!force_run_broken_tests_ && tests_to_retry_.size() >= broken_threshold) {
    fprintf(stdout,
            "Too many failing tests (%" PRIuS "), skipping retries.\n",
            tests_to_retry_.size());
    fflush(stdout);

    results_tracker_.AddGlobalTag("BROKEN_TEST_SKIPPED_RETRIES");
    results_tracker_.AddGlobalTag(kUnreliableResultsTag);

    OnTestIterationFinished();
    return;
  }

  retry_count_++;

  std::vector<std::string> test_names(tests_to_retry_.begin(),
                                      tests_to_retry_.end());

  tests_to_retry_.clear();

  size_t retry_started_count = launcher_delegate_->RetryTests(this, test_names);
  if (retry_started_count == 0) {
    // Signal failure, but continue to run all requested test iterations.
    // With the summary of all iterations at the end this is a good default.
    run_result_ = false;

    OnTestIterationFinished();
    return;
  }

  fprintf(stdout, "Retrying %" PRIuS " test%s (retry #%" PRIuS ")\n",
          retry_started_count,
          retry_started_count > 1 ? "s" : "",
          retry_count_);
  fflush(stdout);

  test_started_count_ += retry_started_count;
}

// Helper used to parse test filter files. Syntax is documented in
// //testing/buildbot/filters/README.md .
bool LoadFilterFile(const FilePath& file_path,
                    std::vector<std::string>* positive_filter,
                    std::vector<std::string>* negative_filter) {
  std::string file_content;
  if (!ReadFileToString(file_path, &file_content)) {
    LOG(ERROR) << "Failed to read the filter file.";
    return false;
  }

  std::vector<std::string> filter_lines = SplitString(
      file_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  int line_num = 0;
  for (const std::string& filter_line : filter_lines) {
    line_num++;

    size_t hash_pos = filter_line.find('#');

    // In case when # symbol is not in the beginning of the line and is not
    // proceeded with a space then it's likely that the comment was
    // unintentional.
    if (hash_pos != std::string::npos && hash_pos > 0 &&
        filter_line[hash_pos - 1] != ' ') {
      LOG(WARNING) << "Content of line " << line_num << " in " << file_path
                   << " after # is treated as a comment, " << filter_line;
    }

    // Strip comments and whitespace from each line.
    std::string trimmed_line =
        TrimWhitespaceASCII(filter_line.substr(0, hash_pos), TRIM_ALL)
            .as_string();

    if (trimmed_line.substr(0, 2) == "//") {
      LOG(ERROR) << "Line " << line_num << " in " << file_path
                 << " starts with //, use # for comments.";
      return false;
    }

    // Treat a line starting with '//' as a comment.
    if (trimmed_line.empty())
      continue;

    if (trimmed_line[0] == '-')
      negative_filter->push_back(trimmed_line.substr(1));
    else
      positive_filter->push_back(trimmed_line);
  }

  return true;
}

bool TestLauncher::Init() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  // Initialize sharding. Command line takes precedence over legacy environment
  // variables.
  if (command_line->HasSwitch(switches::kTestLauncherTotalShards) &&
      command_line->HasSwitch(switches::kTestLauncherShardIndex)) {
    if (!StringToInt(
            command_line->GetSwitchValueASCII(
                switches::kTestLauncherTotalShards),
            &total_shards_)) {
      LOG(ERROR) << "Invalid value for " << switches::kTestLauncherTotalShards;
      return false;
    }
    if (!StringToInt(
            command_line->GetSwitchValueASCII(
                switches::kTestLauncherShardIndex),
            &shard_index_)) {
      LOG(ERROR) << "Invalid value for " << switches::kTestLauncherShardIndex;
      return false;
    }
    fprintf(stdout,
            "Using sharding settings from command line. This is shard %d/%d\n",
            shard_index_, total_shards_);
    fflush(stdout);
  } else {
    if (!TakeInt32FromEnvironment(kTestTotalShards, &total_shards_))
      return false;
    if (!TakeInt32FromEnvironment(kTestShardIndex, &shard_index_))
      return false;
    fprintf(stdout,
            "Using sharding settings from environment. This is shard %d/%d\n",
            shard_index_, total_shards_);
    fflush(stdout);
  }
  if (shard_index_ < 0 ||
      total_shards_ < 0 ||
      shard_index_ >= total_shards_) {
    LOG(ERROR) << "Invalid sharding settings: we require 0 <= "
               << kTestShardIndex << " < " << kTestTotalShards
               << ", but you have " << kTestShardIndex << "=" << shard_index_
               << ", " << kTestTotalShards << "=" << total_shards_ << ".\n";
    return false;
  }

  // Make sure we don't pass any sharding-related environment to the child
  // processes. This test launcher implements the sharding completely.
  CHECK(UnsetEnvironmentVariableIfExists("GTEST_TOTAL_SHARDS"));
  CHECK(UnsetEnvironmentVariableIfExists("GTEST_SHARD_INDEX"));

  if (command_line->HasSwitch(kGTestRepeatFlag) &&
      !StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag),
                   &cycles_)) {
    LOG(ERROR) << "Invalid value for " << kGTestRepeatFlag;
    return false;
  }
  if (command_line->HasSwitch(kIsolatedScriptTestRepeatFlag) &&
      !StringToInt(
          command_line->GetSwitchValueASCII(kIsolatedScriptTestRepeatFlag),
          &cycles_)) {
    LOG(ERROR) << "Invalid value for " << kIsolatedScriptTestRepeatFlag;
    return false;
  }

  if (command_line->HasSwitch(switches::kTestLauncherRetryLimit)) {
    int retry_limit = -1;
    if (!StringToInt(command_line->GetSwitchValueASCII(
                         switches::kTestLauncherRetryLimit), &retry_limit) ||
        retry_limit < 0) {
      LOG(ERROR) << "Invalid value for " << switches::kTestLauncherRetryLimit;
      return false;
    }

    retry_limit_ = retry_limit;
  } else if (command_line->HasSwitch(
                 switches::kIsolatedScriptTestLauncherRetryLimit)) {
    int retry_limit = -1;
    if (!StringToInt(command_line->GetSwitchValueASCII(
                         switches::kIsolatedScriptTestLauncherRetryLimit),
                     &retry_limit) ||
        retry_limit < 0) {
      LOG(ERROR) << "Invalid value for "
                 << switches::kIsolatedScriptTestLauncherRetryLimit;
      return false;
    }

    retry_limit_ = retry_limit;
  } else if (BotModeEnabled() ||
             !(command_line->HasSwitch(kGTestFilterFlag) ||
               command_line->HasSwitch(kIsolatedScriptTestFilterFlag))) {
    // Retry failures 3 times by default if we are running all of the tests or
    // in bot mode.
    retry_limit_ = 3;
  }

  if (command_line->HasSwitch(switches::kTestLauncherForceRunBrokenTests))
    force_run_broken_tests_ = true;

  // Some of the TestLauncherDelegate implementations don't call into gtest
  // until they've already split into test-specific processes. This results
  // in gtest's native shuffle implementation attempting to shuffle one test.
  // Shuffling the list of tests in the test launcher (before the delegate
  // gets involved) ensures that the entire shard is shuffled.
  if (command_line->HasSwitch(kGTestShuffleFlag)) {
    shuffle_ = true;

    if (command_line->HasSwitch(kGTestRandomSeedFlag)) {
      const std::string custom_seed_str =
          command_line->GetSwitchValueASCII(kGTestRandomSeedFlag);
      uint32_t custom_seed = 0;
      if (!StringToUint(custom_seed_str, &custom_seed)) {
        LOG(ERROR) << "Unable to parse seed \"" << custom_seed_str << "\".";
        return false;
      }
      if (custom_seed >= kRandomSeedUpperBound) {
        LOG(ERROR) << "Seed " << custom_seed << " outside of expected range "
                   << "[0, " << kRandomSeedUpperBound << ")";
        return false;
      }
      shuffle_seed_ = custom_seed;
    } else {
      std::uniform_int_distribution<uint32_t> dist(0, kRandomSeedUpperBound);
      std::random_device random_dev;
      shuffle_seed_ = dist(random_dev);
    }
  } else if (command_line->HasSwitch(kGTestRandomSeedFlag)) {
    LOG(ERROR) << kGTestRandomSeedFlag << " requires " << kGTestShuffleFlag;
    return false;
  }

  fprintf(stdout, "Using %" PRIuS " parallel jobs.\n", parallel_jobs_);
  fflush(stdout);

  CreateAndStartTaskScheduler(static_cast<int>(parallel_jobs_));

  std::vector<std::string> positive_file_filter;
  std::vector<std::string> positive_gtest_filter;

  if (command_line->HasSwitch(switches::kTestLauncherFilterFile)) {
    auto filter =
        command_line->GetSwitchValueNative(switches::kTestLauncherFilterFile);
    for (auto filter_file :
         SplitString(filter, FILE_PATH_LITERAL(";"), base::TRIM_WHITESPACE,
                     base::SPLIT_WANT_ALL)) {
      base::FilePath filter_file_path =
          base::MakeAbsoluteFilePath(FilePath(filter_file));
      if (!LoadFilterFile(filter_file_path, &positive_file_filter,
                          &negative_test_filter_))
        return false;
    }
  }

  // Split --gtest_filter at '-', if there is one, to separate into
  // positive filter and negative filter portions.
  bool double_colon_supported = !command_line->HasSwitch(kGTestFilterFlag);
  std::string filter = command_line->GetSwitchValueASCII(
      double_colon_supported ? kIsolatedScriptTestFilterFlag
                             : kGTestFilterFlag);
  size_t dash_pos = filter.find('-');
  if (dash_pos == std::string::npos) {
    positive_gtest_filter =
        ExtractTestsFromFilter(filter, double_colon_supported);
  } else {
    // Everything up to the dash.
    positive_gtest_filter = ExtractTestsFromFilter(filter.substr(0, dash_pos),
                                                   double_colon_supported);

    // Everything after the dash.
    for (std::string pattern : ExtractTestsFromFilter(
             filter.substr(dash_pos + 1), double_colon_supported)) {
      negative_test_filter_.push_back(pattern);
    }
  }

  if (!launcher_delegate_->GetTests(&tests_)) {
    LOG(ERROR) << "Failed to get list of tests.";
    return false;
  }

  CombinePositiveTestFilters(std::move(positive_gtest_filter),
                             std::move(positive_file_filter));

  if (!results_tracker_.Init(*command_line)) {
    LOG(ERROR) << "Failed to initialize test results tracker.";
    return 1;
  }

#if defined(NDEBUG)
  results_tracker_.AddGlobalTag("MODE_RELEASE");
#else
  results_tracker_.AddGlobalTag("MODE_DEBUG");
#endif

  // Operating systems (sorted alphabetically).
  // Note that they can deliberately overlap, e.g. OS_LINUX is a subset
  // of OS_POSIX.
#if defined(OS_ANDROID)
  results_tracker_.AddGlobalTag("OS_ANDROID");
#endif

#if defined(OS_BSD)
  results_tracker_.AddGlobalTag("OS_BSD");
#endif

#if defined(OS_FREEBSD)
  results_tracker_.AddGlobalTag("OS_FREEBSD");
#endif

#if defined(OS_FUCHSIA)
  results_tracker_.AddGlobalTag("OS_FUCHSIA");
#endif

#if defined(OS_IOS)
  results_tracker_.AddGlobalTag("OS_IOS");
#endif

#if defined(OS_LINUX)
  results_tracker_.AddGlobalTag("OS_LINUX");
#endif

#if defined(OS_MACOSX)
  results_tracker_.AddGlobalTag("OS_MACOSX");
#endif

#if defined(OS_NACL)
  results_tracker_.AddGlobalTag("OS_NACL");
#endif

#if defined(OS_OPENBSD)
  results_tracker_.AddGlobalTag("OS_OPENBSD");
#endif

#if defined(OS_POSIX)
  results_tracker_.AddGlobalTag("OS_POSIX");
#endif

#if defined(OS_SOLARIS)
  results_tracker_.AddGlobalTag("OS_SOLARIS");
#endif

#if defined(OS_WIN)
  results_tracker_.AddGlobalTag("OS_WIN");
#endif

  // CPU-related tags.
#if defined(ARCH_CPU_32_BITS)
  results_tracker_.AddGlobalTag("CPU_32_BITS");
#endif

#if defined(ARCH_CPU_64_BITS)
  results_tracker_.AddGlobalTag("CPU_64_BITS");
#endif

  return true;
}

void TestLauncher::CombinePositiveTestFilters(
    std::vector<std::string> filter_a,
    std::vector<std::string> filter_b) {
  has_at_least_one_positive_filter_ = !filter_a.empty() || !filter_b.empty();
  if (!has_at_least_one_positive_filter_) {
    return;
  }
  // If two positive filters are present, only run tests that match a pattern
  // in both filters.
  if (!filter_a.empty() && !filter_b.empty()) {
    for (const auto& i : tests_) {
      std::string test_name = FormatFullTestName(i.test_case_name, i.test_name);
      bool found_a = false;
      bool found_b = false;
      for (const auto& k : filter_a) {
        found_a = found_a || MatchPattern(test_name, k);
      }
      for (const auto& k : filter_b) {
        found_b = found_b || MatchPattern(test_name, k);
      }
      if (found_a && found_b) {
        positive_test_filter_.push_back(test_name);
      }
    }
  } else if (!filter_a.empty()) {
    positive_test_filter_ = std::move(filter_a);
  } else {
    positive_test_filter_ = std::move(filter_b);
  }
}

void TestLauncher::RunTests() {
  std::vector<std::string> test_names;
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  for (const TestIdentifier& test_id : tests_) {
    std::string test_name =
        FormatFullTestName(test_id.test_case_name, test_id.test_name);

    results_tracker_.AddTest(test_name);

    if (test_name.find("DISABLED") != std::string::npos) {
      results_tracker_.AddDisabledTest(test_name);

      // Skip disabled tests unless explicitly requested.
      if (!command_line->HasSwitch(kGTestRunDisabledTestsFlag) &&
          !command_line->HasSwitch(kIsolatedScriptRunDisabledTestsFlag))
        continue;
    }

    if (!launcher_delegate_->ShouldRunTest(test_id.test_case_name,
                                           test_id.test_name)) {
      continue;
    }

    // Count tests in the binary, before we apply filter and sharding.
    test_found_count_++;

    std::string test_name_no_disabled =
        TestNameWithoutDisabledPrefix(test_name);

    // Skip the test that doesn't match the filter (if given).
    if (has_at_least_one_positive_filter_) {
      bool found = false;
      for (auto filter : positive_test_filter_) {
        if (MatchPattern(test_name, filter) ||
            MatchPattern(test_name_no_disabled, filter)) {
          found = true;
          break;
        }
      }

      if (!found)
        continue;
    }
    if (!negative_test_filter_.empty()) {
      bool excluded = false;
      for (auto filter : negative_test_filter_) {
        if (MatchPattern(test_name, filter) ||
            MatchPattern(test_name_no_disabled, filter)) {
          excluded = true;
          break;
        }
      }

      if (excluded)
        continue;
    }

    if (Hash(test_name) % total_shards_ != static_cast<uint32_t>(shard_index_))
      continue;

    // Report test locations after applying all filters, so that we report test
    // locations only for those tests that were run as part of this shard.
    results_tracker_.AddTestLocation(test_name, test_id.file, test_id.line);

    test_names.push_back(test_name);
  }

  if (shuffle_) {
    std::mt19937 randomizer;
    randomizer.seed(shuffle_seed_);
    std::shuffle(test_names.begin(), test_names.end(), randomizer);

    fprintf(stdout, "Randomizing with seed %u\n", shuffle_seed_);
    fflush(stdout);
  }

  // Save an early test summary in case the launcher crashes or gets killed.
  MaybeSaveSummaryAsJSON({"EARLY_SUMMARY", kUnreliableResultsTag});

  test_started_count_ = launcher_delegate_->RunTests(this, test_names);

  if (test_started_count_ == 0) {
    fprintf(stdout, "0 tests run\n");
    fflush(stdout);

    // No tests have actually been started, so kick off the next iteration.
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&TestLauncher::RunTestIteration, Unretained(this)));
  }
}

void TestLauncher::RunTestIteration() {
  const bool stop_on_failure =
      CommandLine::ForCurrentProcess()->HasSwitch(kGTestBreakOnFailure);
  if (cycles_ == 0 ||
      (stop_on_failure && test_success_count_ != test_finished_count_)) {
    RunLoop::QuitCurrentWhenIdleDeprecated();
    return;
  }

  // Special value "-1" means "repeat indefinitely".
  cycles_ = (cycles_ == -1) ? cycles_ : cycles_ - 1;

  test_found_count_ = 0;
  test_started_count_ = 0;
  test_finished_count_ = 0;
  test_success_count_ = 0;
  test_broken_count_ = 0;
  retry_count_ = 0;
  tests_to_retry_.clear();
  results_tracker_.OnTestIterationStarting();

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&TestLauncher::RunTests, Unretained(this)));
}

#if defined(OS_POSIX)
// I/O watcher for the reading end of the self-pipe above.
// Terminates any launched child processes and exits the process.
void TestLauncher::OnShutdownPipeReadable() {
  fprintf(stdout, "\nCaught signal. Killing spawned test processes...\n");
  fflush(stdout);

  KillSpawnedTestProcesses();

  MaybeSaveSummaryAsJSON({"CAUGHT_TERMINATION_SIGNAL", kUnreliableResultsTag});

  // The signal would normally kill the process, so exit now.
  _exit(1);
}
#endif  // defined(OS_POSIX)

void TestLauncher::MaybeSaveSummaryAsJSON(
    const std::vector<std::string>& additional_tags) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestLauncherSummaryOutput)) {
    FilePath summary_path(command_line->GetSwitchValuePath(
                              switches::kTestLauncherSummaryOutput));
    if (!results_tracker_.SaveSummaryAsJSON(summary_path, additional_tags)) {
      LOG(ERROR) << "Failed to save test launcher output summary.";
    }
  }
  if (command_line->HasSwitch(switches::kTestLauncherTrace)) {
    FilePath trace_path(
        command_line->GetSwitchValuePath(switches::kTestLauncherTrace));
    if (!GetTestLauncherTracer()->Dump(trace_path)) {
      LOG(ERROR) << "Failed to save test launcher trace.";
    }
  }
}

void TestLauncher::OnTestIterationFinished() {
  TestResultsTracker::TestStatusMap tests_by_status(
      results_tracker_.GetTestStatusMapForCurrentIteration());
  if (!tests_by_status[TestResult::TEST_UNKNOWN].empty())
    results_tracker_.AddGlobalTag(kUnreliableResultsTag);

  // When we retry tests, success is determined by having nothing more
  // to retry (everything eventually passed), as opposed to having
  // no failures at all.
  if (tests_to_retry_.empty()) {
    fprintf(stdout, "SUCCESS: all tests passed.\n");
    fflush(stdout);
  } else {
    // Signal failure, but continue to run all requested test iterations.
    // With the summary of all iterations at the end this is a good default.
    run_result_ = false;
  }

  results_tracker_.PrintSummaryOfCurrentIteration();

  // Kick off the next iteration.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&TestLauncher::RunTestIteration, Unretained(this)));
}

void TestLauncher::OnOutputTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  AutoLock lock(*GetLiveProcessesLock());

  fprintf(stdout, "Still waiting for the following processes to finish:\n");

  for (const auto& pair : *GetLiveProcesses()) {
#if defined(OS_WIN)
    fwprintf(stdout, L"\t%s\n", pair.second.GetCommandLineString().c_str());
#else
    fprintf(stdout, "\t%s\n", pair.second.GetCommandLineString().c_str());
#endif
  }

  fflush(stdout);

  // Arm the timer again - otherwise it would fire only once.
  watchdog_timer_.Reset();
}

size_t NumParallelJobs() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestLauncherJobs)) {
    // If the number of test launcher jobs was specified, return that number.
    size_t jobs = 0U;

    if (!StringToSizeT(
            command_line->GetSwitchValueASCII(switches::kTestLauncherJobs),
            &jobs) ||
        !jobs) {
      LOG(ERROR) << "Invalid value for " << switches::kTestLauncherJobs;
      return 0U;
    }
    return jobs;
  }
  if (!BotModeEnabled() &&
      (command_line->HasSwitch(kGTestFilterFlag) ||
       command_line->HasSwitch(kIsolatedScriptTestFilterFlag))) {
    // Do not run jobs in parallel by default if we are running a subset of
    // the tests and if bot mode is off.
    return 1U;
  }

  // Default to the number of processor cores.
  return base::checked_cast<size_t>(SysInfo::NumberOfProcessors());
}

std::string GetTestOutputSnippet(const TestResult& result,
                                 const std::string& full_output) {
  size_t run_pos = full_output.find(std::string("[ RUN      ] ") +
                                    result.full_name);
  if (run_pos == std::string::npos)
    return std::string();

  size_t end_pos = full_output.find(std::string("[  FAILED  ] ") +
                                    result.full_name,
                                    run_pos);
  // Only clip the snippet to the "OK" message if the test really
  // succeeded. It still might have e.g. crashed after printing it.
  if (end_pos == std::string::npos &&
      result.status == TestResult::TEST_SUCCESS) {
    end_pos = full_output.find(std::string("[       OK ] ") +
                               result.full_name,
                               run_pos);
  }
  if (end_pos != std::string::npos) {
    size_t newline_pos = full_output.find("\n", end_pos);
    if (newline_pos != std::string::npos)
      end_pos = newline_pos + 1;
  }

  std::string snippet(full_output.substr(run_pos));
  if (end_pos != std::string::npos)
    snippet = full_output.substr(run_pos, end_pos - run_pos);

  return snippet;
}

}  // namespace base
