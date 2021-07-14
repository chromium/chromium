// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#include <stdio.h>

#include <algorithm>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
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
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gtest_util.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/launcher/test_launcher_tracer.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if defined(OS_APPLE)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if defined(OS_WIN)
#include "base/strings/string_util_win.h"
#include "base/win/windows_version.h"

#include <windows.h>

// To avoid conflicts with the macro from the Windows SDK...
#undef GetCommandLine
#endif

#if defined(OS_FUCHSIA)
#include <lib/fdio/namespace.h>
#include <lib/zx/job.h>
#include <lib/zx/time.h>
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

// Prefix indicating test has to run prior to the other test.
const char kPreTestPrefix[] = "PRE_";

// Prefix indicating test is disabled, will not run unless specified.
const char kDisabledTestPrefix[] = "DISABLED_";

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

#if defined(OS_FUCHSIA)
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
#endif  // defined(OS_FUCHSIA)

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

  PlatformThread::Sleep(TimeDelta::FromMilliseconds(500));

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

  if (switches.find(switches::kTestLauncherRetriesLeft) == switches.end()) {
    switches[switches::kTestLauncherRetriesLeft] =
#if defined(OS_WIN)
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
#if defined(OS_WIN)
  new_command_line.PrependWrapper(UTF8ToWide(wrapper));
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
                                      TestLauncherDelegate* delegate,
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
    if (win::GetVersion() < win::Version::WIN8 &&
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

  const base::FilePath kDataPath(base::kPersistedDataDirectoryPath);

  // Clone all namespace entries from the current process, except /data, which
  // is overridden below.
  fdio_flat_namespace_t* flat_namespace = nullptr;
  zx_status_t result = fdio_ns_export_root(&flat_namespace);
  ZX_CHECK(ZX_OK == result, result) << "fdio_ns_export_root";
  for (size_t i = 0; i < flat_namespace->count; ++i) {
    base::FilePath path(flat_namespace->path[i]);
    if (path == kDataPath) {
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
  // subdirectory under data (/data/test-$PID) and binding that to /data on the
  // child process.
  CHECK(base::PathExists(kDataPath));

  // Create the test subdirectory with a name that is unique to the child test
  // process (qualified by parent PID and an autoincrementing test process
  // index).
  static base::AtomicSequenceNumber child_launch_index;
  base::FilePath nested_data_path = kDataPath.AppendASCII(
      base::StringPrintf("test-%zu-%d", base::Process::Current().Pid(),
                         child_launch_index.GetNext()));
  CHECK(!base::DirectoryExists(nested_data_path));
  CHECK(base::CreateDirectory(nested_data_path));
  DCHECK(base::DirectoryExists(nested_data_path));

  // Bind the new test subdirectory to /data in the child process' namespace.
  new_options.paths_to_transfer.push_back(
      {kDataPath,
       base::OpenDirectoryHandle(nested_data_path).TakeChannel().release()});
#endif  // defined(OS_FUCHSIA)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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

  int exit_code = 0;
  bool did_exit = false;

  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    did_exit = process.WaitForExitWithTimeout(timeout, &exit_code);
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

#if defined(OS_FUCHSIA)
  zx_status_t wait_status = WaitForJobExit(job_handle);
  if (wait_status != ZX_OK) {
    LOG(ERROR) << "Batch leaked jobs or processes.";
    exit_code = -1;
  }
#endif  // defined(OS_FUCHSIA)

  {
    // Note how we grab the log before issuing a possibly broad process kill.
    // Other code parts that grab the log kill processes, so avoid trying
    // to do that twice and trigger all kinds of log messages.
    AutoLock lock(*GetLiveProcessesLock());

#if defined(OS_FUCHSIA)
    zx_status_t status = job_handle.kill();
    ZX_CHECK(status == ZX_OK, status);

    // Cleanup the data directory.
    CHECK(DeletePathRecursively(nested_data_path));
#elif defined(OS_POSIX)
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
#endif  // defined(OS_POSIX)

    GetLiveProcesses()->erase(process.Handle());
  }

  GetTestLauncherTracer()->RecordProcessExecution(
      start_time, TimeTicks::Now() - start_time);

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
#if defined(OS_WIN)
  environment->emplace(L"TMP", temp_dir.value());
#elif defined(OS_APPLE)
  environment->emplace("MAC_CHROMIUM_TMPDIR", temp_dir.value());
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  environment->emplace("TMPDIR", temp_dir.value());
#endif
}

// This launches the child test process, waits for it to complete,
// and returns child process results.
ChildProcessResults DoLaunchChildTestProcess(
    const CommandLine& command_line,
    const FilePath& process_temp_dir,
    TimeDelta timeout,
    const TestLauncher::LaunchOptions& test_launch_options,
    bool redirect_stdio,
    TestLauncherDelegate* delegate) {
  TimeTicks start_time = TimeTicks::Now();

  ChildProcessResults result;

  ScopedFILE output_file;
  FilePath output_filename;
  if (redirect_stdio) {
    output_file = CreateAndOpenTemporaryStream(&output_filename);
    CHECK(output_file);
#if defined(OS_WIN)
    // Paint the file so that it will be deleted when all handles are closed.
    if (!FILEToFile(output_file.get()).DeleteOnClose(true)) {
      PLOG(WARNING) << "Failed to mark " << output_filename.AsUTF8Unsafe()
                    << " for deletion on close";
    }
#endif
  }

  LaunchOptions options;

  // Tell the child process to use its designated temporary directory.
  if (!process_temp_dir.empty())
    SetTemporaryDirectory(process_temp_dir, &options.environment);
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
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  options.kill_on_parent_death = true;
#endif

#endif  // !defined(OS_WIN)

  result.exit_code = LaunchChildTestProcessWithOptions(
      command_line, options, test_launch_options.flags, timeout, delegate,
      &result.was_timeout);

  if (redirect_stdio) {
    fflush(output_file.get());

    // Reading the file can sometimes fail when the process was killed midflight
    // (e.g. on test suite timeout): https://crbug.com/826408. Attempt to read
    // the output file anyways, but do not crash on failure in this case.
    CHECK(ReadStreamToString(output_file.get(), &result.output_file_contents) ||
          result.exit_code != 0);

    output_file.reset();
#if !defined(OS_WIN)
    // On Windows, the reset() above is enough to delete the file since it was
    // painted for such after being opened. Lesser platforms require an explicit
    // delete now.
    if (!DeleteFile(output_filename))
      LOG(WARNING) << "Failed to delete " << output_filename.AsUTF8Unsafe();
#endif
  }
  result.elapsed_time = TimeTicks::Now() - start_time;
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
                      size_t runner_count = 1u,
                      size_t batch_size = 1u)
      : launcher_(launcher),
        runner_count_(runner_count),
        batch_size_(batch_size) {}

  // Sets |test_names| to be run, with |batch_size| tests per process.
  // Posts LaunchNextTask |runner_count| number of times, each with a separate
  // task runner.
  void Run(const std::vector<std::string>& test_names);

 private:
  // Called to check if the next batch has to run on the same
  // sequence task runner and using the same temporary directory.
  static bool ShouldReuseStateFromLastBatch(
      const std::vector<std::string>& test_names) {
    return test_names.size() == 1u &&
           test_names.front().find(kPreTestPrefix) != std::string::npos;
  }

  // Launches the next child process on |task_runner| and clears
  // |last_task_temp_dir| from the previous task.
  void LaunchNextTask(scoped_refptr<TaskRunner> task_runner,
                      const FilePath& last_task_temp_dir);

  // Forwards |last_task_temp_dir| and launches the next task on main thread.
  // The method is called on |task_runner|.
  void ClearAndLaunchNext(scoped_refptr<TaskRunner> main_thread_runner,
                          scoped_refptr<TaskRunner> task_runner,
                          const FilePath& last_task_temp_dir) {
    main_thread_runner->PostTask(
        FROM_HERE,
        BindOnce(&TestRunner::LaunchNextTask, weak_ptr_factory_.GetWeakPtr(),
                 task_runner, last_task_temp_dir));
  }

  ThreadChecker thread_checker_;

  std::vector<std::string> tests_to_run_;
  TestLauncher* const launcher_;
  std::vector<scoped_refptr<TaskRunner>> task_runners_;
  // Number of sequenced task runners to use.
  const size_t runner_count_;
  // Number of TaskRunners that have finished.
  size_t runners_done_ = 0;
  // Number of tests per process, 0 is special case for all tests.
  const size_t batch_size_;
  RunLoop run_loop_;

  base::WeakPtrFactory<TestRunner> weak_ptr_factory_{this};
};

void TestRunner::Run(const std::vector<std::string>& test_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // No sequence runners, fail immediately.
  CHECK_GT(runner_count_, 0u);
  tests_to_run_ = test_names;
  // Reverse test order to avoid coping the whole vector when removing tests.
  ranges::reverse(tests_to_run_);
  runners_done_ = 0;
  task_runners_.clear();
  for (size_t i = 0; i < runner_count_; i++) {
    task_runners_.push_back(ThreadPool::CreateSequencedTaskRunner(
        {MayBlock(), TaskShutdownBehavior::BLOCK_SHUTDOWN}));
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        BindOnce(&TestRunner::LaunchNextTask, weak_ptr_factory_.GetWeakPtr(),
                 task_runners_.back(), FilePath()));
  }
  run_loop_.Run();
}

void TestRunner::LaunchNextTask(scoped_refptr<TaskRunner> task_runner,
                                const FilePath& last_task_temp_dir) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // delete previous temporary directory
  if (!last_task_temp_dir.empty() &&
      !DeletePathRecursively(last_task_temp_dir)) {
    // This needs to be non-fatal at least for Windows.
    LOG(WARNING) << "Failed to delete " << last_task_temp_dir.AsUTF8Unsafe();
  }

  // No more tests to run, finish sequence.
  if (tests_to_run_.empty()) {
    runners_done_++;
    // All sequence runners are done, quit the loop.
    if (runners_done_ == runner_count_)
      run_loop_.QuitWhenIdle();
    return;
  }

  // Create a temporary directory for this task. This directory will hold the
  // flags and results files for the child processes as well as their User Data
  // dir, where appropriate. For platforms that support per-child temp dirs,
  // this directory will also contain one subdirectory per child for that
  // child's process-wide temp dir.
  base::FilePath task_temp_dir;
  CHECK(CreateNewTempDirectory(FilePath::StringType(), &task_temp_dir));
  bool post_to_current_runner = true;
  size_t batch_size = (batch_size_ == 0) ? tests_to_run_.size() : batch_size_;

  int child_index = 0;
  while (post_to_current_runner && !tests_to_run_.empty()) {
    batch_size = std::min(batch_size, tests_to_run_.size());
    std::vector<std::string> batch(tests_to_run_.rbegin(),
                                   tests_to_run_.rbegin() + batch_size);
    tests_to_run_.erase(tests_to_run_.end() - batch_size, tests_to_run_.end());
    task_runner->PostTask(
        FROM_HERE,
        BindOnce(&TestLauncher::LaunchChildGTestProcess, Unretained(launcher_),
                 ThreadTaskRunnerHandle::Get(), batch, task_temp_dir,
                 CreateChildTempDirIfSupported(task_temp_dir, child_index++)));
    post_to_current_runner = ShouldReuseStateFromLastBatch(batch);
  }
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(&TestRunner::ClearAndLaunchNext, Unretained(this),
               ThreadTaskRunnerHandle::Get(), task_runner, task_temp_dir));
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
      force_run_broken_tests_(false),
      watchdog_timer_(FROM_HERE,
                      kOutputTimeout,
                      this,
                      &TestLauncher::OnOutputTimeout),
      parallel_jobs_(parallel_jobs),
      print_test_stdio_(AUTO) {}

TestLauncher::~TestLauncher() {
  if (base::ThreadPoolInstance::Get()) {
    base::ThreadPoolInstance::Get()->Shutdown();
  }
}

bool TestLauncher::Run(CommandLine* command_line) {
  if (!Init((command_line == nullptr) ? CommandLine::ForCurrentProcess()
                                      : command_line))
    return false;


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
      base::BindRepeating(&TestLauncher::OnShutdownPipeReadable,
                          Unretained(this)));
#endif  // defined(OS_POSIX)

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

  ChildProcessResults process_results = DoLaunchChildTestProcess(
      new_command_line, child_temp_dir,
      launcher_delegate_->GetTimeout() * test_names.size(), options,
      redirect_stdio_, launcher_delegate_);

  // Invoke ProcessTestResults on the original thread, not
  // on a worker pool thread.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(&TestLauncher::ProcessTestResults, Unretained(this), test_names,
               result_file, process_results.output_file_contents,
               process_results.elapsed_time, process_results.exit_code,
               process_results.was_timeout,
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

  if (result.output_snippet.length() > kOutputSnippetBytesLimit) {
    if (result.status == TestResult::TEST_SUCCESS)
      result.status = TestResult::TEST_EXCESSIVE_OUTPUT;

    // Keep the top and bottom of the log and truncate the middle part.
    result.output_snippet =
        result.output_snippet.substr(0, kOutputSnippetBytesLimit / 2) + "\n" +
        StringPrintf("<truncated (%zu bytes)>\n",
                     result.output_snippet.length()) +
        result.output_snippet.substr(result.output_snippet.length() -
                                     kOutputSnippetBytesLimit / 2) +
        "\n";
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

#if defined(OS_POSIX)
    KillSpawnedTestProcesses();
#endif  // defined(OS_POSIX)

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

  fprintf(stdout, "Using %zu parallel jobs.\n", parallel_jobs_);
  fflush(stdout);

  CreateAndStartThreadPool(static_cast<int>(parallel_jobs_));

  std::vector<std::string> positive_file_filter;
  std::vector<std::string> positive_gtest_filter;

  if (command_line->HasSwitch(switches::kTestLauncherFilterFile)) {
    auto filter =
        command_line->GetSwitchValueNative(switches::kTestLauncherFilterFile);
    for (auto filter_file :
         SplitStringPiece(filter, FILE_PATH_LITERAL(";"), base::TRIM_WHITESPACE,
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

  skip_diabled_tests_ =
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

#if defined(OS_APPLE)
  results_tracker_.AddGlobalTag("OS_APPLE");
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  results_tracker_.AddGlobalTag("OS_LINUX");
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  results_tracker_.AddGlobalTag("OS_CHROMEOS");
#endif

#if defined(OS_MAC)
  results_tracker_.AddGlobalTag("OS_MAC");
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

bool TestLauncher::InitTests() {
  std::vector<TestIdentifier> tests;
  if (!launcher_delegate_->GetTests(&tests)) {
    LOG(ERROR) << "Failed to get list of tests.";
    return false;
  }
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
    if (!test_info.disabled() || !skip_diabled_tests_)
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

void TestLauncher::CreateAndStartThreadPool(int num_parallel_jobs) {
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

std::vector<std::string> TestLauncher::CollectTests() {
  std::vector<std::string> test_names;
  // To support RTS(regression test selection), which may have 100,000 or
  // more exact gtest filter, we first split filter into exact filter
  // and wildcards filter, then exact filter can match faster.
  std::vector<StringPiece> positive_wildcards_filter;
  std::unordered_set<StringPiece, StringPieceHash> positive_exact_filter;
  positive_exact_filter.reserve(positive_test_filter_.size());
  for (const std::string& filter : positive_test_filter_) {
    if (filter.find('*') != std::string::npos) {
      positive_wildcards_filter.push_back(filter);
    } else {
      positive_exact_filter.insert(filter);
    }
  }

  std::vector<StringPiece> negative_wildcards_filter;
  std::unordered_set<StringPiece, StringPieceHash> negative_exact_filter;
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
      if (!found) {
        for (const StringPiece& filter : positive_wildcards_filter) {
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
    for (const StringPiece& filter : negative_wildcards_filter) {
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
    if (PersistentHash(prefix_stripped_name) % total_shards_ !=
        static_cast<uint32_t>(shard_index_)) {
      continue;
    }

    // Report test locations after applying all filters, so that we report test
    // locations only for those tests that were run as part of this shard.
    results_tracker_.AddTestLocation(test_name, test_info.file(),
                                     test_info.line());
    if (!test_info.pre_test()) {
      // Only a subset of tests that are run require placeholders -- namely,
      // those that will output results.
      results_tracker_.AddTestPlaceholder(test_name);
    }

    test_names.push_back(test_name);
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

#if defined(OS_POSIX)
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
#endif  // defined(OS_POSIX)

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
#if defined(OS_WIN)
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

#if defined(OS_WIN)
  // Use processors in all groups (Windows splits more than 64 logical
  // processors into groups).
  size_t cores = base::checked_cast<size_t>(
      ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
#else
  size_t cores = base::checked_cast<size_t>(SysInfo::NumberOfProcessors());
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

}  // namespace base
