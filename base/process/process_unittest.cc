// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/debug/invalid_access_win.h"
#include "base/process/kill.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <unistd.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "base/win/base_win_buildflags.h"

#include <windows.h>
#endif

namespace {

#if BUILDFLAG(IS_WIN)
constexpr int kExpectedStillRunningExitCode = 0x102;
#else
constexpr int kExpectedStillRunningExitCode = 0;
#endif

constexpr int kDummyExitCode = 42;

#if BUILDFLAG(IS_APPLE)
// Fake port provider that returns the calling process's
// task port, ignoring its argument.
class FakePortProvider : public base::PortProvider {
  mach_port_t TaskForPid(base::ProcessHandle process) const override {
    return mach_task_self();
  }
};
#endif

#if BUILDFLAG(IS_CHROMEOS)
const char kForeground[] = "/chrome_renderers/foreground";
const char kCgroupRoot[] = "/sys/fs/cgroup/cpu";
const char kFullRendererCgroupRoot[] = "/sys/fs/cgroup/cpu/chrome_renderers";
const char kProcPath[] = "/proc/%d/cgroup";

std::string GetProcessCpuCgroup(const base::Process& process) {
  std::string proc;
  if (!base::ReadFileToString(
          base::FilePath(base::StringPrintf(kProcPath, process.Pid())),
          &proc)) {
    return std::string();
  }

  std::vector<base::StringPiece> lines = SplitStringPiece(
      proc, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    std::vector<base::StringPiece> fields = SplitStringPiece(
        line, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (fields.size() != 3U) {
      continue;
    }

    if (fields[1] == "cpu") {
      return static_cast<std::string>(fields[2]);
    }
  }

  return std::string();
}

bool AddProcessToCpuCgroup(const base::Process& process,
                           const std::string& cgroup) {
  base::FilePath path(cgroup);
  path = path.Append("cgroup.procs");
  return base::WriteFile(path, base::NumberToString(process.Pid()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

namespace base {

class ProcessTest : public MultiProcessTest {
};

TEST_F(ProcessTest, Create) {
  Process process(SpawnChild("SimpleChildProcess"));
  ASSERT_TRUE(process.IsValid());
  ASSERT_FALSE(process.is_current());
  EXPECT_NE(process.Pid(), kNullProcessId);
  process.Close();
  ASSERT_FALSE(process.IsValid());
}

TEST_F(ProcessTest, CreateCurrent) {
  Process process = Process::Current();
  ASSERT_TRUE(process.IsValid());
  ASSERT_TRUE(process.is_current());
  EXPECT_NE(process.Pid(), kNullProcessId);
  process.Close();
  ASSERT_FALSE(process.IsValid());
}

TEST_F(ProcessTest, Move) {
  Process process1(SpawnChild("SimpleChildProcess"));
  EXPECT_TRUE(process1.IsValid());

  Process process2;
  EXPECT_FALSE(process2.IsValid());

  process2 = std::move(process1);
  EXPECT_TRUE(process2.IsValid());
  EXPECT_FALSE(process1.IsValid());
  EXPECT_FALSE(process2.is_current());

  Process process3 = Process::Current();
  process2 = std::move(process3);
  EXPECT_TRUE(process2.is_current());
  EXPECT_TRUE(process2.IsValid());
  EXPECT_FALSE(process3.IsValid());
}

TEST_F(ProcessTest, Duplicate) {
  Process process1(SpawnChild("SimpleChildProcess"));
  ASSERT_TRUE(process1.IsValid());

  Process process2 = process1.Duplicate();
  ASSERT_TRUE(process1.IsValid());
  ASSERT_TRUE(process2.IsValid());
  EXPECT_EQ(process1.Pid(), process2.Pid());
  EXPECT_FALSE(process1.is_current());
  EXPECT_FALSE(process2.is_current());

  process1.Close();
  ASSERT_TRUE(process2.IsValid());
}

TEST_F(ProcessTest, DuplicateCurrent) {
  Process process1 = Process::Current();
  ASSERT_TRUE(process1.IsValid());

  Process process2 = process1.Duplicate();
  ASSERT_TRUE(process1.IsValid());
  ASSERT_TRUE(process2.IsValid());
  EXPECT_EQ(process1.Pid(), process2.Pid());
  EXPECT_TRUE(process1.is_current());
  EXPECT_TRUE(process2.is_current());

  process1.Close();
  ASSERT_TRUE(process2.IsValid());
}

MULTIPROCESS_TEST_MAIN(SleepyChildProcess) {
  PlatformThread::Sleep(TestTimeouts::action_max_timeout());
  return 0;
}

// TODO(https://crbug.com/726484): Enable these tests on Fuchsia when
// CreationTime() is implemented.
TEST_F(ProcessTest, CreationTimeCurrentProcess) {
  // The current process creation time should be less than or equal to the
  // current time.
  EXPECT_FALSE(Process::Current().CreationTime().is_null());
  EXPECT_LE(Process::Current().CreationTime(), Time::Now());
}

#if !BUILDFLAG(IS_ANDROID)  // Cannot read other processes' creation time on
                            // Android.
TEST_F(ProcessTest, CreationTimeOtherProcess) {
  // The creation time of a process should be between a time recorded before it
  // was spawned and a time recorded after it was spawned. However, since the
  // base::Time and process creation clocks don't match, tolerate some error.
  constexpr base::TimeDelta kTolerance =
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // On Linux, process creation time is relative to boot time which has a
      // 1-second resolution. Tolerate 1 second for the imprecise boot time and
      // 100 ms for the imprecise clock.
      Milliseconds(1100);
#elif BUILDFLAG(IS_WIN)
      // On Windows, process creation time is based on the system clock while
      // Time::Now() is a combination of system clock and
      // QueryPerformanceCounter(). Tolerate 100 ms for the clock mismatch.
      Milliseconds(100);
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
      // On Mac and Fuchsia, process creation time should be very precise.
      Milliseconds(0);
#else
#error Unsupported platform
#endif
  const Time before_creation = Time::Now();
  Process process(SpawnChild("SleepyChildProcess"));
  const Time after_creation = Time::Now();
  const Time creation = process.CreationTime();
  EXPECT_LE(before_creation - kTolerance, creation);
  EXPECT_LE(creation, after_creation + kTolerance);
  EXPECT_TRUE(process.Terminate(kDummyExitCode, true));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ProcessTest, Terminate) {
  Process process(SpawnChild("SleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  int exit_code = kDummyExitCode;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  exit_code = kDummyExitCode;
  int kExpectedExitCode = 250;
  process.Terminate(kExpectedExitCode, false);
  process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                 &exit_code);

  EXPECT_NE(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
#if BUILDFLAG(IS_WIN)
  // Only Windows propagates the |exit_code| set in Terminate().
  EXPECT_EQ(kExpectedExitCode, exit_code);
#endif
}

void AtExitHandler(void*) {
  // At-exit handler should not be called at
  // Process::TerminateCurrentProcessImmediately.
  DCHECK(false);
}

class ThreadLocalObject {
  ~ThreadLocalObject() {
    // Thread-local storage should not be destructed at
    // Process::TerminateCurrentProcessImmediately.
    DCHECK(false);
  }
};

MULTIPROCESS_TEST_MAIN(TerminateCurrentProcessImmediatelyWithCode0) {
  base::ThreadLocalPointer<ThreadLocalObject> object;
  base::AtExitManager::RegisterCallback(&AtExitHandler, nullptr);
  Process::TerminateCurrentProcessImmediately(0);
}

TEST_F(ProcessTest, TerminateCurrentProcessImmediatelyWithZeroExitCode) {
  Process process(SpawnChild("TerminateCurrentProcessImmediatelyWithCode0"));
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(TerminateCurrentProcessImmediatelyWithCode250) {
  Process::TerminateCurrentProcessImmediately(250);
}

TEST_F(ProcessTest, TerminateCurrentProcessImmediatelyWithNonZeroExitCode) {
  Process process(SpawnChild("TerminateCurrentProcessImmediatelyWithCode250"));
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(250, exit_code);
}

MULTIPROCESS_TEST_MAIN(FastSleepyChildProcess) {
  PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 10);
  return 0;
}

TEST_F(ProcessTest, WaitForExit) {
  Process process(SpawnChild("FastSleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  int exit_code = kDummyExitCode;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);
}

TEST_F(ProcessTest, WaitForExitWithTimeout) {
  Process process(SpawnChild("SleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  int exit_code = kDummyExitCode;
  TimeDelta timeout = TestTimeouts::tiny_timeout();
  EXPECT_FALSE(process.WaitForExitWithTimeout(timeout, &exit_code));
  EXPECT_EQ(kDummyExitCode, exit_code);

  process.Terminate(kDummyExitCode, false);
}

#if BUILDFLAG(IS_WIN)
TEST_F(ProcessTest, WaitForExitOrEventWithProcessExit) {
  Process process(SpawnChild("FastSleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  base::win::ScopedHandle stop_watching_handle(
      CreateEvent(nullptr, TRUE, FALSE, nullptr));

  int exit_code = kDummyExitCode;
  EXPECT_EQ(process.WaitForExitOrEvent(stop_watching_handle, &exit_code),
            base::Process::WaitExitStatus::PROCESS_EXITED);
  EXPECT_EQ(0, exit_code);
}

TEST_F(ProcessTest, WaitForExitOrEventWithEventSet) {
  Process process(SpawnChild("SleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  base::win::ScopedHandle stop_watching_handle(
      CreateEvent(nullptr, TRUE, TRUE, nullptr));

  int exit_code = kDummyExitCode;
  EXPECT_EQ(process.WaitForExitOrEvent(stop_watching_handle, &exit_code),
            base::Process::WaitExitStatus::STOP_EVENT_SIGNALED);
  EXPECT_EQ(kDummyExitCode, exit_code);

  process.Terminate(kDummyExitCode, false);
}
#endif  // BUILDFLAG(IS_WIN)

// Ensure that the priority of a process is restored correctly after
// backgrounding and restoring.
// Note: a platform may not be willing or able to lower the priority of
// a process. The calls to SetProcessBackground should be noops then.
TEST_F(ProcessTest, SetProcessBackgrounded) {
  if (!Process::CanBackgroundProcesses())
    return;
  Process process(SpawnChild("SimpleChildProcess"));
  int old_priority = process.GetPriority();
#if BUILDFLAG(IS_APPLE)
  // On the Mac, backgrounding a process requires a port to that process.
  // In the browser it's available through the MachBroker class, which is not
  // part of base. Additionally, there is an indefinite amount of time between
  // spawning a process and receiving its port. Because this test just checks
  // the ability to background/foreground a process, we can use the current
  // process's port instead.
  FakePortProvider provider;
  EXPECT_TRUE(process.SetProcessBackgrounded(&provider, true));
  EXPECT_TRUE(process.IsProcessBackgrounded(&provider));
  EXPECT_TRUE(process.SetProcessBackgrounded(&provider, false));
  EXPECT_FALSE(process.IsProcessBackgrounded(&provider));

#else
  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());
  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
#endif
  int new_priority = process.GetPriority();
  EXPECT_EQ(old_priority, new_priority);
}

// Consumers can use WaitForExitWithTimeout(base::TimeDelta(), nullptr) to check
// whether the process is still running. This may not be safe because of the
// potential reusing of the process id. So we won't export Process::IsRunning()
// on all platforms. But for the controllable scenario in the test cases, the
// behavior should be guaranteed.
TEST_F(ProcessTest, CurrentProcessIsRunning) {
  EXPECT_FALSE(Process::Current().WaitForExitWithTimeout(
      base::TimeDelta(), nullptr));
}

#if BUILDFLAG(IS_APPLE)
// On Mac OSX, we can detect whether a non-child process is running.
TEST_F(ProcessTest, PredefinedProcessIsRunning) {
  // Process 1 is the /sbin/launchd, it should be always running.
  EXPECT_FALSE(Process::Open(1).WaitForExitWithTimeout(
      base::TimeDelta(), nullptr));
}
#endif

// Test is disabled on Windows AMR64 because
// TerminateWithHeapCorruption() isn't expected to work there.
// See: https://crbug.com/1054423
#if BUILDFLAG(IS_WIN)
#if defined(ARCH_CPU_ARM64)
#define MAYBE_HeapCorruption DISABLED_HeapCorruption
#else
#define MAYBE_HeapCorruption HeapCorruption
#endif
TEST_F(ProcessTest, MAYBE_HeapCorruption) {
  EXPECT_EXIT(base::debug::win::TerminateWithHeapCorruption(),
              ::testing::ExitedWithCode(STATUS_HEAP_CORRUPTION), "");
}

#if BUILDFLAG(WIN_ENABLE_CFG_GUARDS)
#define MAYBE_ControlFlowViolation ControlFlowViolation
#else
#define MAYBE_ControlFlowViolation DISABLED_ControlFlowViolation
#endif
TEST_F(ProcessTest, MAYBE_ControlFlowViolation) {
  // CFG causes ntdll!RtlFailFast2 to be called resulting in uncatchable
  // 0xC0000409 (STATUS_STACK_BUFFER_OVERRUN) exception.
  EXPECT_EXIT(base::debug::win::TerminateWithControlFlowViolation(),
              ::testing::ExitedWithCode(STATUS_STACK_BUFFER_OVERRUN), "");
}

#endif  // BUILDFLAG(IS_WIN)

TEST_F(ProcessTest, ChildProcessIsRunning) {
  Process process(SpawnChild("SleepyChildProcess"));
  EXPECT_FALSE(process.WaitForExitWithTimeout(
      base::TimeDelta(), nullptr));
  process.Terminate(0, true);
  EXPECT_TRUE(process.WaitForExitWithTimeout(
      base::TimeDelta(), nullptr));
}

#if BUILDFLAG(IS_CHROMEOS)

// Tests that the function IsProcessBackgroundedCGroup() can parse the contents
// of the /proc/<pid>/cgroup file successfully.
TEST_F(ProcessTest, TestIsProcessBackgroundedCGroup) {
  const char kNotBackgrounded[] = "5:cpuacct,cpu,cpuset:/daemons\n";
  const char kBackgrounded[] =
      "2:freezer:/chrome_renderers/to_be_frozen\n"
      "1:cpu:/chrome_renderers/background\n";

  EXPECT_FALSE(IsProcessBackgroundedCGroup(kNotBackgrounded));
  EXPECT_TRUE(IsProcessBackgroundedCGroup(kBackgrounded));
}

TEST_F(ProcessTest, InitializePriorityEmptyProcess) {
  // TODO(b/172213843): base::Process is used by base::TestSuite::Initialize
  // before we can use ScopedFeatureList here. Update the test to allow the
  // use of ScopedFeatureList before base::TestSuite::Initialize runs.
  if (!Process::OneGroupPerRendererEnabledForTesting())
    return;

  Process process;
  process.InitializePriority();
  const std::string unique_token = process.unique_token();
  ASSERT_TRUE(unique_token.empty());
}

TEST_F(ProcessTest, SetProcessBackgroundedOneCgroupPerRender) {
  if (!Process::OneGroupPerRendererEnabledForTesting())
    return;

  base::test::TaskEnvironment task_env;

  Process process(SpawnChild("SimpleChildProcess"));
  process.InitializePriority();
  const std::string unique_token = process.unique_token();
  ASSERT_FALSE(unique_token.empty());

  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
  std::string cgroup = GetProcessCpuCgroup(process);
  EXPECT_FALSE(cgroup.empty());
  EXPECT_NE(cgroup.find(unique_token), std::string::npos);

  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());

  EXPECT_TRUE(process.Terminate(0, false));
  // Terminate should post a task, wait for it to run
  task_env.RunUntilIdle();

  cgroup = std::string(kCgroupRoot) + cgroup;
  EXPECT_FALSE(base::DirectoryExists(FilePath(cgroup)));
}

TEST_F(ProcessTest, CleanUpBusyProcess) {
  if (!Process::OneGroupPerRendererEnabledForTesting())
    return;

  base::test::TaskEnvironment task_env;

  Process process(SpawnChild("SimpleChildProcess"));
  process.InitializePriority();
  const std::string unique_token = process.unique_token();
  ASSERT_FALSE(unique_token.empty());

  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
  std::string cgroup = GetProcessCpuCgroup(process);
  EXPECT_FALSE(cgroup.empty());
  EXPECT_NE(cgroup.find(unique_token), std::string::npos);

  // Add another process to the cgroup to ensure it stays busy.
  cgroup = std::string(kCgroupRoot) + cgroup;
  Process process2(SpawnChild("SimpleChildProcess"));
  EXPECT_TRUE(AddProcessToCpuCgroup(process2, cgroup));

  // Terminate the first process that should tirgger a cleanup of the cgroup
  EXPECT_TRUE(process.Terminate(0, false));
  // Wait until the background task runs once. This should fail and requeue
  // another task to retry.
  task_env.RunUntilIdle();
  EXPECT_TRUE(base::DirectoryExists(FilePath(cgroup)));

  // Move the second process to free the cgroup
  std::string foreground_path =
      std::string(kCgroupRoot) + std::string(kForeground);
  EXPECT_TRUE(AddProcessToCpuCgroup(process2, foreground_path));

  // Wait for the retry.
  PlatformThread::Sleep(base::Milliseconds(1100));
  task_env.RunUntilIdle();
  // The cgroup should be deleted now.
  EXPECT_FALSE(base::DirectoryExists(FilePath(cgroup)));

  process2.Terminate(0, false);
}

TEST_F(ProcessTest, SetProcessBackgroundedEmptyToken) {
  if (!Process::OneGroupPerRendererEnabledForTesting())
    return;

  Process process(SpawnChild("SimpleChildProcess"));
  const std::string unique_token = process.unique_token();
  ASSERT_TRUE(unique_token.empty());

  // Moving to the foreground should use the default foregorund path
  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
  std::string cgroup = GetProcessCpuCgroup(process);
  EXPECT_FALSE(cgroup.empty());
  EXPECT_EQ(cgroup, kForeground);
}

TEST_F(ProcessTest, CleansUpStaleGroups) {
  if (!Process::OneGroupPerRendererEnabledForTesting())
    return;

  base::test::TaskEnvironment task_env;

  // Create a process that will not be cleaned up
  Process process(SpawnChild("SimpleChildProcess"));
  process.InitializePriority();
  const std::string unique_token = process.unique_token();
  ASSERT_FALSE(unique_token.empty());

  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());

  // Create a stale cgroup
  std::string root = kFullRendererCgroupRoot;
  std::string cgroup = root + "/" + unique_token;
  std::vector<std::string> tokens = base::SplitString(
      cgroup, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  tokens[1] = "fake";
  std::string fake_cgroup = base::JoinString(tokens, "-");
  EXPECT_TRUE(base::CreateDirectory(FilePath(fake_cgroup)));

  // Clean up stale groups
  Process::CleanUpStaleProcessStates();

  // validate the fake group is deleted
  EXPECT_FALSE(base::DirectoryExists(FilePath(fake_cgroup)));

  // validate the active process cgroup is not deleted
  EXPECT_TRUE(base::DirectoryExists(FilePath(cgroup)));

  // validate foreground and background are not deleted
  EXPECT_TRUE(base::DirectoryExists(FilePath(root + "/foreground")));
  EXPECT_TRUE(base::DirectoryExists(FilePath(root + "/background")));

  // clean up the process
  EXPECT_TRUE(process.Terminate(0, false));
  // Terminate should post a task, wait for it to run
  task_env.RunUntilIdle();
  EXPECT_FALSE(base::DirectoryExists(FilePath(cgroup)));
}

TEST_F(ProcessTest, OneCgroupDoesNotCleanUpGroupsWithWrongPrefix) {
  if (!Process::OneGroupPerRendererEnabledForTesting())
    return;

  base::test::TaskEnvironment task_env;

  // Create a process that will not be cleaned up
  Process process(SpawnChild("SimpleChildProcess"));
  process.InitializePriority();
  const std::string unique_token = process.unique_token();
  ASSERT_FALSE(unique_token.empty());

  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
  std::string cgroup = GetProcessCpuCgroup(process);
  EXPECT_FALSE(cgroup.empty());
  EXPECT_NE(cgroup.find(unique_token), std::string::npos);

  // Create a stale cgroup
  FilePath cgroup_path = FilePath(std::string(kCgroupRoot) + cgroup);
  FilePath fake_cgroup = FilePath(kFullRendererCgroupRoot).AppendASCII("fake");
  EXPECT_TRUE(base::CreateDirectory(fake_cgroup));

  // Clean up stale groups
  Process::CleanUpStaleProcessStates();

  // validate the fake group is deleted
  EXPECT_TRUE(base::DirectoryExists(fake_cgroup));
  EXPECT_TRUE(base::DirectoryExists(cgroup_path));

  // clean up the process
  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());
  EXPECT_TRUE(process.Terminate(0, false));
  task_env.RunUntilIdle();
  EXPECT_FALSE(base::DirectoryExists(cgroup_path));
  base::DeleteFile(fake_cgroup);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace base
