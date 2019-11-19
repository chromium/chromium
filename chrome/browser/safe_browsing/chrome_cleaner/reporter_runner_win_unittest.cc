// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/win_util.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

namespace safe_browsing {
namespace {

constexpr char kParentHandleSwitch[] = "parent-handle";

std::string LastSystemErrorString() {
  return logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());
}

// Duplicates |src_handle| and gives |target_process| access to it.
//
// To give a process that doesn't exist yet access to the handle, use the
// current process as |target_process| and add the resulting handle to
// LaunchOptions::handles_to_inherit_vector.
HANDLE DuplicateHandleIntoProcess(const base::Process& target_process,
                                  HANDLE src_handle) {
  HANDLE target_handle;
  if (!::DuplicateHandle(/*src_process=*/::GetCurrentProcess(), src_handle,
                         target_process.Handle(), &target_handle,
                         /*desired_access=*/0, /*inherit_handle=*/true,
                         /*flags=*/DUPLICATE_SAME_ACCESS)) {
    return INVALID_HANDLE_VALUE;
  }
  return target_handle;
}

// Allows a test child to spawn a test grandchild without endless recursion.
base::CommandLine GetNestedTestChildBaseCommandLine() {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();

  // Since we are already be in a child process, remove the test child process
  // switch. It will be re-added with the grandchild test name by
  // SpawnMultiprocessTestChild.
  command_line.RemoveSwitch(switches::kTestChildProcess);
  return command_line;
}

// Gives the parent process access to |grandchild|'s process handle. Called in
// the child process.
void GiveGrandchildHandleToParent(const base::Process& grandchild) {
  // Get the parent process handle from the commandline.
  unsigned int raw_handle;
  CHECK(base::StringToUint(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kParentHandleSwitch),
      &raw_handle));
  base::Process parent(base::win::Uint32ToHandle(raw_handle));

  // Give the parent access to the grandchild process handle and write it to
  // stdout.
  base::File write_pipe(::GetStdHandle(STD_OUTPUT_HANDLE));
  uint32_t grandchild_handle = base::win::HandleToUint32(
      DuplicateHandleIntoProcess(parent, grandchild.Handle()));
  constexpr int handle_size = sizeof(grandchild_handle);
  CHECK_EQ(write_pipe.WriteAtCurrentPos(
               reinterpret_cast<char*>(&grandchild_handle), handle_size),
           handle_size);
}

// Reporter delegate that implements LaunchReporterProcess to spawn a
// multiprocess test child.
class LaunchReporterDelegate : public internal::SwReporterTestingDelegate {
 public:
  LaunchReporterDelegate() = default;

  ~LaunchReporterDelegate() override = default;

  base::Process LaunchReporterProcess(
      const SwReporterInvocation& invocation,
      const base::LaunchOptions& options) override {
    // Launch a multiprocess test child, getting the name from the invocation
    // commandline.
    base::Process grandchild = base::SpawnMultiProcessTestChild(
        invocation.command_line().GetProgram().AsUTF8Unsafe(),
        GetNestedTestChildBaseCommandLine(), options);
    GiveGrandchildHandleToParent(grandchild);
    return grandchild;
  }

  int WaitForReporterExit(const base::Process& process) const override {
    int exit_code;
    process.WaitForExit(&exit_code);
    return exit_code;
  }

  base::Time Now() const override {
    NOTREACHED();
    return base::Time::Now();
  }

  base::TaskRunner* BlockingTaskRunner() const override {
    NOTREACHED();
    return nullptr;
  }

  ChromeCleanerController* GetCleanerController() override {
    NOTREACHED();
    return nullptr;
  }

  void CreateChromeCleanerDialogController() override { NOTREACHED(); }
};

// Spawns a test child process that runs |child_function|. This process must
// spawn a grandchild process and write its handle to stdout.
//
// On success |child| will be set to the child process object and |grandchild|
// will be set to the grandchild process object. Both of these processes will
// be valid and running.
AssertionResult CreateRunningProcesseses(const std::string& child_function,
                                         base::Process* child,
                                         base::Process* grandchild) {
  base::CommandLine child_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions options;

  // Give the child a handle to the current process. It will need this to pass
  // the grandchild handle back to us.

  // ::GetCurrentProcess returns a pseudo-handle that always refers to the
  // current process. Duplicate it to get a real handle to pass to the child.
  HANDLE pseudo_handle = ::GetCurrentProcess();
  HANDLE parent_handle =
      DuplicateHandleIntoProcess(base::Process::Current(), pseudo_handle);
  if (parent_handle == INVALID_HANDLE_VALUE) {
    return AssertionFailure()
           << "Failed to duplicate parent handle: " << LastSystemErrorString();
  }

  child_command_line.AppendSwitchASCII(
      kParentHandleSwitch,
      base::NumberToString(base::win::HandleToUint32(parent_handle)));
  options.handles_to_inherit.push_back(parent_handle);

  // Create a pipe to read the grandchild handle back from the child.
  // (Adapted from
  // https://cs.chromium.org/chromium/src/base/process/process_util_unittest.cc?rcl=51b17c51acc7dbf5fb812371d5724b2564578661&l=1294)
  HANDLE read_handle, write_handle;
  if (!::CreatePipe(&read_handle, &write_handle, nullptr, 0)) {
    return AssertionFailure()
           << "Failed to create pipe: " << LastSystemErrorString();
  }
  base::File read_pipe(read_handle);
  base::File write_pipe(write_handle);
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = write_handle;
  options.stderr_handle = ::GetStdHandle(STD_ERROR_HANDLE);
  options.handles_to_inherit.push_back(write_handle);

  *child = base::SpawnMultiProcessTestChild(child_function, child_command_line,
                                            options);
  if (!child->IsValid()) {
    return AssertionFailure() << "Failed to spawn child process";
  }

  // Read the grandchild handle from the child.
  uint32_t grandchild_handle;
  constexpr int handle_size = sizeof(grandchild_handle);
  int bytes_read = read_pipe.ReadAtCurrentPos(
      reinterpret_cast<char*>(&grandchild_handle), handle_size);
  if (bytes_read != handle_size) {
    return AssertionFailure() << "Failure reading grandchild handle. Expected "
                              << handle_size << " bytes, read " << bytes_read;
  }
  *grandchild = base::Process(base::win::Uint32ToHandle(grandchild_handle));
  if (!grandchild->IsValid()) {
    return AssertionFailure()
           << "Child wrote invalid grandchild handle: " << grandchild_handle;
  }

  // All the subprocesses should now be running. Wait a moment before verifying
  // this since if something went wrong it will take a few msec for them exit.
  base::WaitableEvent pause;
  pause.TimedWait(TestTimeouts::tiny_timeout());

  if (!child->IsRunning()) {
    return AssertionFailure() << "Child process died unexpectedly.";
  }
  if (!grandchild->IsRunning()) {
    return AssertionFailure() << "Grandchild process died before child did.";
  }
  return AssertionSuccess();
}

void WaitUntilSlain() {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  run_loop.Run();
}

// Main function for the grandchild process in all tests.
MULTIPROCESS_TEST_MAIN(InfiniteSubprocess) {
  WaitUntilSlain();
  return 0;
}

// ValidateDefaultBehaviour test
//
// The TerminateWhileRunning test assumes that, when a child process exits, the
// test framework doesn't immediately kill its subprocesses. This makes sure
// that assumption continues to be true.

// Main function for the child process.
MULTIPROCESS_TEST_MAIN(DefaultBehaviourChild) {
  base::LaunchOptions options;
  base::Process grandchild = base::SpawnMultiProcessTestChild(
      "InfiniteSubprocess", GetNestedTestChildBaseCommandLine(), options);
  GiveGrandchildHandleToParent(grandchild);
  WaitUntilSlain();
  return 0;
}

// Parent process.
TEST(ReporterRunnerLaunchTest, ValidateDefaultBehaviour) {
  if (!safe_browsing::internal::ReporterTerminatesOnBrowserExit()) {
    // No point testing the default behaviour if we won't run the
    // TerminateWhileRunning test.
    return;
  }

  base::Process child, grandchild;
  ASSERT_TRUE(
      CreateRunningProcesseses("DefaultBehaviourChild", &child, &grandchild));

  ASSERT_TRUE(child.Terminate(/*exit_code=*/0, /*wait=*/true));

  // Expect the grandchild process is still running even though the child is
  // not. Wait a moment before checking this because it will take a few msec
  // for it to exit.
  base::WaitableEvent pause;
  pause.TimedWait(TestTimeouts::tiny_timeout());
  EXPECT_TRUE(grandchild.IsRunning());

  grandchild.Terminate(/*exit_code=*/0, /*wait=*/false);
}

// TerminateWhileRunning test
//
// This makes sure that the software reporter process doesn't outlive its
// parent if the parent is terminated while the reporter is still running.

// Main function for the child process.
MULTIPROCESS_TEST_MAIN(SwReporterChild) {
  LaunchReporterDelegate delegate;
  SetSwReporterTestingDelegate(&delegate);

  // Stuff the multiprocess test name for the grandchild into a CommandLine
  // object by pretending it's a program path.
  const SwReporterInvocation invocation(base::CommandLine(
      base::FilePath(FILE_PATH_LITERAL("InfiniteSubprocess"))));
  return safe_browsing::internal::LaunchAndWaitForExit(invocation);
}

// Parent process.
TEST(ReporterRunnerLaunchTest, TerminateWhileRunning) {
  if (!safe_browsing::internal::ReporterTerminatesOnBrowserExit()) {
    // Skip the test since the code being tested isn't enabled in this
    // configuration.
    return;
  }

  base::Process child, grandchild;
  ASSERT_TRUE(CreateRunningProcesseses("SwReporterChild", &child, &grandchild));

  ASSERT_TRUE(child.Terminate(/*exit_code=*/0, /*wait=*/true));

  // The child process called safe_browsing::internal::LaunchReporter which
  // should set things up so that the grandchild is automatically killed when
  // the child exits. Wait a moment before checking this because it will take a
  // few msec for it to exit.
  base::WaitableEvent pause;
  pause.TimedWait(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(grandchild.IsRunning());
}

}  // namespace
}  // namespace safe_browsing
