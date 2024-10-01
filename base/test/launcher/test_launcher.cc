// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/test/launcher/test_launcher.h"

#include <stdio.h>

#include <algorithm>
#include <map>
#include <random>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/at_exit.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gtest_util.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/launcher/test_launcher_tracer.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_file_util.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>

#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/string_util_win.h"

// To avoid conflicts with the macro from the Windows SDK...
#undef GetCommandLine
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/fdio/namespace.h>
#include <lib/zx/job.h>
#include <lib/zx/time.h>
#include "base/atomic_sequence_num.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "base/path_service.h"
#endif

namespace base {

// See
// https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ
using ::operator<<;

// The environment variable name for the total number of test shards.
const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard index.
const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

// Prefix indicating test has to run prior to the other test.
const char kPreTestPrefix[] = "PRE_";

// Prefix indicating test is disabled, will not run unless specified.
const char kDisabledTestPrefix[] = "DISABLED_";

ResultWatcher::ResultWatcher(FilePath result_file, size_t num_tests)
    : result_file_(std::move(result_file)), num_tests_(num_tests) {}

bool ResultWatcher::PollUntilDone(TimeDelta timeout_per_test) {
  CHECK(timeout_per_test.is_positive());
  TimeTicks batch_deadline = TimeTicks::Now() + num_tests_ * timeout_per_test;
  TimeDelta time_to_next_check = timeout_per_test;
  do {
    if (WaitWithTimeout(time_to_next_check)) {
      return true;
    }
    time_to_next_check = PollOnce(timeout_per_test);
  } while (TimeTicks::Now() < batch_deadline &&
           time_to_next_check.is_positive());
  // The process may have exited or is about to exit. Give the process a grace
  // period to exit on its own.
  return WaitWithTimeout(TestTimeouts::tiny_timeout());
}

TimeDelta ResultWatcher::PollOnce(TimeDelta timeout_per_test) {
  std::vector<TestResult> test_results;
  // If the result watcher is unlucky enough to read the results while the
  // runner process is writing an update, it is possible to read an incomplete
  // XML entry, in which case `ProcessGTestOutput` will return false.
  if (!ProcessGTestOutput(result_file_, &test_results, nullptr)) {
    return TestTimeouts::tiny_timeout();
  }
  Time latest_completion = LatestCompletionTimestamp(test_results);
  // Didn't complete a single test before timeout, fail.
  if (latest_completion.is_null()) {
    return TimeDelta();
  }
  // The gtest result writer gets timestamps from `Time::Now`.
  TimeDelta time_since_latest_completion = Time::Now() - latest_completion;
  // This heuristic attempts to prevent unrelated clock changes between the
  // latest write and read from being falsely identified as a test timeout.
  // For example, daylight savings time starting or ending can add an
  // artificial delta of +1 or -1 hour to `time_since_latest_completion`.
  if (time_since_latest_completion.is_negative() ||
      time_since_latest_completion > kDaylightSavingsThreshold) {
    return timeout_per_test;
  }
  // Expect another test to complete no later than `timeout_per_test` after
  // the latest completion.
  return timeout_per_test - time_since_latest_completion;
}

Time ResultWatcher::LatestCompletionTimestamp(
    const std::vector<TestResult>& test_results) {
  CHECK_LE(test_results.size(), num_tests_);
  // Since the result file is append-only, timestamps should already be in
  // ascending order.
  for (const TestResult& result : Reversed(test_results)) {
    if (result.completed()) {
      Time test_start = result.timestamp.value_or(Time());
      return test_start + result.elapsed_time;
    }
  }
  return Time();
}

// Watch results generated by a child test process. Wait for the child process
// to exit between result checks.
class ProcessResultWatcher : public ResultWatcher {
 public:
  ProcessResultWatcher(FilePath result_file, size_t num_tests, Process& process)
      : ResultWatcher(result_file, num_tests), process_(process) {}

  // Get the exit code of the process, or -1 if the process has not exited yet.
  int GetExitCode();

  bool WaitWithTimeout(TimeDelta timeout) override;

 private:
  const raw_ref<Process> process_;
  int exit_code_ = -1;
};

int ProcessResultWatcher::GetExitCode() {
  return exit_code_;
}

bool ProcessResultWatcher::WaitWithTimeout(TimeDelta timeout) {
  return process_->WaitForExitWithTimeout(timeout, &exit_code_);
}

namespace {

// Global tag for test runs where the results are unreliable for any reason.
const char kUnreliableResultsTag[] = "UNRELIABLE_RESULTS";

// Maximum time of no output after which we print list of processes still
// running. This deliberately doesn't use TestTimeouts (which is otherwise
// a recommended solution), because they can be increased. This would defeat
// the purpose of this timeout, which is 1) to avoid buildbot "no output for
// X seconds" timeout killing the process 2) help communicate status of
// the test launcher to people looking at the output (no output for a long
// time is mysterious and gives no info about what is happening) 3) help
// debugging in case the process hangs anyway.
constexpr TimeDelta kOutputTimeout = Seconds(15);

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

#if BUILDFLAG(IS_FUCHSIA)
zx_status_t WaitForJobExit(const zx::job& job) {
  zx::time deadline =
      zx::deadline_after(zx::duration(kOutputTimeout.ToZxDuration()));
  zx_signals_t to_wait_for = ZX_JOB_NO_JOBS | ZX_JOB_NO_PROCESSES;
  while (to_wait_for) {
    zx_signals_t observed = 0;
    zx_status_t status = job.wait_one(to_wait_for, deadline, &observed);
    if (status != ZX_OK)
      return status;
    to_wait_for &= ~observed;
  }
  return ZX_OK;
}
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_POSIX)
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

  fprintf(stdout, "Sending SIGTERM to %zu child processes... ",
          GetLiveProcesses()->size());
  fflush(stdout);

  for (const auto& pair : *GetLiveProcesses()) {
    // Send the signal to entire process group.
    kill((-1) * (pair.first), SIGTERM);
  }

  fprintf(stdout,
          "done.\nGiving processes a chance to terminate cleanly... ");
  fflush(stdout);

  PlatformThread::Sleep(Milliseconds(500));

  fprintf(stdout, "done.\n");
  fflush(stdout);

  fprintf(stdout, "Sending SIGKILL to %zu child processes... ",
          GetLiveProcesses()->size());
  fflush(stdout);

  for (const auto& pair : *GetLiveProcesses()) {
    // Send the signal to entire process group.
    kill((-1) * (pair.first), SIGKILL);
  }

  fprintf(stdout, "done.\n");
  fflush(stdout);
}
#endif  // BUILDFLAG(IS_POSIX)

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
bool BotModeEnabled(const CommandLine* command_line) {
  std::unique_ptr<Environment> env(Environment::Create());
  return command_line->HasSwitch(switches::kTestLauncherBotMode) ||
         env->HasVar("CHROMIUM_TEST_LAUNCHER_BOT_MODE");
}

// Returns command line command line after gtest-specific processing
// and applying |wrapper|.
CommandLine PrepareCommandLineForGTest(const CommandLine& command_line,
                                       const std::string& wrapper,
                                       const size_t retries_left) {
  CommandLine new_command_line(command_line.GetProgram());
  CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Handled by the launcher process.
  switches.erase(kGTestRepeatFlag);
  switches.erase(kIsolatedScriptTestRepeatFlag);

  // Don't try to write the final XML report in child processes.
  switches.erase(kGTestOutputFlag);

#if BUILDFLAG(IS_IOS)
  // We only need the xctest flag for the parent process. Passing it to
  // child processes will cause the tests not to run, so remove it.
  switches.erase(switches::kEnableRunIOSUnittestsWithXCTest);
#endif

  if (switches.find(switches::kTestLauncherRetriesLeft) == switches.end()) {
    switches[switches::kTestLauncherRetriesLeft] =
#if BUILDFLAG(IS_WIN)
        base::NumberToWString(
#else
        base::NumberToString(
#endif
            retries_left);
  }

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  // Prepend wrapper after last CommandLine quasi-copy operation. CommandLine
  // does not really support removing switches well, and trying to do that
  // on a CommandLine with a wrapper is known to break.
  // TODO(phajdan.jr): Give it a try to support CommandLine removing switches.
#if BUILDFLAG(IS_WIN)
  new_command_line.PrependWrapper(UTF8ToWide(wrapper));
#else
  new_command_line.PrependWrapper(wrapper);
#endif

  return new_command_line;
}

// Launches a child process using |command_line|. If a test is still running
// after |timeout|, the child process is terminated and |*was_timeout| is set to
// true. Returns exit code of the process.
int LaunchChildTestProcessWithOptions(const CommandLine& command_line,
                                      const LaunchOptions& options,
                                      int flags,
                                      const FilePath& result_file,
                                      TimeDelta timeout_per_test,
                                      size_t num_tests,
                                      TestLauncherDelegate* delegate,
                                      bool* was_timeout) {
#if BUILDFLAG(IS_POSIX)
  // Make sure an option we rely on is present - see LaunchChildGTestProcess.
  DCHECK(options.new_process_group);
#endif

  LaunchOptions new_options(options);

#if BUILDFLAG(IS_WIN)
  DCHECK(!new_options.job_handle);

  win::ScopedHandle job_handle;
  if (flags & TestLauncher::USE_JOB_OBJECTS) {
    job_handle.Set(CreateJobObject(NULL, NULL));
    if (!job_handle.is_valid()) {
      LOG(ERROR) << "Could not create JobObject.";
      return -1;
    }

    DWORD job_flags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetJobObjectLimitFlags(job_handle.get(), job_flags)) {
      LOG(ERROR) << "Could not SetJobObjectLimitFlags.";
      return -1;
    }

    new_options.job_handle = job_handle.get();
  }
#elif BUILDFLAG(IS_FUCHSIA)
  DCHECK(!new_options.job_handle);

  // Set the clone policy, deliberately omitting FDIO_SPAWN_CLONE_NAMESPACE so
  // that we can install a different /data.
  new_options.spawn_flags = FDIO_SPAWN_CLONE_STDIO | FDIO_SPAWN_CLONE_JOB;

  const base::FilePath kDataPath(base::kPersistedDataDirectoryPath);
  const base::FilePath kCachePath(base::kPersistedCacheDirectoryPath);

  // Clone all namespace entries from the current process, except /data and
  // /cache, which are overridden below.
  fdio_flat_namespace_t* flat_namespace = nullptr;
  zx_status_t result = fdio_ns_export_root(&flat_namespace);
  ZX_CHECK(ZX_OK == result, result) << "fdio_ns_export_root";
  for (size_t i = 0; i < flat_namespace->count; ++i) {
    base::FilePath path(flat_namespace->path[i]);
    if (path == kDataPath || path == kCachePath) {
      result = zx_handle_close(flat_namespace->handle[i]);
      ZX_CHECK(ZX_OK == result, result) << "zx_handle_close";
    } else {
      new_options.paths_to_transfer.push_back(
          {path, flat_namespace->handle[i]});
    }
  }
  free(flat_namespace);

  zx::job job_handle;
  result = zx::job::create(*GetDefaultJob(), 0, &job_handle);
  ZX_CHECK(ZX_OK == result, result) << "zx_job_create";
  new_options.job_handle = job_handle.get();

  // Give this test its own isolated /data directory by creating a new temporary
  // subdirectory under data (/data/test-$PID) and binding paths under that to
  // /data and /cache in the child process.
  // Persistent data storage is mapped to /cache rather than system-provided
  // cache storage, to avoid unexpected purges (see crbug.com/1242170).
  CHECK(base::PathExists(kDataPath));

  // Create the test subdirectory with a name that is unique to the child test
  // process (qualified by parent PID and an autoincrementing test process
  // index).
  static base::AtomicSequenceNumber child_launch_index;
  const base::FilePath child_data_path = kDataPath.AppendASCII(
      base::StringPrintf("test-%zu-%d", base::Process::Current().Pid(),
                         child_launch_index.GetNext()));
  CHECK(!base::DirectoryExists(child_data_path));
  CHECK(base::CreateDirectory(child_data_path));
  DCHECK(base::DirectoryExists(child_data_path));

  const base::FilePath test_data_dir(child_data_path.AppendASCII("data"));
  CHECK(base::CreateDirectory(test_data_dir));
  const base::FilePath test_cache_dir(child_data_path.AppendASCII("cache"));
  CHECK(base::CreateDirectory(test_cache_dir));

  // Transfer handles to the new directories as /data and /cache in the child
  // process' namespace.
  new_options.paths_to_transfer.push_back(
      {kDataPath,
       base::OpenDirectoryHandle(test_data_dir).TakeChannel().release()});
  new_options.paths_to_transfer.push_back(
      {kCachePath,
       base::OpenDirectoryHandle(test_cache_dir).TakeChannel().release()});
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_WIN)
    // Allow the handle used to capture stdio and stdout to be inherited by the
    // child. Note that this is done under GetLiveProcessesLock() to ensure that
    // only the desired child receives the handle.
    if (new_options.stdout_handle) {
      ::SetHandleInformation(new_options.stdout_handle, HANDLE_FLAG_INHERIT,
                             HANDLE_FLAG_INHERIT);
    }
#endif

    process = LaunchProcess(command_line, new_options);

#if BUILDFLAG(IS_WIN)
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

  int exit_code = 0;
  bool did_exit = false;

  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    if (num_tests == 1) {
      did_exit = process.WaitForExitWithTimeout(timeout_per_test, &exit_code);
    } else {
      ProcessResultWatcher result_watcher(result_file, num_tests, process);
      did_exit = result_watcher.PollUntilDone(timeout_per_test);
      exit_code = result_watcher.GetExitCode();
    }
  }

  if (!did_exit) {
    if (delegate)
      delegate->OnTestTimedOut(command_line);

    *was_timeout = true;
    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    {
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
      // Ensure that the process terminates.
      process.Terminate(-1, true);
    }
  }

#if BUILDFLAG(IS_FUCHSIA)
  zx_status_t wait_status = WaitForJobExit(job_handle);
  if (wait_status != ZX_OK) {
    LOG(ERROR) << "Batch leaked jobs or processes.";
    exit_code = -1;
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  {
    // Note how we grab the log before issuing a possibly broad process kill.
    // Other code parts that grab the log kill processes, so avoid trying
    // to do that twice and trigger all kinds of log messages.
    AutoLock lock(*GetLiveProcessesLock());

#if BUILDFLAG(IS_FUCHSIA)
    zx_status_t status = job_handle.kill();
    ZX_CHECK(status == ZX_OK, status);

    // Cleanup the data directory.
    CHECK(DeletePathRecursively(child_data_path));
#elif BUILDFLAG(IS_POSIX)
    // It is not possible to waitpid() on any leaked sub-processes of the test
    // batch process, since those are not direct children of this process.
    // kill()ing the process-group will return a result indicating whether the
    // group was found (i.e. processes were still running in it) or not (i.e.
    // sub-processes had exited already). Unfortunately many tests (e.g. browser
    // tests) have processes exit asynchronously, so checking the kill() result
    // will report false failures.
    // Unconditionally kill the process group, regardless of the batch exit-code
    // until a better solution is available.
    kill(-1 * process.Handle(), SIGKILL);
#endif  // BUILDFLAG(IS_POSIX)

    GetLiveProcesses()->erase(process.Handle());
  }

  return exit_code;
}

struct ChildProcessResults {
  // Total time for DoLaunchChildTest Process to execute.
  TimeDelta elapsed_time;
  // If stdio is redirected, pass output file content.
  std::string output_file_contents;
  // True if child process timed out.
  bool was_timeout = false;
  // Exit code of child process.
  int exit_code;
  // Thread ID of the runner.
  PlatformThreadId thread_id;
  // The sequence number of the child test process executed.
  // It's used instead of process id to distinguish processes that process id
  // might be reused by OS.
  int process_num;
};

// Returns the path to a temporary directory within |task_temp_dir| for the
// child process of index |child_index|, or an empty FilePath if per-child temp
// dirs are not supported.
FilePath CreateChildTempDirIfSupported(const FilePath& task_temp_dir,
                                       int child_index) {
  if (!TestLauncher::SupportsPerChildTempDirs())
    return FilePath();
  FilePath child_temp = task_temp_dir.AppendASCII(NumberToString(child_index));
  CHECK(CreateDirectoryAndGetError(child_temp, nullptr));
  return child_temp;
}

// Adds the platform-specific variable setting |temp_dir| as a process's
// temporary directory to |environment|.
void SetTemporaryDirectory(const FilePath& temp_dir,
                           EnvironmentMap* environment) {
#if BUILDFLAG(IS_WIN)
  environment->emplace(L"TMP", temp_dir.value());
#elif BUILDFLAG(IS_APPLE)
  environment->emplace("MAC_CHROMIUM_TMPDIR", temp_dir.value());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  environment->emplace("TMPDIR", temp_dir.value());
#endif
}

// This launches the child test process, waits for it to complete,
// and returns child process results.
ChildProcessResults DoLaunchChildTestProcess(
    const CommandLine& command_line,
    const FilePath& process_temp_dir,
    const FilePath& result_file,
    TimeDelta timeout_per_test,
    size_t num_tests,
    const TestLauncher::LaunchOptions& test_launch_options,
    bool redirect_stdio,
    TestLauncherDelegate* delegate) {
  TimeTicks start_time = TimeTicks::Now();

  ChildProcessResults result;
  result.thread_id = PlatformThread::CurrentId();

  ScopedFILE output_file;
  FilePath output_filename;
  if (redirect_stdio) {
    output_file = CreateAndOpenTemporaryStream(&output_filename);
    CHECK(output_file);
#if BUILDFLAG(IS_WIN)
    // Paint the file so that it will be deleted when all handles are closed.
    if (!FILEToFile(output_file.get()).DeleteOnClose(true)) {
      PLOG(WARNING) << "Failed to mark " << output_filename.AsUTF8Unsafe()
                    << " for deletion on close";
    }
#endif
  }

  LaunchOptions options;

#if BUILDFLAG(IS_IOS)
  // We need to allow XPC to start extension processes so magically we set this
  // flag to 1.
  options.environment.emplace("XPC_FLAGS", "1");
#endif
  // Tell the child process to use its designated temporary directory.
  if (!process_temp_dir.empty())
    SetTemporaryDirectory(process_temp_dir, &options.environment);
#if BUILDFLAG(IS_WIN)

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

#else  // if !BUILDFLAG(IS_WIN)

  options.fds_to_remap = test_launch_options.fds_to_remap;
  if (redirect_stdio) {
    int output_file_fd = fileno(output_file.get());
    CHECK_LE(0, output_file_fd);
    options.fds_to_remap.push_back(
        std::make_pair(output_file_fd, STDOUT_FILENO));
    options.fds_to_remap.push_back(
        std::make_pair(output_file_fd, STDERR_FILENO));
  }

#if !BUILDFLAG(IS_FUCHSIA)
  options.new_process_group = true;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  options.kill_on_parent_death = true;
#endif

#endif  // !BUILDFLAG(IS_WIN)

  result.exit_code = LaunchChildTestProcessWithOptions(
      command_line, options, test_launch_options.flags, result_file,
      timeout_per_test, num_tests, delegate, &result.was_timeout);

  if (redirect_stdio) {
    fflush(output_file.get());

    // Reading the file can sometimes fail when the process was killed midflight
    // (e.g. on test suite timeout): https://crbug.com/826408. Attempt to read
    // the output file anyways, but do not crash on failure in this case.
    CHECK(ReadStreamToString(output_file.get(), &result.output_file_contents) ||
          result.exit_code != 0);

    output_file.reset();
#if !BUILDFLAG(IS_WIN)
    // On Windows, the reset() above is enough to delete the file since it was
    // painted for such after being opened. Lesser platforms require an explicit
    // delete now.
    if (!DeleteFile(output_filename))
      LOG(WARNING) << "Failed to delete " << output_filename.AsUTF8Unsafe();
#endif
  }
  result.elapsed_time = TimeTicks::Now() - start_time;
  result.process_num = GetTestLauncherTracer()->RecordProcessExecution(
      start_time, result.elapsed_time);
  return result;
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

// A test runner object to run tests across a number of sequence runners,
// and control running pre tests in sequence.
class TestRunner {
 public:
  explicit TestRunner(TestLauncher* launcher,
                      size_t max_workers = 1u,
                      size_t batch_size = 1u)
      : launcher_(launcher),
        max_workers_(max_workers),
        batch_size_(batch_size) {}

  // Sets |test_names| to be run, with |batch_size| tests per process.
  // Posts a job to run LaunchChildGTestProcess on |max_workers| workers.
  void Run(const std::vector<std::string>& test_names);

 private:
  // Called to check if the next batch has to run on the same
  // sequence task runner and using the same temporary directory.
  static bool IsPreTestBatch(const std::vector<std::string>& test_names) {
    return test_names.size() == 1u &&
           test_names.front().find(kPreTestPrefix) != std::string::npos;
  }

  bool IsSingleThreaded() const { return batch_size_ == 0; }

  void WorkerTask(scoped_refptr<TaskRunner> main_task_runner,
                  base::JobDelegate* delegate);

  size_t GetMaxConcurrency(size_t worker_count) {
    AutoLock auto_lock(lock_);
    if (IsSingleThreaded()) {
      return tests_to_run_.empty() ? 0 : 1;
    }

    // Round up the division to ensure enough workers for all tests.
    return std::min((tests_to_run_.size() + batch_size_ - 1) / batch_size_,
                    max_workers_);
  }

  std::vector<std::string> GetNextBatch() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    size_t batch_size;
    // Single threaded case runs all tests in one batch.
    if (IsSingleThreaded()) {
      batch_size = tests_to_run_.size();
    }
    // Run remaining tests up to |batch_size_|.
    else {
      batch_size = std::min(batch_size_, tests_to_run_.size());
    }
    std::vector<std::string> batch(tests_to_run_.rbegin(),
                                   tests_to_run_.rbegin() + batch_size);
    tests_to_run_.erase(tests_to_run_.end() - batch_size, tests_to_run_.end());
    return batch;
  }

  // Cleans up |task_temp_dir| from a previous task and quits |run_loop| if
  // |done|.
  void CleanupTask(base::ScopedTempDir task_temp_dir, bool done);

  ThreadChecker thread_checker_;

  const raw_ptr<TestLauncher> launcher_;
  JobHandle job_handle_;
  // Max number of workers to use.
  const size_t max_workers_;
  // Number of tests per process, 0 is special case for all tests.
  const size_t batch_size_;
  RunLoop run_loop_;
  // Protects member used concurrently by worker tasks.
  base::Lock lock_;
  std::vector<std::string> tests_to_run_ GUARDED_BY(lock_);

  base::WeakPtrFactory<TestRunner> weak_ptr_factory_{this};
};

void TestRunner::Run(const std::vector<std::string>& test_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // No workers, fail immediately.
  CHECK_GT(max_workers_, 0u);
  if (test_names.empty()) {
    return;
  }

  {
    AutoLock auto_lock(lock_);
    tests_to_run_ = test_names;
    // Reverse test order to avoid copying the whole vector when removing tests.
    std::reverse(tests_to_run_.begin(), tests_to_run_.end());
  }

  job_handle_ = base::PostJob(
      FROM_HERE, {TaskPriority::USER_BLOCKING, MayBlock()},
      BindRepeating(&TestRunner::WorkerTask, Unretained(this),
                    SingleThreadTaskRunner::GetCurrentDefault()),
      BindRepeating(&TestRunner::GetMaxConcurrency, Unretained(this)));

  run_loop_.Run();
}

void TestRunner::WorkerTask(scoped_refptr<TaskRunner> main_task_runner,
                            base::JobDelegate* delegate) {
  bool done = false;
  while (!done && !delegate->ShouldYield()) {
    // Create a temporary directory for this task. This directory will hold the
    // flags and results files for the child processes as well as their User
    // Data dir, where appropriate. For platforms that support per-child temp
    // dirs, this directory will also contain one subdirectory per child for
    // that child's process-wide temp dir.
    base::ScopedTempDir task_temp_dir;
    CHECK(task_temp_dir.CreateUniqueTempDirUnderPath(GetTempDirForTesting()));
    int child_index = 0;

    std::vector<std::vector<std::string>> batches;
    {
      AutoLock auto_lock(lock_);
      if (!tests_to_run_.empty()) {
        batches.push_back(GetNextBatch());
        while (IsPreTestBatch(batches.back())) {
          DCHECK(!tests_to_run_.empty());
          batches.push_back(GetNextBatch());
        }
      }
      done = tests_to_run_.empty();
    }
    for (const auto& batch : batches) {
      launcher_->LaunchChildGTestProcess(
          main_task_runner, batch, task_temp_dir.GetPath(),
          CreateChildTempDirIfSupported(task_temp_dir.GetPath(),
                                        child_index++));
    }

    // Cleaning up test results is scheduled to |main_task_runner| because it
    // must happen after all post processing step that was scheduled in
    // LaunchChildGTestProcess to |main_task_runner|.
    main_task_runner->PostTask(
        FROM_HERE,
        BindOnce(&TestRunner::CleanupTask, weak_ptr_factory_.GetWeakPtr(),
                 std::move(task_temp_dir), done));
  }
}

void TestRunner::CleanupTask(base::ScopedTempDir task_temp_dir, bool done) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // delete previous temporary directory
  if (!task_temp_dir.Delete()) {
    // This needs to be non-fatal at least for Windows.
    LOG(WARNING) << "Failed to delete "
                 << task_temp_dir.GetPath().AsUTF8Unsafe();
  }

  if (!done) {
    return;
  }

  if (job_handle_) {
    job_handle_.Cancel();
    run_loop_.QuitWhenIdle();
  }
}

// Returns the number of files and directories in |dir|, or 0 if |dir| is empty.
int CountItemsInDirectory(const FilePath& dir) {
  if (dir.empty())
    return 0;
  int items = 0;
  FileEnumerator file_enumerator(
      dir, /*recursive=*/false,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  for (FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    ++items;
  }
  return items;
}

// Truncates a snippet in the middle to the given byte limit. byte_limit should
// be at least 30.
std::string TruncateSnippet(std::string_view snippet, size_t byte_limit) {
  if (snippet.length() <= byte_limit) {
    return std::string(snippet);
  }
  std::string truncation_message =
      StringPrintf("\n<truncated (%zu bytes)>\n", snippet.length());
  if (truncation_message.length() > byte_limit) {
    // Fail gracefully.
    return truncation_message;
  }
  size_t remaining_limit = byte_limit - truncation_message.length();
  size_t first_half = remaining_limit / 2;
  return base::StrCat(
      {snippet.substr(0, first_half), truncation_message,
       snippet.substr(snippet.length() - (remaining_limit - first_half))});
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

class TestLauncher::TestInfo {
 public:
  TestInfo() = default;
  TestInfo(const TestInfo& other) = default;
  TestInfo(const TestIdentifier& test_id);
  ~TestInfo() = default;

  // Returns test name excluding DISABLE_ prefix.
  std::string GetDisabledStrippedName() const;

  // Returns full test name.
  std::string GetFullName() const;

  // Returns test name with PRE_ prefix added, excluding DISABLE_ prefix.
  std::string GetPreName() const;

  // Returns test name excluding DISABLED_ and PRE_ prefixes.
  std::string GetPrefixStrippedName() const;

  const std::string& test_case_name() const { return test_case_name_; }
  const std::string& test_name() const { return test_name_; }
  const std::string& file() const { return file_; }
  int line() const { return line_; }
  bool disabled() const { return disabled_; }
  bool pre_test() const { return pre_test_; }

 private:
  std::string test_case_name_;
  std::string test_name_;
  std::string file_;
  int line_;
  bool disabled_;
  bool pre_test_;
};

TestLauncher::TestInfo::TestInfo(const TestIdentifier& test_id)
    : test_case_name_(test_id.test_case_name),
      test_name_(test_id.test_name),
      file_(test_id.file),
      line_(test_id.line),
      disabled_(false),
      pre_test_(false) {
  disabled_ = GetFullName().find(kDisabledTestPrefix) != std::string::npos;
  pre_test_ = test_name_.find(kPreTestPrefix) != std::string::npos;
}

std::string TestLauncher::TestInfo::GetDisabledStrippedName() const {
  std::string test_name = GetFullName();
  ReplaceSubstringsAfterOffset(&test_name, 0, kDisabledTestPrefix,
                               std::string());
  return test_name;
}

std::string TestLauncher::TestInfo::GetFullName() const {
  return FormatFullTestName(test_case_name_, test_name_);
}

std::string TestLauncher::TestInfo::GetPreName() const {
  std::string name = test_name_;
  ReplaceSubstringsAfterOffset(&name, 0, kDisabledTestPrefix, std::string());
  std::string case_name = test_case_name_;
  ReplaceSubstringsAfterOffset(&case_name, 0, kDisabledTestPrefix,
                               std::string());
  return FormatFullTestName(case_name, kPreTestPrefix + name);
}

std::string TestLauncher::TestInfo::GetPrefixStrippedName() const {
  std::string test_name = GetDisabledStrippedName();
  ReplaceSubstringsAfterOffset(&test_name, 0, kPreTestPrefix, std::string());
  return test_name;
}

TestLauncherDelegate::~TestLauncherDelegate() = default;

bool TestLauncherDelegate::ShouldRunTest(const TestIdentifier& test) {
  return true;
}

TestLauncher::LaunchOptions::LaunchOptions() = default;
TestLauncher::LaunchOptions::LaunchOptions(const LaunchOptions& other) =
    default;
TestLauncher::LaunchOptions::~LaunchOptions() = default;

TestLauncher::TestLauncher(TestLauncherDelegate* launcher_delegate,
                           size_t parallel_jobs,
                           size_t retry_limit)
    : launcher_delegate_(launcher_delegate),
      total_shards_(1),
      shard_index_(0),
      cycles_(1),
      broken_threshold_(0),
      test_started_count_(0),
      test_finished_count_(0),
      test_success_count_(0),
      test_broken_count_(0),
      retries_left_(0),
      retry_limit_(retry_limit),
      output_bytes_limit_(kOutputSnippetBytesLimit),
      force_run_broken_tests_(false),
      watchdog_timer_(FROM_HERE,
                      kOutputTimeout,
                      this,
                      &TestLauncher::OnOutputTimeout),
      parallel_jobs_(parallel_jobs),
      print_test_stdio_(AUTO) {}

TestLauncher::~TestLauncher() {
  if (base::ThreadPoolInstance::Get()) {
    // Clear the ThreadPoolInstance entirely to make it clear to final cleanup
    // phases that they are happening in a single-threaded phase. Assertions in
    // code like ~ScopedFeatureList are unhappy otherwise (crbug.com/1359095).
    base::ThreadPoolInstance::Get()->Shutdown();
    base::ThreadPoolInstance::Get()->JoinForTesting();
    base::ThreadPoolInstance::Set(nullptr);
  }
}

bool TestLauncher::Run(CommandLine* command_line) {
  base::PlatformThread::SetName("TestLauncherMain");

  if (!Init((command_line == nullptr) ? CommandLine::ForCurrentProcess()
                                      : command_line))
    return false;

#if BUILDFLAG(IS_POSIX)
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
      base::BindRepeating(&TestLauncher::OnShutdownPipeReadable,
                          Unretained(this)));
#endif  // BUILDFLAG(IS_POSIX)

  // Start the watchdog timer.
  watchdog_timer_.Reset();

  // Indicate a test did not succeed.
  bool test_failed = false;
  int iterations = cycles_;
  if (cycles_ > 1 && !stop_on_failure_) {
    // If we don't stop on failure, execute all the repeats in all iteration,
    // which allows us to parallelize the execution.
    iterations = 1;
    repeats_per_iteration_ = cycles_;
  }
  // Set to false if any iteration fails.
  bool run_result = true;

  while ((iterations > 0 || iterations == -1) &&
         !(stop_on_failure_ && test_failed)) {
    OnTestIterationStart();

    RunTests();
    bool retry_result = RunRetryTests();
    // Signal failure, but continue to run all requested test iterations.
    // With the summary of all iterations at the end this is a good default.
    run_result = run_result && retry_result;

    if (retry_result) {
      fprintf(stdout, "SUCCESS: all tests passed.\n");
      fflush(stdout);
    }

    test_failed = test_success_count_ != test_finished_count_;
    OnTestIterationFinished();
    // Special value "-1" means "repeat indefinitely".
    iterations = (iterations == -1) ? iterations : iterations - 1;
  }

  if (cycles_ != 1)
    results_tracker_.PrintSummaryOfAllIterations();

  MaybeSaveSummaryAsJSON(std::vector<std::string>());

  return run_result;
}

void TestLauncher::LaunchChildGTestProcess(
    scoped_refptr<TaskRunner> task_runner,
    const std::vector<std::string>& test_names,
    const FilePath& task_temp_dir,
    const FilePath& child_temp_dir) {
  FilePath result_file;
  CommandLine cmd_line = launcher_delegate_->GetCommandLine(
      test_names, task_temp_dir, &result_file);

  // Record the exact command line used to launch the child.
  CommandLine new_command_line(PrepareCommandLineForGTest(
      cmd_line, launcher_delegate_->GetWrapper(), retries_left_));
  LaunchOptions options;
  options.flags = launcher_delegate_->GetLaunchOptions();

  if (BotModeEnabled(CommandLine::ForCurrentProcess())) {
    LOG(INFO) << "Starting [" << base::JoinString(test_names, ", ") << "]";
  }

  ChildProcessResults process_results = DoLaunchChildTestProcess(
      new_command_line, child_temp_dir, result_file,
      launcher_delegate_->GetTimeout(), test_names.size(), options,
      redirect_stdio_, launcher_delegate_);

  // Invoke ProcessTestResults on the original thread, not
  // on a worker pool thread.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(&TestLauncher::ProcessTestResults, Unretained(this), test_names,
               result_file, process_results.output_file_contents,
               process_results.elapsed_time, process_results.exit_code,
               process_results.was_timeout, process_results.thread_id,
               process_results.process_num,
               CountItemsInDirectory(child_temp_dir)));
}

// Determines which result status will be assigned for missing test results.
TestResult::Status MissingResultStatus(size_t tests_to_run_count,
                                       bool was_timeout,
                                       bool exit_code) {
  // There is more than one test, cannot assess status.
  if (tests_to_run_count > 1u)
    return TestResult::TEST_SKIPPED;

  // There is only one test and no results.
  // Try to determine status by timeout or exit code.
  if (was_timeout)
    return TestResult::TEST_TIMEOUT;
  if (exit_code != 0)
    return TestResult::TEST_FAILURE;

  // It's strange case when test executed successfully,
  // but we failed to read machine-readable report for it.
  return TestResult::TEST_UNKNOWN;
}

// Returns interpreted test results.
void TestLauncher::ProcessTestResults(
    const std::vector<std::string>& test_names,
    const FilePath& result_file,
    const std::string& output,
    TimeDelta elapsed_time,
    int exit_code,
    bool was_timeout,
    PlatformThreadId thread_id,
    int process_num,
    int leaked_items) {
  std::vector<TestResult> test_results;
  bool crashed = false;
  bool have_test_results =
      ProcessGTestOutput(result_file, &test_results, &crashed);

  if (!have_test_results) {
    // We do not have reliable details about test results (parsing test
    // stdout is known to be unreliable).
    LOG(ERROR) << "Failed to get out-of-band test success data, "
                  "dumping full stdio below:\n"
               << output << "\n";
    // This is odd, but sometimes ProcessGtestOutput returns
    // false, but TestResults is not empty.
    test_results.clear();
  }

  TestResult::Status missing_result_status =
      MissingResultStatus(test_names.size(), was_timeout, exit_code);

  // TODO(phajdan.jr): Check for duplicates and mismatches between
  // the results we got from XML file and tests we intended to run.
  std::map<std::string, TestResult> results_map;
  for (const auto& i : test_results)
    results_map[i.full_name] = i;

  // Results to be reported back to the test launcher.
  std::vector<TestResult> final_results;

  for (const auto& i : test_names) {
    if (Contains(results_map, i)) {
      TestResult test_result = results_map[i];
      // Fix up the test status: we forcibly kill the child process
      // after the timeout, so from XML results it looks just like
      // a crash.
      if ((was_timeout && test_result.status == TestResult::TEST_CRASH) ||
          // If we run multiple tests in a batch with a timeout applied
          // to the entire batch. It is possible that with other tests
          // running quickly some tests take longer than the per-test timeout.
          // For consistent handling of tests independent of order and other
          // factors, mark them as timing out.
          test_result.elapsed_time > launcher_delegate_->GetTimeout()) {
        test_result.status = TestResult::TEST_TIMEOUT;
      }
      final_results.push_back(test_result);
    } else {
      // TODO(phajdan.jr): Explicitly pass the info that the test didn't
      // run for a mysterious reason.
      LOG(ERROR) << "no test result for " << i;
      TestResult test_result;
      test_result.full_name = i;
      test_result.status = missing_result_status;
      final_results.push_back(test_result);
    }
  }
  // TODO(phajdan.jr): Handle the case where processing XML output
  // indicates a crash but none of the test results is marked as crashing.

  bool has_non_success_test = false;
  for (const auto& i : final_results) {
    if (i.status != TestResult::TEST_SUCCESS) {
      has_non_success_test = true;
      break;
    }
  }

  if (!has_non_success_test && exit_code != 0) {
    // This is a bit surprising case: all tests are marked as successful,
    // but the exit code was not zero. This can happen e.g. under memory
    // tools that report leaks this way. Mark all tests as a failure on exit,
    // and for more precise info they'd need to be retried serially.
    for (auto& i : final_results)
      i.status = TestResult::TEST_FAILURE_ON_EXIT;
  }

  for (auto& i : final_results) {
    // Fix the output snippet after possible changes to the test result.
    i.output_snippet = GetTestOutputSnippet(i, output);
    // The thread id injected here is the worker thread that launching the child
    // testing process, it might be different from the current thread that
    // ProcessTestResults.
    i.thread_id = thread_id;
    i.process_num = process_num;
  }

  if (leaked_items)
    results_tracker_.AddLeakedItems(leaked_items, test_names);

  launcher_delegate_->ProcessTestResults(final_results, elapsed_time);

  for (const auto& result : final_results)
    OnTestFinished(result);
}

void TestLauncher::OnTestFinished(const TestResult& original_result) {
  ++test_finished_count_;

  TestResult result(original_result);

  if (result.output_snippet.length() > output_bytes_limit_) {
    if (result.status == TestResult::TEST_SUCCESS)
      result.status = TestResult::TEST_EXCESSIVE_OUTPUT;

    result.output_snippet =
        TruncateSnippetFocused(result.output_snippet, output_bytes_limit_);
  }

  bool print_snippet = false;
  if (print_test_stdio_ == AUTO) {
    print_snippet = (result.status != TestResult::TEST_SUCCESS);
  } else if (print_test_stdio_ == ALWAYS) {
    print_snippet = true;
  } else if (print_test_stdio_ == NEVER) {
    print_snippet = false;
  }
  if (print_snippet) {
    std::vector<std::string_view> snippet_lines =
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
    // Records prefix stripped name to run all dependent tests.
    std::string test_name(result.full_name);
    ReplaceSubstringsAfterOffset(&test_name, 0, kPreTestPrefix, std::string());
    ReplaceSubstringsAfterOffset(&test_name, 0, kDisabledTestPrefix,
                                 std::string());
    tests_to_retry_.insert(test_name);
  }

  // There are no results for this tests,
  // most likley due to another test failing in the same batch.
  if (result.status != TestResult::TEST_SKIPPED)
    results_tracker_.AddTestResult(result);

  // TODO(phajdan.jr): Align counter (padding).
  std::string status_line(StringPrintf("[%zu/%zu] %s ", test_finished_count_,
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherPrintTimestamps)) {
    ::logging::ScopedLoggingSettings scoped_logging_setting;
    ::logging::SetLogItems(true, true, true, true);
    LOG(INFO) << "Test_finished_timestamp";
  }
  // We just printed a status line, reset the watchdog timer.
  watchdog_timer_.Reset();

  // Do not waste time on timeouts.
  if (result.status == TestResult::TEST_TIMEOUT) {
    test_broken_count_++;
  }
  if (!force_run_broken_tests_ && test_broken_count_ >= broken_threshold_) {
    fprintf(stdout, "Too many badly broken tests (%zu), exiting now.\n",
            test_broken_count_);
    fflush(stdout);

#if BUILDFLAG(IS_POSIX)
    KillSpawnedTestProcesses();
#endif  // BUILDFLAG(IS_POSIX)

    MaybeSaveSummaryAsJSON({"BROKEN_TEST_EARLY_EXIT"});

    exit(1);
  }
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
    std::string trimmed_line(
        TrimWhitespaceASCII(filter_line.substr(0, hash_pos), TRIM_ALL));

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

bool TestLauncher::IsOnlyExactPositiveFilterFromFile(
    const CommandLine* command_line) const {
  if (command_line->HasSwitch(kGTestFilterFlag)) {
    LOG(ERROR) << "Found " << switches::kTestLauncherFilterFile;
    return false;
  }
  if (!negative_test_filter_.empty()) {
    LOG(ERROR) << "Found negative filters in the filter file.";
    return false;
  }
  for (const auto& filter : positive_test_filter_) {
    if (Contains(filter, '*')) {
      LOG(ERROR) << "Found wildcard positive filters in the filter file.";
      return false;
    }
  }
  return true;
}

bool TestLauncher::Init(CommandLine* command_line) {
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
  } else if (command_line->HasSwitch(kGTestRepeatFlag) ||
             command_line->HasSwitch(kGTestBreakOnFailure)) {
    // If we are repeating tests or waiting for the first test to fail, disable
    // retries.
    retry_limit_ = 0U;
  } else if (!BotModeEnabled(command_line) &&
             (command_line->HasSwitch(kGTestFilterFlag) ||
              command_line->HasSwitch(kIsolatedScriptTestFilterFlag))) {
    // No retry flag specified, not in bot mode and filtered by flag
    // Set reties to zero
    retry_limit_ = 0U;
  }

  retries_left_ = retry_limit_;
  force_run_broken_tests_ =
      command_line->HasSwitch(switches::kTestLauncherForceRunBrokenTests);

  if (command_line->HasSwitch(switches::kTestLauncherOutputBytesLimit)) {
    int output_bytes_limit = -1;
    if (!StringToInt(command_line->GetSwitchValueASCII(
                         switches::kTestLauncherOutputBytesLimit),
                     &output_bytes_limit) ||
        output_bytes_limit < 0) {
      LOG(ERROR) << "Invalid value for "
                 << switches::kTestLauncherOutputBytesLimit;
      return false;
    }

    output_bytes_limit_ = output_bytes_limit;
  }

  fprintf(stdout, "Using %zu parallel jobs.\n", parallel_jobs_);
  fflush(stdout);

  CreateAndStartThreadPool(parallel_jobs_);

  std::vector<std::string> positive_file_filter;
  std::vector<std::string> positive_gtest_filter;

  if (command_line->HasSwitch(switches::kTestLauncherFilterFile)) {
    auto filter =
        command_line->GetSwitchValueNative(switches::kTestLauncherFilterFile);
    for (auto filter_file :
         SplitStringPiece(filter, FILE_PATH_LITERAL(";"), base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_ALL)) {
#if BUILDFLAG(IS_IOS)
      // On iOS, the filter files are bundled with the test application.
      base::FilePath data_dir;
      PathService::Get(DIR_SRC_TEST_DATA_ROOT, &data_dir);
      base::FilePath filter_file_path = data_dir.Append(FilePath(filter_file));
#else
      base::FilePath filter_file_path =
          base::MakeAbsoluteFilePath(FilePath(filter_file));
#endif  // BUILDFLAG(IS_IOS)

      if (!LoadFilterFile(filter_file_path, &positive_file_filter,
                          &negative_test_filter_))
        return false;
    }
  }

  // If kGTestRunDisabledTestsFlag is set, force running all negative
  // tests in testing/buildbot/filters.
  if (command_line->HasSwitch(kGTestRunDisabledTestsFlag)) {
    negative_test_filter_.clear();
  }

  // If `kEnforceExactPositiveFilter` is set, only accept exact positive
  // filters from the filter file.
  enforce_exact_postive_filter_ =
      command_line->HasSwitch(switches::kEnforceExactPositiveFilter);
  if (enforce_exact_postive_filter_ &&
      !IsOnlyExactPositiveFilterFromFile(command_line)) {
    LOG(ERROR) << "With " << switches::kEnforceExactPositiveFilter
               << ", only accept exact positive filters via "
               << switches::kTestLauncherFilterFile;
    return false;
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

  skip_disabled_tests_ =
      !command_line->HasSwitch(kGTestRunDisabledTestsFlag) &&
      !command_line->HasSwitch(kIsolatedScriptRunDisabledTestsFlag);

  if (!InitTests())
    return false;

  if (!ShuffleTests(command_line))
    return false;

  if (!ProcessAndValidateTests())
    return false;

  if (command_line->HasSwitch(switches::kTestLauncherPrintTestStdio)) {
    std::string print_test_stdio = command_line->GetSwitchValueASCII(
        switches::kTestLauncherPrintTestStdio);
    if (print_test_stdio == "auto") {
      print_test_stdio_ = AUTO;
    } else if (print_test_stdio == "always") {
      print_test_stdio_ = ALWAYS;
    } else if (print_test_stdio == "never") {
      print_test_stdio_ = NEVER;
    } else {
      LOG(WARNING) << "Invalid value of "
                   << switches::kTestLauncherPrintTestStdio << ": "
                   << print_test_stdio;
      return false;
    }
  }

  stop_on_failure_ = command_line->HasSwitch(kGTestBreakOnFailure);

  if (command_line->HasSwitch(switches::kTestLauncherSummaryOutput)) {
    summary_path_ = FilePath(
        command_line->GetSwitchValuePath(switches::kTestLauncherSummaryOutput));
  }
  if (command_line->HasSwitch(switches::kTestLauncherTrace)) {
    trace_path_ = FilePath(
        command_line->GetSwitchValuePath(switches::kTestLauncherTrace));
  }

  // When running in parallel mode we need to redirect stdio to avoid mixed-up
  // output. We also always redirect on the bots to get the test output into
  // JSON summary.
  redirect_stdio_ = (parallel_jobs_ > 1) || BotModeEnabled(command_line);

  CombinePositiveTestFilters(std::move(positive_gtest_filter),
                             std::move(positive_file_filter));

  if (!results_tracker_.Init(*command_line)) {
    LOG(ERROR) << "Failed to initialize test results tracker.";
    return true;
  }

#if defined(NDEBUG)
  results_tracker_.AddGlobalTag("MODE_RELEASE");
#else
  results_tracker_.AddGlobalTag("MODE_DEBUG");
#endif

  // Operating systems (sorted alphabetically).
  // Note that they can deliberately overlap, e.g. OS_LINUX is a subset
  // of OS_POSIX.
#if BUILDFLAG(IS_ANDROID)
  results_tracker_.AddGlobalTag("OS_ANDROID");
#endif

#if BUILDFLAG(IS_APPLE)
  results_tracker_.AddGlobalTag("OS_APPLE");
#endif

#if BUILDFLAG(IS_BSD)
  results_tracker_.AddGlobalTag("OS_BSD");
#endif

#if BUILDFLAG(IS_FREEBSD)
  results_tracker_.AddGlobalTag("OS_FREEBSD");
#endif

#if BUILDFLAG(IS_FUCHSIA)
  results_tracker_.AddGlobalTag("OS_FUCHSIA");
#endif

#if BUILDFLAG(IS_IOS)
  results_tracker_.AddGlobalTag("OS_IOS");
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  results_tracker_.AddGlobalTag("OS_LINUX");
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  results_tracker_.AddGlobalTag("OS_CHROMEOS");
#endif

#if BUILDFLAG(IS_MAC)
  results_tracker_.AddGlobalTag("OS_MAC");
#endif

#if BUILDFLAG(IS_NACL)
  results_tracker_.AddGlobalTag("OS_NACL");
#endif

#if BUILDFLAG(IS_OPENBSD)
  results_tracker_.AddGlobalTag("OS_OPENBSD");
#endif

#if BUILDFLAG(IS_POSIX)
  results_tracker_.AddGlobalTag("OS_POSIX");
#endif

#if BUILDFLAG(IS_SOLARIS)
  results_tracker_.AddGlobalTag("OS_SOLARIS");
#endif

#if BUILDFLAG(IS_WIN)
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

bool TestLauncher::InitTests() {
  std::vector<TestIdentifier> tests;
  if (!launcher_delegate_->GetTests(&tests)) {
    LOG(ERROR) << "Failed to get list of tests.";
    return false;
  }

  // Check for duplicate test names. These can cause difficult-to-diagnose
  // crashes in the test runner as well as confusion about exactly what test is
  // failing. See https://crbug.com/1463355 for details.
  std::unordered_set<std::string> full_test_names;
  bool dups_found = false;
  for (auto& test : tests) {
    const std::string full_test_name =
        test.test_case_name + "." + test.test_name;
    auto [it, inserted] = full_test_names.insert(full_test_name);
    if (!inserted) {
      LOG(WARNING) << "Duplicate test name found: " << full_test_name;
      dups_found = true;
    }
  }
  CHECK(!dups_found);

  std::vector<std::string> uninstantiated_tests;
  for (const TestIdentifier& test_id : tests) {
    TestInfo test_info(test_id);
    if (test_id.test_case_name == "GoogleTestVerification") {
      // GoogleTestVerification is used by googletest to detect tests that are
      // parameterized but not instantiated.
      uninstantiated_tests.push_back(test_id.test_name);
      continue;
    }
    // TODO(isamsonov): crbug.com/1004417 remove when windows builders
    // stop flaking on MANAUAL_ tests.
    if (launcher_delegate_->ShouldRunTest(test_id))
      tests_.push_back(test_info);
  }
  if (!uninstantiated_tests.empty()) {
    LOG(ERROR) << "Found uninstantiated parameterized tests. These test suites "
                  "will not run:";
    for (const std::string& name : uninstantiated_tests)
      LOG(ERROR) << "  " << name;
    LOG(ERROR) << "Please use INSTANTIATE_TEST_SUITE_P to instantiate the "
                  "tests, or GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST if "
                  "the parameter list can be intentionally empty. See "
                  "//third_party/googletest/src/docs/advanced.md";
    return false;
  }
  return true;
}

bool TestLauncher::ShuffleTests(CommandLine* command_line) {
  if (command_line->HasSwitch(kGTestShuffleFlag)) {
    uint32_t shuffle_seed;
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
      shuffle_seed = custom_seed;
    } else {
      std::uniform_int_distribution<uint32_t> dist(0, kRandomSeedUpperBound);
      std::random_device random_dev;
      shuffle_seed = dist(random_dev);
    }

    std::mt19937 randomizer;
    randomizer.seed(shuffle_seed);
    ranges::shuffle(tests_, randomizer);

    fprintf(stdout, "Randomizing with seed %u\n", shuffle_seed);
    fflush(stdout);
  } else if (command_line->HasSwitch(kGTestRandomSeedFlag)) {
    LOG(ERROR) << kGTestRandomSeedFlag << " requires " << kGTestShuffleFlag;
    return false;
  }
  return true;
}

bool TestLauncher::ProcessAndValidateTests() {
  bool result = true;
  std::unordered_set<std::string> disabled_tests;
  std::unordered_map<std::string, TestInfo> pre_tests;

  // Find disabled and pre tests
  for (const TestInfo& test_info : tests_) {
    std::string test_name = test_info.GetFullName();
    results_tracker_.AddTest(test_name);
    if (test_info.disabled()) {
      disabled_tests.insert(test_info.GetDisabledStrippedName());
      results_tracker_.AddDisabledTest(test_name);
    }
    if (test_info.pre_test())
      pre_tests[test_info.GetDisabledStrippedName()] = test_info;
  }

  std::vector<TestInfo> tests_to_run;
  for (const TestInfo& test_info : tests_) {
    std::string test_name = test_info.GetFullName();
    // If any test has a matching disabled test, fail and log for audit.
    if (base::Contains(disabled_tests, test_name)) {
      LOG(ERROR) << test_name << " duplicated by a DISABLED_ test";
      result = false;
    }

    // Passes on PRE tests, those will append when final test is found.
    if (test_info.pre_test())
      continue;

    std::vector<TestInfo> test_sequence;
    test_sequence.push_back(test_info);
    // Move Pre Tests prior to final test in order.
    while (base::Contains(pre_tests, test_sequence.back().GetPreName())) {
      test_sequence.push_back(pre_tests[test_sequence.back().GetPreName()]);
      pre_tests.erase(test_sequence.back().GetDisabledStrippedName());
    }
    // Skip disabled tests unless explicitly requested.
    if (!test_info.disabled() || !skip_disabled_tests_)
      tests_to_run.insert(tests_to_run.end(), test_sequence.rbegin(),
                          test_sequence.rend());
  }
  tests_ = std::move(tests_to_run);

  // If any tests remain in |pre_tests| map, fail and log for audit.
  for (const auto& i : pre_tests) {
    LOG(ERROR) << i.first << " is an orphaned pre test";
    result = false;
  }
  return result;
}

void TestLauncher::CreateAndStartThreadPool(size_t num_parallel_jobs) {
  base::ThreadPoolInstance::Create("TestLauncher");
  base::ThreadPoolInstance::Get()->Start({num_parallel_jobs});
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
      std::string test_name = i.GetFullName();
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

bool TestLauncher::ShouldRunInCurrentShard(
    std::string_view prefix_stripped_name) const {
  CHECK(!StartsWith(prefix_stripped_name, kPreTestPrefix));
  CHECK(!StartsWith(prefix_stripped_name, kDisabledTestPrefix));
  return PersistentHash(prefix_stripped_name) % total_shards_ ==
         static_cast<uint32_t>(shard_index_);
}

std::vector<std::string> TestLauncher::CollectTests() {
  std::vector<std::string> test_names;
  // To support RTS(regression test selection), which may have 100,000 or
  // more exact gtest filter, we first split filter into exact filter
  // and wildcards filter, then exact filter can match faster.
  std::vector<std::string_view> positive_wildcards_filter;
  std::unordered_set<std::string_view> positive_exact_filter;
  positive_exact_filter.reserve(positive_test_filter_.size());
  std::unordered_set<std::string> enforced_positive_tests;
  for (const std::string& filter : positive_test_filter_) {
    if (filter.find('*') != std::string::npos) {
      positive_wildcards_filter.push_back(filter);
    } else {
      positive_exact_filter.insert(filter);
    }
  }

  std::vector<std::string_view> negative_wildcards_filter;
  std::unordered_set<std::string_view> negative_exact_filter;
  negative_exact_filter.reserve(negative_test_filter_.size());
  for (const std::string& filter : negative_test_filter_) {
    if (filter.find('*') != std::string::npos) {
      negative_wildcards_filter.push_back(filter);
    } else {
      negative_exact_filter.insert(filter);
    }
  }

  for (const TestInfo& test_info : tests_) {
    std::string test_name = test_info.GetFullName();

    std::string prefix_stripped_name = test_info.GetPrefixStrippedName();

    // Skip the test that doesn't match the filter (if given).
    if (has_at_least_one_positive_filter_) {
      bool found = positive_exact_filter.find(test_name) !=
                       positive_exact_filter.end() ||
                   positive_exact_filter.find(prefix_stripped_name) !=
                       positive_exact_filter.end();
      if (found && enforce_exact_postive_filter_) {
        enforced_positive_tests.insert(prefix_stripped_name);
      }
      if (!found) {
        for (std::string_view filter : positive_wildcards_filter) {
          if (MatchPattern(test_name, filter) ||
              MatchPattern(prefix_stripped_name, filter)) {
            found = true;
            break;
          }
        }
      }

      if (!found)
        continue;
    }

    if (negative_exact_filter.find(test_name) != negative_exact_filter.end() ||
        negative_exact_filter.find(prefix_stripped_name) !=
            negative_exact_filter.end()) {
      continue;
    }

    bool excluded = false;
    for (std::string_view filter : negative_wildcards_filter) {
      if (MatchPattern(test_name, filter) ||
          MatchPattern(prefix_stripped_name, filter)) {
        excluded = true;
        break;
      }
    }
    if (excluded)
      continue;

    // Tests with the name XYZ will cause tests with the name PRE_XYZ to run. We
    // should bucket all of these tests together.
    if (!ShouldRunInCurrentShard(prefix_stripped_name)) {
      continue;
    }

    // Report test locations after applying all filters, so that we report test
    // locations only for those tests that were run as part of this shard.
    results_tracker_.AddTestLocation(test_name, test_info.file(),
                                     test_info.line());

    if (!test_info.pre_test()) {
      // Only a subset of tests that are run require placeholders -- namely,
      // those that will output results. Note that the results for PRE_XYZ will
      // be merged into XYZ's results if the former fails, so we don't need a
      // placeholder for it.
      results_tracker_.AddTestPlaceholder(test_name);
    }

    test_names.push_back(test_name);
  }

  // If `kEnforceExactPositiveFilter` is set, all test cases listed in the
  // exact positive filter for the current shard should exist in the
  // `enforced_positive_tests`. Otherwise, print the missing cases and fail
  // loudly.
  if (enforce_exact_postive_filter_) {
    bool found_exact_positive_filter_not_enforced = false;
    for (const auto& filter : positive_exact_filter) {
      if (!ShouldRunInCurrentShard(filter) ||
          Contains(enforced_positive_tests, std::string(filter))) {
        continue;
      }
      if (!found_exact_positive_filter_not_enforced) {
        LOG(ERROR) << "Found exact positive filter not enforced:";
        found_exact_positive_filter_not_enforced = true;
      }
      LOG(ERROR) << filter;
    }
    CHECK(!found_exact_positive_filter_not_enforced);
  }

  return test_names;
}

void TestLauncher::RunTests() {
  std::vector<std::string> original_test_names = CollectTests();

  std::vector<std::string> test_names;
  for (int i = 0; i < repeats_per_iteration_; ++i) {
    test_names.insert(test_names.end(), original_test_names.begin(),
                      original_test_names.end());
  }

  broken_threshold_ = std::max(static_cast<size_t>(20), tests_.size() / 10);

  test_started_count_ = test_names.size();

  // If there are no matching tests, warn and notify of any matches against
  // *<filter>*.
  if (test_started_count_ == 0) {
    PrintFuzzyMatchingTestNames();
    fprintf(stdout, "WARNING: No matching tests to run.\n");
    fflush(stdout);
  }

  // Save an early test summary in case the launcher crashes or gets killed.
  results_tracker_.GeneratePlaceholderIteration();
  MaybeSaveSummaryAsJSON({"EARLY_SUMMARY"});

  // If we are repeating the test, set batch size to 1 to ensure that batch size
  // does not interfere with repeats (unittests are using filter for batches and
  // can't run the same test twice in the same batch).
  size_t batch_size =
      repeats_per_iteration_ > 1 ? 1U : launcher_delegate_->GetBatchSize();

  TestRunner test_runner(this, parallel_jobs_, batch_size);
  test_runner.Run(test_names);
}

void TestLauncher::PrintFuzzyMatchingTestNames() {
  for (auto filter : positive_test_filter_) {
    if (filter.empty())
      continue;
    std::string almost_filter;
    if (filter.front() != '*')
      almost_filter += '*';
    almost_filter += filter;
    if (filter.back() != '*')
      almost_filter += '*';

    for (const TestInfo& test_info : tests_) {
      std::string test_name = test_info.GetFullName();
      std::string prefix_stripped_name = test_info.GetPrefixStrippedName();
      if (MatchPattern(test_name, almost_filter) ||
          MatchPattern(prefix_stripped_name, almost_filter)) {
        fprintf(stdout, "Filter \"%s\" would have matched: %s\n",
                almost_filter.c_str(), test_name.c_str());
        fflush(stdout);
      }
    }
  }
}

bool TestLauncher::RunRetryTests() {
  while (!tests_to_retry_.empty() && retries_left_ > 0) {
    // Retry all tests that depend on a failing test.
    std::vector<std::string> test_names;
    for (const TestInfo& test_info : tests_) {
      if (base::Contains(tests_to_retry_, test_info.GetPrefixStrippedName()))
        test_names.push_back(test_info.GetFullName());
    }
    tests_to_retry_.clear();

    size_t retry_started_count = test_names.size();
    test_started_count_ += retry_started_count;

    // Only invoke RunLoop if there are any tasks to run.
    if (retry_started_count == 0)
      return false;

    fprintf(stdout, "Retrying %zu test%s (retry #%zu)\n", retry_started_count,
            retry_started_count > 1 ? "s" : "", retry_limit_ - retries_left_);
    fflush(stdout);

    --retries_left_;
    TestRunner test_runner(this);
    test_runner.Run(test_names);
  }
  return tests_to_retry_.empty();
}

void TestLauncher::OnTestIterationStart() {
  test_started_count_ = 0;
  test_finished_count_ = 0;
  test_success_count_ = 0;
  test_broken_count_ = 0;
  tests_to_retry_.clear();
  results_tracker_.OnTestIterationStarting();
}

#if BUILDFLAG(IS_POSIX)
// I/O watcher for the reading end of the self-pipe above.
// Terminates any launched child processes and exits the process.
void TestLauncher::OnShutdownPipeReadable() {
  fprintf(stdout, "\nCaught signal. Killing spawned test processes...\n");
  fflush(stdout);

  KillSpawnedTestProcesses();

  MaybeSaveSummaryAsJSON({"CAUGHT_TERMINATION_SIGNAL"});

  // The signal would normally kill the process, so exit now.
  _exit(1);
}
#endif  // BUILDFLAG(IS_POSIX)

void TestLauncher::MaybeSaveSummaryAsJSON(
    const std::vector<std::string>& additional_tags) {
  if (!summary_path_.empty()) {
    if (!results_tracker_.SaveSummaryAsJSON(summary_path_, additional_tags)) {
      LOG(ERROR) << "Failed to save test launcher output summary.";
    }
  }
  if (!trace_path_.empty()) {
    if (!GetTestLauncherTracer()->Dump(trace_path_)) {
      LOG(ERROR) << "Failed to save test launcher trace.";
    }
  }
}

void TestLauncher::OnTestIterationFinished() {
  TestResultsTracker::TestStatusMap tests_by_status(
      results_tracker_.GetTestStatusMapForCurrentIteration());
  if (!tests_by_status[TestResult::TEST_UNKNOWN].empty())
    results_tracker_.AddGlobalTag(kUnreliableResultsTag);

  results_tracker_.PrintSummaryOfCurrentIteration();
}

void TestLauncher::OnOutputTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  AutoLock lock(*GetLiveProcessesLock());

  fprintf(stdout, "Still waiting for the following processes to finish:\n");

  for (const auto& pair : *GetLiveProcesses()) {
#if BUILDFLAG(IS_WIN)
    fwprintf(stdout, L"\t%s\n", pair.second.GetCommandLineString().c_str());
#else
    fprintf(stdout, "\t%s\n", pair.second.GetCommandLineString().c_str());
#endif
  }

  fflush(stdout);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherPrintTimestamps)) {
    ::logging::ScopedLoggingSettings scoped_logging_setting;
    ::logging::SetLogItems(true, true, true, true);
    LOG(INFO) << "Waiting_timestamp";
  }
  // Arm the timer again - otherwise it would fire only once.
  watchdog_timer_.Reset();
}

size_t NumParallelJobs(unsigned int cores_per_job) {
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
  if (!BotModeEnabled(command_line) &&
      (command_line->HasSwitch(kGTestFilterFlag) ||
       command_line->HasSwitch(kIsolatedScriptTestFilterFlag))) {
    // Do not run jobs in parallel by default if we are running a subset of
    // the tests and if bot mode is off.
    return 1U;
  }

#if BUILDFLAG(IS_WIN)
  // Use processors in all groups (Windows splits more than 64 logical
  // processors into groups).
  size_t cores = base::checked_cast<size_t>(
      ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
#else
  size_t cores = base::checked_cast<size_t>(SysInfo::NumberOfProcessors());
#if BUILDFLAG(IS_MAC)
  // This is necessary to allow tests to call SetCpuSecurityMitigationsEnabled()
  // despite NumberOfProcessors() having already been called in the process.
  SysInfo::ResetCpuSecurityMitigationsEnabledForTesting();
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_IOS) && TARGET_OS_SIMULATOR
  // If we are targeting the simulator increase the number of jobs we use by 2x
  // the number of cores. This is necessary because the startup of each
  // process is slow, so using 2x empirically approaches the total machine
  // utilization.
  cores *= 2;
#endif
  return std::max(size_t(1), cores / cores_per_job);
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
  // succeeded or was skipped. It still might have e.g. crashed
  // after printing it.
  if (end_pos == std::string::npos) {
    if (result.status == TestResult::TEST_SUCCESS) {
      end_pos = full_output.find(std::string("[       OK ] ") +
                                result.full_name,
                                run_pos);

      // Also handle SKIPPED next to SUCCESS because the GTest XML output
      // doesn't make a difference between SKIPPED and SUCCESS
      if (end_pos == std::string::npos)
        end_pos = full_output.find(
            std::string("[  SKIPPED ] ") + result.full_name, run_pos);
    } else {
      // If test is not successful, include all output until subsequent test.
      end_pos = full_output.find(std::string("[ RUN      ]"), run_pos + 1);
      if (end_pos != std::string::npos)
        end_pos--;
    }
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

std::string TruncateSnippetFocused(std::string_view snippet,
                                   size_t byte_limit) {
  // Find the start of anything that looks like a fatal log message.
  // We want to preferentially preserve these from truncation as we
  // run extraction of fatal test errors from snippets in result_adapter
  // to populate failure reasons in ResultDB. It is also convenient for
  // the user to see them.
  // Refer to LogMessage::Init in base/logging[_platform].cc for patterns.
  size_t fatal_message_pos =
      std::min(snippet.find("FATAL:"), snippet.find("FATAL "));

  size_t fatal_message_start = 0;
  size_t fatal_message_end = 0;
  if (fatal_message_pos != std::string::npos) {
    // Find the line-endings before and after the fatal message.
    size_t start_pos = snippet.rfind("\n", fatal_message_pos);
    if (start_pos != std::string::npos) {
      fatal_message_start = start_pos;
    }
    size_t end_pos = snippet.find("\n", fatal_message_pos);
    if (end_pos != std::string::npos) {
      // Include the new-line character.
      fatal_message_end = end_pos + 1;
    } else {
      fatal_message_end = snippet.length();
    }
  }
  // Limit fatal message length to half the snippet byte quota. This ensures
  // we have space for some context at the beginning and end of the snippet.
  fatal_message_end =
      std::min(fatal_message_end, fatal_message_start + (byte_limit / 2));

  // Distribute remaining bytes between start and end of snippet.
  // The split is either even, or if one is small enough to be displayed
  // without truncation, it gets displayed in full and the other split gets
  // the remaining bytes.
  size_t remaining_bytes =
      byte_limit - (fatal_message_end - fatal_message_start);
  size_t start_split_bytes;
  size_t end_split_bytes;
  if (fatal_message_start < remaining_bytes / 2) {
    start_split_bytes = fatal_message_start;
    end_split_bytes = remaining_bytes - fatal_message_start;
  } else if ((snippet.length() - fatal_message_end) < remaining_bytes / 2) {
    start_split_bytes =
        remaining_bytes - (snippet.length() - fatal_message_end);
    end_split_bytes = (snippet.length() - fatal_message_end);
  } else {
    start_split_bytes = remaining_bytes / 2;
    end_split_bytes = remaining_bytes - start_split_bytes;
  }
  return base::StrCat(
      {TruncateSnippet(snippet.substr(0, fatal_message_start),
                       start_split_bytes),
       snippet.substr(fatal_message_start,
                      fatal_message_end - fatal_message_start),
       TruncateSnippet(snippet.substr(fatal_message_end), end_split_bytes)});
}

}  // namespace base
