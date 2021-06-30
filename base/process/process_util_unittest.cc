// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <malloc.h>
#include <sched.h>
#include <sys/syscall.h>
#endif
#if defined(OS_POSIX)
#include <sys/resource.h>
#endif
#if defined(OS_POSIX)
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#if defined(OS_WIN)
#include <windows.h>
#endif
#if defined(OS_APPLE)
#include <mach/vm_param.h>
#include <malloc/malloc.h>
#endif
#if defined(OS_ANDROID)
#include "third_party/lss/linux_syscall_support.h"
#endif
#if defined(OS_FUCHSIA)
#include <lib/fdio/limits.h>
#include <zircon/process.h>
#include <zircon/processargs.h>
#include <zircon/syscalls.h>
#include "base/base_paths_fuchsia.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#endif

namespace base {

namespace {

const char kSignalFileSlow[] = "SlowChildProcess.die";
const char kSignalFileKill[] = "KilledChildProcess.die";
const char kTestHelper[] = "test_child_process";

#if defined(OS_POSIX)
const char kSignalFileTerm[] = "TerminatedChildProcess.die";
#endif

#if defined(OS_FUCHSIA)
const char kSignalFileClone[] = "ClonedTmpDir.die";
const char kDataDirHasStaged[] = "DataDirHasStaged.die";
const char kFooDirHasStaged[] = "FooDirHasStaged.die";
const char kFooDirDoesNotHaveStaged[] = "FooDirDoesNotHaveStaged.die";
#endif

#if defined(OS_WIN)
const int kExpectedStillRunningExitCode = 0x102;
const int kExpectedKilledExitCode = 1;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
const int kExpectedStillRunningExitCode = 0;
#endif

// Sleeps until file filename is created.
void WaitToDie(const char* filename) {
  FILE* fp;
  do {
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));
    fp = fopen(filename, "r");
  } while (!fp);
  fclose(fp);
}

// Signals children they should die now.
void SignalChildren(const char* filename) {
  FILE* fp = fopen(filename, "w");
  fclose(fp);
}

// Using a pipe to the child to wait for an event was considered, but
// there were cases in the past where pipes caused problems (other
// libraries closing the fds, child deadlocking). This is a simple
// case, so it's not worth the risk.  Using wait loops is discouraged
// in most instances.
TerminationStatus WaitForChildTermination(ProcessHandle handle,
                                          int* exit_code) {
  // Now we wait until the result is something other than STILL_RUNNING.
  TerminationStatus status = TERMINATION_STATUS_STILL_RUNNING;
  const TimeDelta kInterval = TimeDelta::FromMilliseconds(20);
  TimeDelta waited;
  do {
    status = GetTerminationStatus(handle, exit_code);
    PlatformThread::Sleep(kInterval);
    waited += kInterval;
  } while (status == TERMINATION_STATUS_STILL_RUNNING &&
           waited < TestTimeouts::action_max_timeout());

  return status;
}

}  // namespace

const int kSuccess = 0;

class ProcessUtilTest : public MultiProcessTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(PathService::Get(DIR_ASSETS, &test_helper_path_));
    test_helper_path_ = test_helper_path_.AppendASCII(kTestHelper);
  }

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Spawn a child process that counts how many file descriptors are open.
  int CountOpenFDsInChild();
#endif
  // Converts the filename to a platform specific filepath.
  // On Android files can not be created in arbitrary directories.
  static std::string GetSignalFilePath(const char* filename);

 protected:
  base::FilePath test_helper_path_;
};

std::string ProcessUtilTest::GetSignalFilePath(const char* filename) {
#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
  FilePath tmp_dir;
  PathService::Get(DIR_TEMP, &tmp_dir);
  tmp_dir = tmp_dir.Append(filename);
  return tmp_dir.value();
#else
  return filename;
#endif
}

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  return kSuccess;
}

// TODO(viettrungluu): This should be in a "MultiProcessTestTest".
TEST_F(ProcessUtilTest, SpawnChild) {
  Process process = SpawnChild("SimpleChildProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
}

MULTIPROCESS_TEST_MAIN(SlowChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileSlow).c_str());
  return kSuccess;
}

TEST_F(ProcessUtilTest, KillSlowChild) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileSlow);
  remove(signal_file.c_str());
  Process process = SpawnChild("SlowChildProcess");
  ASSERT_TRUE(process.IsValid());
  SignalChildren(signal_file.c_str());
  int exit_code;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  remove(signal_file.c_str());
}

// Times out on Linux and Win, flakes on other platforms, http://crbug.com/95058
TEST_F(ProcessUtilTest, DISABLED_GetTerminationStatusExit) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileSlow);
  remove(signal_file.c_str());
  Process process = SpawnChild("SlowChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
  EXPECT_EQ(TERMINATION_STATUS_NORMAL_TERMINATION, status);
  EXPECT_EQ(kSuccess, exit_code);
  remove(signal_file.c_str());
}

#if defined(OS_FUCHSIA)

MULTIPROCESS_TEST_MAIN(CheckDataDirHasStaged) {
  if (!PathExists(base::FilePath("/data/staged"))) {
    return 1;
  }
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kDataDirHasStaged).c_str());
  return kSuccess;
}

// Test transferred paths override cloned paths.
TEST_F(ProcessUtilTest, HandleTransfersOverrideClones) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kDataDirHasStaged);
  remove(signal_file.c_str());

  // Create a tempdir with "staged" as its contents.
  ScopedTempDir tmpdir_with_staged;
  ASSERT_TRUE(tmpdir_with_staged.CreateUniqueTempDir());
  {
    base::FilePath staged_file_path =
        tmpdir_with_staged.GetPath().Append("staged");
    base::File staged_file(staged_file_path,
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(staged_file.created());
    staged_file.Close();
  }

  base::LaunchOptions options;
  options.spawn_flags = FDIO_SPAWN_CLONE_STDIO;

  // Attach the tempdir to "data", but also try to duplicate the existing "data"
  // directory.
  options.paths_to_clone.push_back(
      base::FilePath(base::kPersistedDataDirectoryPath));
  options.paths_to_clone.push_back(base::FilePath("/tmp"));
  options.paths_to_transfer.push_back(
      {FilePath(base::kPersistedDataDirectoryPath),
       base::OpenDirectoryHandle(base::FilePath(tmpdir_with_staged.GetPath()))
           .TakeChannel()
           .release()});

  // Verify from that "/data/staged" exists from the child process' perspective.
  Process process(SpawnChildWithOptions("CheckDataDirHasStaged", options));
  ASSERT_TRUE(process.IsValid());
  SignalChildren(signal_file.c_str());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

MULTIPROCESS_TEST_MAIN(CheckMountedDir) {
  if (!PathExists(base::FilePath("/foo/staged"))) {
    return 1;
  }
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kFooDirHasStaged).c_str());
  return kSuccess;
}

// Test that we can install an opaque handle in the child process' namespace.
TEST_F(ProcessUtilTest, TransferHandleToPath) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kFooDirHasStaged);
  remove(signal_file.c_str());

  // Create a tempdir with "staged" as its contents.
  ScopedTempDir new_tmpdir;
  ASSERT_TRUE(new_tmpdir.CreateUniqueTempDir());
  base::FilePath staged_file_path = new_tmpdir.GetPath().Append("staged");
  base::File staged_file(staged_file_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(staged_file.created());
  staged_file.Close();

  // Mount the tempdir to "/foo".
  zx::channel tmp_channel =
      base::OpenDirectoryHandle(new_tmpdir.GetPath()).TakeChannel();

  ASSERT_TRUE(tmp_channel.is_valid());
  LaunchOptions options;
  options.paths_to_clone.push_back(base::FilePath("/tmp"));
  options.paths_to_transfer.push_back(
      {base::FilePath("/foo"), tmp_channel.release()});
  options.spawn_flags = FDIO_SPAWN_CLONE_STDIO;

  // Verify from that "/foo/staged" exists from the child process' perspective.
  Process process(SpawnChildWithOptions("CheckMountedDir", options));
  ASSERT_TRUE(process.IsValid());
  SignalChildren(signal_file.c_str());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

MULTIPROCESS_TEST_MAIN(CheckTmpFileExists) {
  // Look through the filesystem to ensure that no other directories
  // besides "tmp" are in the namespace.
  base::FileEnumerator enumerator(
      base::FilePath("/"), false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  base::FilePath next_path;
  while (!(next_path = enumerator.Next()).empty()) {
    if (next_path != base::FilePath("/tmp")) {
      LOG(ERROR) << "Clone policy violation: found non-tmp directory "
                 << next_path.MaybeAsASCII();
      return 1;
    }
  }
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileClone).c_str());
  return kSuccess;
}

TEST_F(ProcessUtilTest, CloneTmp) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileClone);
  remove(signal_file.c_str());

  LaunchOptions options;
  options.paths_to_clone.push_back(base::FilePath("/tmp"));
  options.spawn_flags = FDIO_SPAWN_CLONE_STDIO;

  Process process(SpawnChildWithOptions("CheckTmpFileExists", options));
  ASSERT_TRUE(process.IsValid());

  SignalChildren(signal_file.c_str());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

MULTIPROCESS_TEST_MAIN(CheckMountedDirDoesNotExist) {
  if (PathExists(base::FilePath("/foo"))) {
    return 1;
  }
  WaitToDie(
      ProcessUtilTest::GetSignalFilePath(kFooDirDoesNotHaveStaged).c_str());
  return kSuccess;
}

TEST_F(ProcessUtilTest, TransferInvalidHandleFails) {
  LaunchOptions options;
  options.paths_to_clone.push_back(base::FilePath("/tmp"));
  options.paths_to_transfer.push_back(
      {base::FilePath("/foo"), ZX_HANDLE_INVALID});
  options.spawn_flags = FDIO_SPAWN_CLONE_STDIO;

  // Verify that the process is never constructed.
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kFooDirDoesNotHaveStaged);
  remove(signal_file.c_str());
  Process process(
      SpawnChildWithOptions("CheckMountedDirDoesNotExist", options));
  ASSERT_FALSE(process.IsValid());
}

TEST_F(ProcessUtilTest, CloneInvalidDirFails) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileClone);
  remove(signal_file.c_str());

  LaunchOptions options;
  options.paths_to_clone.push_back(base::FilePath("/tmp"));
  options.paths_to_clone.push_back(base::FilePath("/definitely_not_a_dir"));
  options.spawn_flags = FDIO_SPAWN_CLONE_STDIO;

  Process process(SpawnChildWithOptions("CheckTmpFileExists", options));
  ASSERT_FALSE(process.IsValid());
}

// Test that we can clone other directories. CheckTmpFileExists will return an
// error code if it detects a directory other than "/tmp", so we can use that as
// a signal that it successfully detected another entry in the root namespace.
TEST_F(ProcessUtilTest, CloneAlternateDir) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileClone);
  remove(signal_file.c_str());

  LaunchOptions options;
  options.paths_to_clone.push_back(base::FilePath("/tmp"));
  options.paths_to_clone.push_back(base::FilePath("/data"));
  options.spawn_flags = FDIO_SPAWN_CLONE_STDIO;

  Process process(SpawnChildWithOptions("CheckTmpFileExists", options));
  ASSERT_TRUE(process.IsValid());

  SignalChildren(signal_file.c_str());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(1, exit_code);
}

TEST_F(ProcessUtilTest, HandlesToTransferClosedOnSpawnFailure) {
  zx::handle handles[2];
  zx_status_t result = zx_channel_create(0, handles[0].reset_and_get_address(),
                                         handles[1].reset_and_get_address());
  ZX_CHECK(ZX_OK == result, result) << "zx_channel_create";

  LaunchOptions options;
  options.handles_to_transfer.push_back({0, handles[0].get()});

  // Launch a non-existent binary, causing fdio_spawn() to fail.
  CommandLine command_line(FilePath(
      FILE_PATH_LITERAL("💩magical_filename_that_will_never_exist_ever")));
  Process process(LaunchProcess(command_line, options));
  ASSERT_FALSE(process.IsValid());

  // If LaunchProcess did its job then handles[0] is no longer valid, and
  // handles[1] should observe a channel-closed signal.
  EXPECT_EQ(
      zx_object_wait_one(handles[1].get(), ZX_CHANNEL_PEER_CLOSED, 0, nullptr),
      ZX_OK);
  EXPECT_EQ(ZX_ERR_BAD_HANDLE, zx_handle_close(handles[0].get()));
  ignore_result(handles[0].release());
}

TEST_F(ProcessUtilTest, HandlesToTransferClosedOnBadPathToMapFailure) {
  zx::handle handles[2];
  zx_status_t result = zx_channel_create(0, handles[0].reset_and_get_address(),
                                         handles[1].reset_and_get_address());
  ZX_CHECK(ZX_OK == result, result) << "zx_channel_create";

  LaunchOptions options;
  options.handles_to_transfer.push_back({0, handles[0].get()});
  options.spawn_flags = options.spawn_flags & ~FDIO_SPAWN_CLONE_NAMESPACE;
  options.paths_to_clone.emplace_back(
      "💩magical_path_that_will_never_exist_ever");

  // LaunchProces should fail to open() the path_to_map, and fail before
  // fdio_spawn().
  Process process(LaunchProcess(CommandLine(FilePath()), options));
  ASSERT_FALSE(process.IsValid());

  // If LaunchProcess did its job then handles[0] is no longer valid, and
  // handles[1] should observe a channel-closed signal.
  EXPECT_EQ(
      zx_object_wait_one(handles[1].get(), ZX_CHANNEL_PEER_CLOSED, 0, nullptr),
      ZX_OK);
  EXPECT_EQ(ZX_ERR_BAD_HANDLE, zx_handle_close(handles[0].get()));
  ignore_result(handles[0].release());
}

TEST_F(ProcessUtilTest, FuchsiaProcessNameSuffix) {
  LaunchOptions options;
  options.process_name_suffix = "#test";
  Process process(SpawnChildWithOptions("SimpleChildProcess", options));

  char name[256] = {};
  size_t name_size = sizeof(name);
  zx_status_t status =
      zx_object_get_property(process.Handle(), ZX_PROP_NAME, name, name_size);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(std::string(name),
            CommandLine::ForCurrentProcess()->GetProgram().BaseName().value() +
                "#test");
}

#endif  // defined(OS_FUCHSIA)

// On Android SpawnProcess() doesn't use LaunchProcess() and doesn't support
// LaunchOptions::current_directory.
#if !defined(OS_ANDROID)
static void CheckCwdIsExpected(FilePath expected) {
  FilePath actual;
  CHECK(GetCurrentDirectory(&actual));
  actual = MakeAbsoluteFilePath(actual);
  CHECK(!actual.empty());

  CHECK_EQ(expected, actual);
}

// N.B. This test does extra work to check the cwd on multiple threads, because
// on macOS a per-thread cwd is set when using LaunchProcess().
MULTIPROCESS_TEST_MAIN(CheckCwdProcess) {
  // Get the expected cwd.
  FilePath temp_dir;
  CHECK(GetTempDir(&temp_dir));
  temp_dir = MakeAbsoluteFilePath(temp_dir);
  CHECK(!temp_dir.empty());

  // Test that the main thread has the right cwd.
  CheckCwdIsExpected(temp_dir);

  // Create a non-main thread.
  Thread thread("CheckCwdThread");
  thread.Start();
  auto task_runner = thread.task_runner();

  // A synchronization primitive used to wait for work done on the non-main
  // thread.
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC);
  RepeatingClosure signal_event =
      BindRepeating(&WaitableEvent::Signal, Unretained(&event));

  // Test that a non-main thread has the right cwd.
  task_runner->PostTask(FROM_HERE, BindOnce(&CheckCwdIsExpected, temp_dir));
  task_runner->PostTask(FROM_HERE, signal_event);

  event.Wait();

  // Get a new cwd for the process.
  const FilePath home_dir = PathService::CheckedGet(DIR_HOME);

  // Change the cwd on the secondary thread. IgnoreResult is used when setting
  // because it is checked immediately after.
  task_runner->PostTask(FROM_HERE,
                        BindOnce(IgnoreResult(&SetCurrentDirectory), home_dir));
  task_runner->PostTask(FROM_HERE, BindOnce(&CheckCwdIsExpected, home_dir));
  task_runner->PostTask(FROM_HERE, signal_event);

  event.Wait();

  // Make sure the main thread sees the cwd from the secondary thread.
  CheckCwdIsExpected(home_dir);

  // Change the directory back on the main thread.
  CHECK(SetCurrentDirectory(temp_dir));
  CheckCwdIsExpected(temp_dir);

  // Ensure that the secondary thread sees the new cwd too.
  task_runner->PostTask(FROM_HERE, BindOnce(&CheckCwdIsExpected, temp_dir));
  task_runner->PostTask(FROM_HERE, signal_event);

  event.Wait();

  // Change the cwd on the secondary thread one more time and join the thread.
  task_runner->PostTask(FROM_HERE,
                        BindOnce(IgnoreResult(&SetCurrentDirectory), home_dir));
  thread.Stop();

  // Make sure that the main thread picked up the new cwd.
  CheckCwdIsExpected(home_dir);

  return kSuccess;
}

TEST_F(ProcessUtilTest, CurrentDirectory) {
  // TODO(rickyz): Add support for passing arguments to multiprocess children,
  // then create a special directory for this test.
  FilePath tmp_dir;
  ASSERT_TRUE(GetTempDir(&tmp_dir));

  LaunchOptions options;
  options.current_directory = tmp_dir;

  Process process(SpawnChildWithOptions("CheckCwdProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
// TODO(cpu): figure out how to test this in other platforms.
TEST_F(ProcessUtilTest, GetProcId) {
  ProcessId id1 = GetProcId(GetCurrentProcess());
  EXPECT_NE(0ul, id1);
  Process process = SpawnChild("SimpleChildProcess");
  ASSERT_TRUE(process.IsValid());
  ProcessId id2 = process.Pid();
  EXPECT_NE(0ul, id2);
  EXPECT_NE(id1, id2);
}
#endif  // defined(OS_WIN)

#if !defined(OS_APPLE) && !defined(OS_ANDROID)
// This test is disabled on Mac, since it's flaky due to ReportCrash
// taking a variable amount of time to parse and load the debug and
// symbol data for this unit test's executable before firing the
// signal handler.
//
// TODO(gspencer): turn this test process into a very small program
// with no symbols (instead of using the multiprocess testing
// framework) to reduce the ReportCrash overhead.
//
// It is disabled on Android as MultiprocessTests are started as services that
// the framework restarts on crashes.
const char kSignalFileCrash[] = "CrashingChildProcess.die";

MULTIPROCESS_TEST_MAIN(CrashingChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileCrash).c_str());
#if defined(OS_POSIX)
  // Have to disable to signal handler for segv so we can get a crash
  // instead of an abnormal termination through the crash dump handler.
  ::signal(SIGSEGV, SIG_DFL);
#endif
  // Make this process have a segmentation fault.
  volatile int* oops = nullptr;
  *oops = 0xDEAD;
  return 1;
}

// This test intentionally crashes, so we don't need to run it under
// AddressSanitizer.
#if defined(ADDRESS_SANITIZER) || defined(OS_FUCHSIA)
// TODO(crbug.com/753490): Access to the process termination reason is not
// implemented in Fuchsia.
#define MAYBE_GetTerminationStatusCrash DISABLED_GetTerminationStatusCrash
#else
#define MAYBE_GetTerminationStatusCrash GetTerminationStatusCrash
#endif
TEST_F(ProcessUtilTest, MAYBE_GetTerminationStatusCrash) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileCrash);
  remove(signal_file.c_str());
  Process process = SpawnChild("CrashingChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_CRASHED, status);

#if defined(OS_WIN)
  EXPECT_EQ(static_cast<int>(0xc0000005), exit_code);
#elif defined(OS_POSIX)
  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGSEGV, signal);
#endif

  // Reset signal handlers back to "normal".
  debug::EnableInProcessStackDumping();
  remove(signal_file.c_str());
}
#endif  // !defined(OS_APPLE) && !defined(OS_ANDROID)

MULTIPROCESS_TEST_MAIN(KilledChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileKill).c_str());
#if defined(OS_WIN)
  // Kill ourselves.
  HANDLE handle = ::OpenProcess(PROCESS_ALL_ACCESS, 0, ::GetCurrentProcessId());
  ::TerminateProcess(handle, kExpectedKilledExitCode);
#elif defined(OS_POSIX)
  // Send a SIGKILL to this process, just like the OOM killer would.
  ::kill(getpid(), SIGKILL);
#elif defined(OS_FUCHSIA)
  zx_task_kill(zx_process_self());
#endif
  return 1;
}

#if defined(OS_POSIX)
MULTIPROCESS_TEST_MAIN(TerminatedChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileTerm).c_str());
  // Send a SIGTERM to this process.
  ::kill(getpid(), SIGTERM);
  return 1;
}
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
// TODO(crbug.com/753490): Access to the process termination reason is not
// implemented in Fuchsia.
#define MAYBE_GetTerminationStatusSigKill DISABLED_GetTerminationStatusSigKill
#else
#define MAYBE_GetTerminationStatusSigKill GetTerminationStatusSigKill
#endif
TEST_F(ProcessUtilTest, MAYBE_GetTerminationStatusSigKill) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileKill);
  remove(signal_file.c_str());
  Process process = SpawnChild("KilledChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM, status);
#else
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_WAS_KILLED, status);
#endif

#if defined(OS_WIN)
  EXPECT_EQ(kExpectedKilledExitCode, exit_code);
#elif defined(OS_POSIX)
  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGKILL, signal);
#endif
  remove(signal_file.c_str());
}

#if defined(OS_POSIX)
// TODO(crbug.com/753490): Access to the process termination reason is not
// implemented in Fuchsia. Unix signals are not implemented in Fuchsia so this
// test might not be relevant anyway.
TEST_F(ProcessUtilTest, GetTerminationStatusSigTerm) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileTerm);
  remove(signal_file.c_str());
  Process process = SpawnChild("TerminatedChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_WAS_KILLED, status);

  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGTERM, signal);
  remove(signal_file.c_str());
}
#endif  // defined(OS_POSIX)

TEST_F(ProcessUtilTest, EnsureTerminationUndying) {
  test::TaskEnvironment task_environment;

  Process child_process = SpawnChild("process_util_test_never_die");
  ASSERT_TRUE(child_process.IsValid());

  EnsureProcessTerminated(child_process.Duplicate());

#if defined(OS_POSIX)
  errno = 0;
#endif  // defined(OS_POSIX)

  // Allow a generous timeout, to cope with slow/loaded test bots.
  bool did_exit = child_process.WaitForExitWithTimeout(
      TestTimeouts::action_max_timeout(), nullptr);

#if defined(OS_POSIX)
  // Both EnsureProcessTerminated() and WaitForExitWithTimeout() will call
  // waitpid(). One will succeed, and the other will fail with ECHILD. If our
  // wait failed then check for ECHILD, and assumed |did_exit| in that case.
  did_exit = did_exit || (errno == ECHILD);
#endif  // defined(OS_POSIX)

  EXPECT_TRUE(did_exit);
}

MULTIPROCESS_TEST_MAIN(process_util_test_never_die) {
  while (1) {
    PlatformThread::Sleep(TimeDelta::FromSeconds(500));
  }
}

TEST_F(ProcessUtilTest, EnsureTerminationGracefulExit) {
  test::TaskEnvironment task_environment;

  Process child_process = SpawnChild("process_util_test_die_immediately");
  ASSERT_TRUE(child_process.IsValid());

  // Wait for the child process to actually exit.
  child_process.Duplicate().WaitForExitWithTimeout(
      TestTimeouts::action_max_timeout(), nullptr);

  EnsureProcessTerminated(child_process.Duplicate());

  // Verify that the process is really, truly gone.
  EXPECT_TRUE(child_process.WaitForExitWithTimeout(
      TestTimeouts::action_max_timeout(), nullptr));
}

MULTIPROCESS_TEST_MAIN(process_util_test_die_immediately) {
  return kSuccess;
}

#if defined(OS_WIN)
// TODO(estade): if possible, port this test.
TEST_F(ProcessUtilTest, LaunchAsUser) {
  UserTokenHandle token;
  ASSERT_TRUE(OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token));
  LaunchOptions options;
  options.as_user = token;
  EXPECT_TRUE(
      LaunchProcess(MakeCmdLine("SimpleChildProcess"), options).IsValid());
}

MULTIPROCESS_TEST_MAIN(ChildVerifiesCetDisabled) {
  auto get_process_mitigation_policy =
      reinterpret_cast<decltype(&GetProcessMitigationPolicy)>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));

  // Not available for Win7 but this process should still work.
  if (!get_process_mitigation_policy)
    return kSuccess;

  // Policy not defined for Win < Win10 20H1 but that's also ok.
  PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy = {};
  if (get_process_mitigation_policy(GetCurrentProcess(),
                                    ProcessUserShadowStackPolicy, &policy,
                                    sizeof(policy))) {
    if (policy.EnableUserShadowStack)
      return 1;
  }
  return kSuccess;
}

TEST_F(ProcessUtilTest, LaunchDisablingCetCompat) {
  LaunchOptions options;
  // This only has an effect on Windows > 20H2 with CET hardware but
  // is safe on every platform.
  options.disable_cetcompat = true;
  EXPECT_TRUE(LaunchProcess(MakeCmdLine("ChildVerifiesCetDisabled"), options)
                  .IsValid());
}

static const char kEventToTriggerHandleSwitch[] = "event-to-trigger-handle";

MULTIPROCESS_TEST_MAIN(TriggerEventChildProcess) {
  std::string handle_value_string =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kEventToTriggerHandleSwitch);
  CHECK(!handle_value_string.empty());

  uint64_t handle_value_uint64;
  CHECK(StringToUint64(handle_value_string, &handle_value_uint64));
  // Give ownership of the handle to |event|.
  WaitableEvent event(
      win::ScopedHandle(reinterpret_cast<HANDLE>(handle_value_uint64)));

  event.Signal();

  return 0;
}

TEST_F(ProcessUtilTest, InheritSpecifiedHandles) {
  // Manually create the event, so that it can be inheritable.
  SECURITY_ATTRIBUTES security_attributes = {};
  security_attributes.nLength = static_cast<DWORD>(sizeof(security_attributes));
  security_attributes.lpSecurityDescriptor = NULL;
  security_attributes.bInheritHandle = true;

  // Takes ownership of the event handle.
  WaitableEvent event(
      win::ScopedHandle(CreateEvent(&security_attributes, true, false, NULL)));
  LaunchOptions options;
  options.handles_to_inherit.emplace_back(event.handle());

  CommandLine cmd_line = MakeCmdLine("TriggerEventChildProcess");
  cmd_line.AppendSwitchASCII(
      kEventToTriggerHandleSwitch,
      NumberToString(reinterpret_cast<uint64_t>(event.handle())));

  // Launch the process and wait for it to trigger the event.
  ASSERT_TRUE(LaunchProcess(cmd_line, options).IsValid());
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
}
#endif  // defined(OS_WIN)

TEST_F(ProcessUtilTest, GetAppOutput) {
  base::CommandLine command(test_helper_path_);
  command.AppendArg("hello");
  command.AppendArg("there");
  command.AppendArg("good");
  command.AppendArg("people");
  std::string output;
  EXPECT_TRUE(GetAppOutput(command, &output));
  EXPECT_EQ("hello there good people", output);
  output.clear();

  const char* kEchoMessage = "blah";
  command = base::CommandLine(test_helper_path_);
  command.AppendArg("-x");
  command.AppendArg("28");
  command.AppendArg(kEchoMessage);
  EXPECT_FALSE(GetAppOutput(command, &output));
  EXPECT_EQ(kEchoMessage, output);
}

TEST_F(ProcessUtilTest, GetAppOutputWithExitCode) {
  const char* kEchoMessage1 = "doge";
  int exit_code = -1;
  base::CommandLine command(test_helper_path_);
  command.AppendArg(kEchoMessage1);
  std::string output;
  EXPECT_TRUE(GetAppOutputWithExitCode(command, &output, &exit_code));
  EXPECT_EQ(kEchoMessage1, output);
  EXPECT_EQ(0, exit_code);
  output.clear();

  const char* kEchoMessage2 = "pupper";
  const int kExpectedExitCode = 42;
  command = base::CommandLine(test_helper_path_);
  command.AppendArg("-x");
  command.AppendArg(base::NumberToString(kExpectedExitCode));
  command.AppendArg(kEchoMessage2);
#if defined(OS_WIN)
  // On Windows, anything that quits with a nonzero status code is handled as a
  // "crash", so just ignore GetAppOutputWithExitCode's return value.
  GetAppOutputWithExitCode(command, &output, &exit_code);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  EXPECT_TRUE(GetAppOutputWithExitCode(command, &output, &exit_code));
#endif
  EXPECT_EQ(kEchoMessage2, output);
  EXPECT_EQ(kExpectedExitCode, exit_code);
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)

namespace {

// Returns the maximum number of files that a process can have open.
// Returns 0 on error.
int GetMaxFilesOpenInProcess() {
#if defined(OS_FUCHSIA)
  return FDIO_MAX_FD;
#else
  struct rlimit rlim;
  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    return 0;
  }

  // rlim_t is a uint64_t - clip to maxint. We do this since FD #s are ints
  // which are all 32 bits on the supported platforms.
  rlim_t max_int = static_cast<rlim_t>(std::numeric_limits<int32_t>::max());
  if (rlim.rlim_cur > max_int) {
    return max_int;
  }

  return rlim.rlim_cur;
#endif  // defined(OS_FUCHSIA)
}

const int kChildPipe = 20;  // FD # for write end of pipe in child process.

#if defined(OS_APPLE)

// <http://opensource.apple.com/source/xnu/xnu-2422.1.72/bsd/sys/guarded.h>
#if !defined(_GUARDID_T)
#define _GUARDID_T
typedef __uint64_t guardid_t;
#endif  // _GUARDID_T

// From .../MacOSX10.9.sdk/usr/include/sys/syscall.h
#if !defined(SYS_change_fdguard_np)
#define SYS_change_fdguard_np 444
#endif

// <http://opensource.apple.com/source/xnu/xnu-2422.1.72/bsd/sys/guarded.h>
#if !defined(GUARD_DUP)
#define GUARD_DUP (1u << 1)
#endif

// <http://opensource.apple.com/source/xnu/xnu-2422.1.72/bsd/kern/kern_guarded.c?txt>
//
// Atomically replaces |guard|/|guardflags| with |nguard|/|nguardflags| on |fd|.
int change_fdguard_np(int fd,
                      const guardid_t* guard,
                      u_int guardflags,
                      const guardid_t* nguard,
                      u_int nguardflags,
                      int* fdflagsp) {
  return syscall(SYS_change_fdguard_np, fd, guard, guardflags, nguard,
                 nguardflags, fdflagsp);
}

// Attempt to set a file-descriptor guard on |fd|.  In case of success, remove
// it and return |true| to indicate that it can be guarded.  Returning |false|
// means either that |fd| is guarded by some other code, or more likely EBADF.
//
// Starting with 10.9, libdispatch began setting GUARD_DUP on a file descriptor.
// Unfortunately, it is spun up as part of +[NSApplication initialize], which is
// not really something that Chromium can avoid using on OSX.  See
// <http://crbug.com/338157>.  This function allows querying whether the file
// descriptor is guarded before attempting to close it.
bool CanGuardFd(int fd) {
  // Saves the original flags to reset later.
  int original_fdflags = 0;

  // This can be any value at all, it just has to match up between the two
  // calls.
  const guardid_t kGuard = 15;

  // Attempt to change the guard.  This can fail with EBADF if the file
  // descriptor is bad, or EINVAL if the fd already has a guard set.
  int ret =
      change_fdguard_np(fd, NULL, 0, &kGuard, GUARD_DUP, &original_fdflags);
  if (ret == -1)
    return false;

  // Remove the guard.  It should not be possible to fail in removing the guard
  // just added.
  ret = change_fdguard_np(fd, &kGuard, GUARD_DUP, NULL, 0, &original_fdflags);
  DPCHECK(ret == 0);

  return true;
}
#endif  // defined(OS_APPLE)

}  // namespace

MULTIPROCESS_TEST_MAIN(ProcessUtilsLeakFDChildProcess) {
  // This child process counts the number of open FDs, it then writes that
  // number out to a pipe connected to the parent.
  int num_open_files = 0;
  int write_pipe = kChildPipe;
  int max_files = GetMaxFilesOpenInProcess();
  for (int i = STDERR_FILENO + 1; i < max_files; i++) {
#if defined(OS_APPLE)
    // Ignore guarded or invalid file descriptors.
    if (!CanGuardFd(i))
      continue;
#endif

    if (i != kChildPipe) {
      int fd;
      if ((fd = HANDLE_EINTR(dup(i))) != -1) {
        close(fd);
        num_open_files += 1;
      }
    }
  }

  int written =
      HANDLE_EINTR(write(write_pipe, &num_open_files, sizeof(num_open_files)));
  DCHECK_EQ(static_cast<size_t>(written), sizeof(num_open_files));
  int ret = IGNORE_EINTR(close(write_pipe));
  DPCHECK(ret == 0);

  return 0;
}

int ProcessUtilTest::CountOpenFDsInChild() {
  int fds[2];
  if (pipe(fds) < 0)
    NOTREACHED();

  LaunchOptions options;
  options.fds_to_remap.emplace_back(fds[1], kChildPipe);
  Process process =
      SpawnChildWithOptions("ProcessUtilsLeakFDChildProcess", options);
  CHECK(process.IsValid());
  int ret = IGNORE_EINTR(close(fds[1]));
  DPCHECK(ret == 0);

  // Read number of open files in client process from pipe;
  int num_open_files = -1;
  ssize_t bytes_read =
      HANDLE_EINTR(read(fds[0], &num_open_files, sizeof(num_open_files)));
  CHECK_EQ(bytes_read, static_cast<ssize_t>(sizeof(num_open_files)));

#if defined(THREAD_SANITIZER)
  // Compiler-based ThreadSanitizer makes this test slow.
  TimeDelta timeout = TimeDelta::FromSeconds(3);
#else
  TimeDelta timeout = TimeDelta::FromSeconds(1);
#endif
  int exit_code;
  CHECK(process.WaitForExitWithTimeout(timeout, &exit_code));
  ret = IGNORE_EINTR(close(fds[0]));
  DPCHECK(ret == 0);

  return num_open_files;
}

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
// ProcessUtilTest.FDRemapping is flaky when ran under xvfb-run on Precise.
// The problem is 100% reproducible with both ASan and TSan.
// See http://crbug.com/136720.
#define MAYBE_FDRemapping DISABLED_FDRemapping
#else
#define MAYBE_FDRemapping FDRemapping
#endif  // defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
TEST_F(ProcessUtilTest, MAYBE_FDRemapping) {
  int fds_before = CountOpenFDsInChild();

  // open some dummy fds to make sure they don't propagate over to the
  // child process.
  int dev_null = open("/dev/null", O_RDONLY);
  DPCHECK(dev_null != -1);
  int sockets[2];
  int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
  DPCHECK(ret == 0);

  int fds_after = CountOpenFDsInChild();

  ASSERT_EQ(fds_after, fds_before);

  ret = IGNORE_EINTR(close(sockets[0]));
  DPCHECK(ret == 0);
  ret = IGNORE_EINTR(close(sockets[1]));
  DPCHECK(ret == 0);
  ret = IGNORE_EINTR(close(dev_null));
  DPCHECK(ret == 0);
}

const char kPipeValue = '\xcc';
MULTIPROCESS_TEST_MAIN(ProcessUtilsVerifyStdio) {
  // Write to stdio so the parent process can observe output.
  CHECK_EQ(1, HANDLE_EINTR(write(STDOUT_FILENO, &kPipeValue, 1)));

  // Close all of the handles, to verify they are valid.
  CHECK_EQ(0, IGNORE_EINTR(close(STDIN_FILENO)));
  CHECK_EQ(0, IGNORE_EINTR(close(STDOUT_FILENO)));
  CHECK_EQ(0, IGNORE_EINTR(close(STDERR_FILENO)));
  return 0;
}

TEST_F(ProcessUtilTest, FDRemappingIncludesStdio) {
  int dev_null = open("/dev/null", O_RDONLY);
  ASSERT_LT(2, dev_null);

  // Backup stdio and replace it with the write end of a pipe, for our
  // child process to inherit.
  int pipe_fds[2];
  int result = pipe(pipe_fds);
  ASSERT_EQ(0, result);
  int backup_stdio = HANDLE_EINTR(dup(STDOUT_FILENO));
  ASSERT_LE(0, backup_stdio);
  result = dup2(pipe_fds[1], STDOUT_FILENO);
  ASSERT_EQ(STDOUT_FILENO, result);

  // Launch the test process, which should inherit our pipe stdio.
  LaunchOptions options;
  options.fds_to_remap.emplace_back(dev_null, dev_null);
  Process process = SpawnChildWithOptions("ProcessUtilsVerifyStdio", options);
  ASSERT_TRUE(process.IsValid());

  // Restore stdio, so we can output stuff.
  result = dup2(backup_stdio, STDOUT_FILENO);
  ASSERT_EQ(STDOUT_FILENO, result);

  // Close our copy of the write end of the pipe, so that the read()
  // from the other end will see EOF if it wasn't copied to the child.
  result = IGNORE_EINTR(close(pipe_fds[1]));
  ASSERT_EQ(0, result);

  result = IGNORE_EINTR(close(backup_stdio));
  ASSERT_EQ(0, result);
  result = IGNORE_EINTR(close(dev_null));
  ASSERT_EQ(0, result);

  // Read from the pipe to verify that it is connected to the child
  // process' stdio.
  char buf[16] = {};
  EXPECT_EQ(1, HANDLE_EINTR(read(pipe_fds[0], buf, sizeof(buf))));
  EXPECT_EQ(kPipeValue, buf[0]);

  result = IGNORE_EINTR(close(pipe_fds[0]));
  ASSERT_EQ(0, result);

  int exit_code;
  ASSERT_TRUE(
      process.WaitForExitWithTimeout(TimeDelta::FromSeconds(5), &exit_code));
  EXPECT_EQ(0, exit_code);
}

#if defined(OS_FUCHSIA)

const uint16_t kStartupHandleId = 43;
MULTIPROCESS_TEST_MAIN(ProcessUtilsVerifyHandle) {
  zx_handle_t handle =
      zx_take_startup_handle(PA_HND(PA_USER0, kStartupHandleId));
  CHECK_NE(ZX_HANDLE_INVALID, handle);

  // Write to the pipe so the parent process can observe output.
  size_t bytes_written = 0;
  zx_status_t result = zx_socket_write(handle, 0, &kPipeValue,
                                       sizeof(kPipeValue), &bytes_written);
  CHECK_EQ(ZX_OK, result);
  CHECK_EQ(1u, bytes_written);

  CHECK_EQ(ZX_OK, zx_handle_close(handle));
  return 0;
}

TEST_F(ProcessUtilTest, LaunchWithHandleTransfer) {
  // Create a pipe to pass to the child process.
  zx_handle_t handles[2];
  zx_status_t result =
      zx_socket_create(ZX_SOCKET_STREAM, &handles[0], &handles[1]);
  ASSERT_EQ(ZX_OK, result);

  // Launch the test process, and pass it one end of the pipe.
  LaunchOptions options;
  options.handles_to_transfer.push_back(
      {PA_HND(PA_USER0, kStartupHandleId), handles[0]});
  Process process = SpawnChildWithOptions("ProcessUtilsVerifyHandle", options);
  ASSERT_TRUE(process.IsValid());

  // Read from the pipe to verify that the child received it.
  zx_signals_t signals = 0;
  result = zx_object_wait_one(
      handles[1], ZX_SOCKET_READABLE | ZX_SOCKET_PEER_CLOSED,
      (base::TimeTicks::Now() + TestTimeouts::action_timeout()).ToZxTime(),
      &signals);
  ASSERT_EQ(ZX_OK, result);
  ASSERT_TRUE(signals & ZX_SOCKET_READABLE);

  size_t bytes_read = 0;
  char buf[16] = {0};
  result = zx_socket_read(handles[1], 0, buf, sizeof(buf), &bytes_read);
  EXPECT_EQ(ZX_OK, result);
  EXPECT_EQ(1u, bytes_read);
  EXPECT_EQ(kPipeValue, buf[0]);

  CHECK_EQ(ZX_OK, zx_handle_close(handles[1]));

  int exit_code;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

#endif  // defined(OS_FUCHSIA)

// There's no such thing as a parent process id on Fuchsia.
#if !defined(OS_FUCHSIA)
TEST_F(ProcessUtilTest, GetParentProcessId) {
  ProcessId ppid = GetParentProcessId(GetCurrentProcessHandle());
  EXPECT_EQ(ppid, static_cast<ProcessId>(getppid()));
}
#endif  // !defined(OS_FUCHSIA)

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA) && !defined(OS_APPLE)
class WriteToPipeDelegate : public LaunchOptions::PreExecDelegate {
 public:
  explicit WriteToPipeDelegate(int fd) : fd_(fd) {}
  ~WriteToPipeDelegate() override = default;
  void RunAsyncSafe() override {
    RAW_CHECK(HANDLE_EINTR(write(fd_, &kPipeValue, 1)) == 1);
    RAW_CHECK(IGNORE_EINTR(close(fd_)) == 0);
  }

 private:
  int fd_;
  DISALLOW_COPY_AND_ASSIGN(WriteToPipeDelegate);
};

TEST_F(ProcessUtilTest, PreExecHook) {
  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));

  ScopedFD read_fd(pipe_fds[0]);
  ScopedFD write_fd(pipe_fds[1]);

  WriteToPipeDelegate write_to_pipe_delegate(write_fd.get());
  LaunchOptions options;
  options.fds_to_remap.emplace_back(write_fd.get(), write_fd.get());
  options.pre_exec_delegate = &write_to_pipe_delegate;
  Process process(SpawnChildWithOptions("SimpleChildProcess", options));
  ASSERT_TRUE(process.IsValid());

  write_fd.reset();
  char c;
  ASSERT_EQ(1, HANDLE_EINTR(read(read_fd.get(), &c, 1)));
  EXPECT_EQ(c, kPipeValue);

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);
}
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

namespace {

std::string TestLaunchProcess(const CommandLine& cmdline,
                              const EnvironmentMap& env_changes,
                              const bool clear_environment,
                              const int clone_flags) {
  LaunchOptions options;
  options.wait = true;
  options.environment = env_changes;
  options.clear_environment = clear_environment;

#if defined(OS_WIN)
  HANDLE read_handle, write_handle;
  PCHECK(CreatePipe(&read_handle, &write_handle, nullptr, 0));
  File read_pipe(read_handle);
  File write_pipe(write_handle);
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = write_handle;
  options.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  options.handles_to_inherit.push_back(write_handle);
#else
  int fds[2];
  PCHECK(pipe(fds) == 0);
  File read_pipe(fds[0]);
  File write_pipe(fds[1]);
  options.fds_to_remap.emplace_back(fds[1], STDOUT_FILENO);
#endif  // defined(OS_WIN)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  options.clone_flags = clone_flags;
#else
  CHECK_EQ(0, clone_flags);
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  EXPECT_TRUE(LaunchProcess(cmdline, options).IsValid());
  write_pipe.Close();

  char buf[512];
  int n = read_pipe.ReadAtCurrentPos(buf, sizeof(buf));
#if defined(OS_WIN)
  // Closed pipes fail with ERROR_BROKEN_PIPE on Windows, rather than
  // successfully reporting EOF.
  if (n < 0 && GetLastError() == ERROR_BROKEN_PIPE) {
    n = 0;
  }
#endif  // OS_WIN
  PCHECK(n >= 0);

  return std::string(buf, n);
}

const char kLargeString[] =
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789";

}  // namespace

TEST_F(ProcessUtilTest, LaunchProcess) {
  const int no_clone_flags = 0;
  const bool no_clear_environ = false;
  const FilePath::CharType kBaseTest[] = FILE_PATH_LITERAL("BASE_TEST");
  const CommandLine kPrintEnvCommand(CommandLine::StringVector(
      {test_helper_path_.value(), FILE_PATH_LITERAL("-e"), kBaseTest}));
  std::unique_ptr<Environment> env = Environment::Create();

  EnvironmentMap env_changes;
  env_changes[kBaseTest] = FILE_PATH_LITERAL("bar");
  EXPECT_EQ("bar", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                     no_clear_environ, no_clone_flags));
  env_changes.clear();

  EXPECT_TRUE(env->SetVar("BASE_TEST", "testing"));
  EXPECT_EQ("testing", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                         no_clear_environ, no_clone_flags));

  env_changes[kBaseTest] = FilePath::StringType();
  EXPECT_EQ("", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                  no_clear_environ, no_clone_flags));

  env_changes[kBaseTest] = FILE_PATH_LITERAL("foo");
  EXPECT_EQ("foo", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                     no_clear_environ, no_clone_flags));

  env_changes.clear();
  EXPECT_TRUE(env->SetVar("BASE_TEST", kLargeString));
  EXPECT_EQ(std::string(kLargeString),
            TestLaunchProcess(kPrintEnvCommand, env_changes, no_clear_environ,
                              no_clone_flags));

  env_changes[kBaseTest] = FILE_PATH_LITERAL("wibble");
  EXPECT_EQ("wibble", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                        no_clear_environ, no_clone_flags));

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Test a non-trival value for clone_flags.
  EXPECT_EQ("wibble", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                        no_clear_environ, CLONE_FS));
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  EXPECT_EQ("wibble",
            TestLaunchProcess(kPrintEnvCommand, env_changes,
                              true /* clear_environ */, no_clone_flags));
  env_changes.clear();
  EXPECT_EQ("", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                  true /* clear_environ */, no_clone_flags));
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
MULTIPROCESS_TEST_MAIN(CheckPidProcess) {
  const pid_t kInitPid = 1;
  const pid_t pid = syscall(__NR_getpid);
  CHECK(pid == kInitPid);
  CHECK(getpid() == pid);
  return kSuccess;
}

#if defined(CLONE_NEWUSER) && defined(CLONE_NEWPID)
TEST_F(ProcessUtilTest, CloneFlags) {
  if (!PathExists(FilePath("/proc/self/ns/user")) ||
      !PathExists(FilePath("/proc/self/ns/pid"))) {
    // User or PID namespaces are not supported.
    return;
  }

  LaunchOptions options;
  options.clone_flags = CLONE_NEWUSER | CLONE_NEWPID;

  Process process(SpawnChildWithOptions("CheckPidProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}
#endif  // defined(CLONE_NEWUSER) && defined(CLONE_NEWPID)

TEST(ForkWithFlagsTest, UpdatesPidCache) {
  // Warm up the libc pid cache, if there is one.
  ASSERT_EQ(syscall(__NR_getpid), getpid());

  pid_t ctid = 0;
  const pid_t pid = ForkWithFlags(SIGCHLD | CLONE_CHILD_SETTID, nullptr, &ctid);
  if (pid == 0) {
    // In child.  Check both the raw getpid syscall and the libc getpid wrapper
    // (which may rely on a pid cache).
    RAW_CHECK(syscall(__NR_getpid) == ctid);
    RAW_CHECK(getpid() == ctid);
    _exit(kSuccess);
  }

  ASSERT_NE(-1, pid);
  int status = 42;
  ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(kSuccess, WEXITSTATUS(status));
}

TEST_F(ProcessUtilTest, InvalidCurrentDirectory) {
  LaunchOptions options;
  options.current_directory = FilePath("/dev/null");

  Process process(SpawnChildWithOptions("SimpleChildProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = kSuccess;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_NE(kSuccess, exit_code);
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // namespace base
