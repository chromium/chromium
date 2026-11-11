// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher_fork_server.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {
namespace {

constexpr char kChildActionSwitch[] = "fork-server-test-action";
constexpr char kChildExitCodeSwitch[] = "fork-server-test-exit-code";
constexpr char kChildCustomArgSwitch[] = "fork-server-test-custom-arg";
constexpr char kTestEnvVarName[] = "FORK_SERVER_UNITTEST_ENV";

class FakeTestLauncherDelegate : public TestLauncherDelegate {
 public:
  bool GetTests(std::vector<TestIdentifier>* output) override { return true; }

  CommandLine GetCommandLine(const std::vector<std::string>& test_names,
                             const FilePath& temp_dir,
                             FilePath* output_file) override {
    return CommandLine(CommandLine::ForCurrentProcess()->GetProgram());
  }

  void OnTestTimedOut(const CommandLine& cmd_line) override {
    ++timed_out_count_;
    last_timed_out_cmd_line_ = cmd_line;
    if (live_processes_lock_ && live_processes_) {
      AutoLock lock(*live_processes_lock_);
      live_processes_count_on_timeout_ = live_processes_->size();
      if (!live_processes_->empty()) {
        timed_out_pid_ = live_processes_->begin()->first;
      }
    }
  }

  std::string GetWrapper() override { return wrapper_; }
  int GetLaunchOptions() override { return 0; }
  TimeDelta GetTimeout() override { return TestTimeouts::action_timeout(); }
  size_t GetBatchSize() override { return 1; }

  void set_wrapper(std::string wrapper) { wrapper_ = std::move(wrapper); }
  void set_live_processes_for_inspection(
      Lock* lock,
      absl::flat_hash_map<ProcessHandle, CommandLine>* live_processes) {
    live_processes_lock_ = lock;
    live_processes_ = live_processes;
  }

  int timed_out_count() const { return timed_out_count_; }
  const CommandLine& last_timed_out_cmd_line() const {
    return last_timed_out_cmd_line_;
  }
  size_t live_processes_count_on_timeout() const {
    return live_processes_count_on_timeout_;
  }
  ProcessHandle timed_out_pid() const { return timed_out_pid_; }

 private:
  std::string wrapper_;
  int timed_out_count_ = 0;
  CommandLine last_timed_out_cmd_line_{CommandLine::NO_PROGRAM};
  raw_ptr<Lock> live_processes_lock_ = nullptr;
  raw_ptr<absl::flat_hash_map<ProcessHandle, CommandLine>> live_processes_ =
      nullptr;
  size_t live_processes_count_on_timeout_ = 0;
  ProcessHandle timed_out_pid_ = kNullProcessHandle;
};

int RunForkedChildProcess(int argc, char** argv) {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  const std::string action = cmd_line->GetSwitchValueASCII(kChildActionSwitch);
  if (action == "exit-code") {
    int code = 0;
    if (!StringToInt(cmd_line->GetSwitchValueASCII(kChildExitCodeSwitch),
                     &code)) {
      return 99;
    }
    if (argc < 1 || !argv) {
      return 98;
    }
    struct sigaction sa = {};
    if (sigaction(SIGHUP, nullptr, &sa) != 0 || sa.sa_handler != SIG_DFL) {
      return 97;
    }
    return code;
  }
  if (action == "signal") {
    kill(getpid(), SIGTERM);
    return 1;
  }
  if (action == "verify-options") {
    FilePath cwd;
    if (!GetCurrentDirectory(&cwd)) {
      return 2;
    }
    const char* env_val = getenv(kTestEnvVarName);
    std::string out =
        "arg=" + cmd_line->GetSwitchValueASCII(kChildCustomArgSwitch) +
        ";env=" + (env_val ? env_val : "") + ";cwd=" + cwd.value() + "\n";
    if (!WriteFileDescriptor(STDOUT_FILENO, out) ||
        !WriteFileDescriptor(STDERR_FILENO, "stderr-ok\n")) {
      return 4;
    }
    return 0;
  }
  if (action == "spawn-grandchild" ||
      action == "spawn-grandchild-and-kill-waiter" ||
      action == "spawn-grandchild-and-hang") {
    pid_t grandchild = fork();
    if (grandchild < 0) {
      return 3;
    }
    if (grandchild == 0) {
      pause();
      _exit(0);
    }
    std::string pid_str = NumberToString(grandchild) + "\n";
    if (!WriteFileDescriptor(STDOUT_FILENO, pid_str)) {
      return 5;
    }
    if (action == "spawn-grandchild-and-kill-waiter") {
      kill(getppid(), SIGKILL);
      pause();
    } else if (action == "spawn-grandchild-and-hang") {
      pause();
    }
    return 0;
  }
  if (action == "report-fork-server-pid") {
    std::string stat_contents;
    if (!ReadFileToString(
            FilePath("/proc/" + NumberToString(getppid()) + "/stat"),
            &stat_contents)) {
      return 6;
    }
    size_t close_paren = stat_contents.rfind(')');
    if (close_paren == std::string::npos ||
        close_paren + 4 >= stat_contents.size()) {
      return 7;
    }
    std::string_view after_state =
        std::string_view(stat_contents).substr(close_paren + 4);
    size_t space_pos = after_state.find(' ');
    if (space_pos == std::string_view::npos) {
      return 8;
    }
    std::string out = std::string(after_state.substr(0, space_pos)) + "\n";
    if (!WriteFileDescriptor(STDOUT_FILENO, out)) {
      return 9;
    }
    return 0;
  }
  if (action == "verify-sharding-cleared") {
    if (getenv("GTEST_TOTAL_SHARDS") != nullptr ||
        getenv("GTEST_SHARD_INDEX") != nullptr) {
      return 10;
    }
    if (GTEST_FLAG_GET(total_shards) != -1 ||
        GTEST_FLAG_GET(shard_index) != -1) {
      return 11;
    }
    return 0;
  }
  if (action == "hang") {
    pause();
    return 0;
  }
  return 100;
}

std::string ReadAllFromFd(int fd) {
  std::string result;
  std::array<char, 256> buf;
  for (;;) {
    ssize_t n = HANDLE_EINTR(read(fd, buf.data(), buf.size()));
    if (n <= 0) {
      break;
    }
    result.append(std::string_view(buf.data(), static_cast<size_t>(n)));
  }
  return result;
}

std::string ReadLineFromFd(int fd) {
  std::string line;
  const TimeTicks deadline = TimeTicks::Now() + TestTimeouts::action_timeout();
  for (TimeTicks now = TimeTicks::Now(); now < deadline;
       now = TimeTicks::Now()) {
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    struct timespec ts = (deadline - now).ToTimeSpec();
    int poll_res = ppoll(&pfd, 1, &ts, nullptr);
    if (poll_res == 0 || (poll_res < 0 && errno != EINTR)) {
      break;
    }
    if (poll_res < 0) {
      continue;
    }
    char c = '\0';
    ssize_t n = HANDLE_EINTR(read(fd, &c, 1));
    if (n <= 0 || c == '\n') {
      break;
    }
    line.push_back(c);
  }
  return line;
}

bool IsProcessDeadOrZombie(pid_t pid) {
  if (kill(pid, 0) != 0 && errno == ESRCH) {
    return true;
  }
  std::string stat_contents;
  if (!ReadFileToString(FilePath("/proc/" + NumberToString(pid) + "/stat"),
                        &stat_contents)) {
    return true;
  }
  size_t close_paren = stat_contents.rfind(')');
  return close_paren != std::string::npos &&
         close_paren + 2 < stat_contents.size() &&
         stat_contents[close_paren + 2] == 'Z';
}

bool WaitForProcessDeadOrZombie(pid_t pid) {
  const TimeTicks deadline = TimeTicks::Now() + TestTimeouts::action_timeout();
  while (TimeTicks::Now() < deadline) {
    if (IsProcessDeadOrZombie(pid)) {
      return true;
    }
    PlatformThread::Sleep(Milliseconds(5));
  }
  return IsProcessDeadOrZombie(pid);
}

class TestLauncherForkServerTest : public testing::Test {
 protected:
  void SetUp() override { StopTestLauncherForkServer(); }

  void TearDown() override { StopTestLauncherForkServer(); }

  void StartForkServer() {
    int argc = 1;
    std::string prog_name = "base_unittests";
    std::array<char*, 2> argv_storage = {prog_name.data(), nullptr};
    char** argv = argv_storage.data();
    if (MaybeStartTestLauncherForkServer(&argc, &argv,
                                         /*force_for_testing=*/true)) {
      _exit(RunForkedChildProcess(argc, argv));
    }
  }

  CommandLine CreateChildCommandLine(const std::string& action) {
    CommandLine cmd(CommandLine::ForCurrentProcess()->GetProgram());
    cmd.AppendSwitchASCII(kChildActionSwitch, action);
    return cmd;
  }

  LaunchOptions CreateStdioRedirectOptions(int output_fd) {
    LaunchOptions options;
    options.fds_to_remap.emplace_back(output_fd, STDOUT_FILENO);
    options.fds_to_remap.emplace_back(output_fd, STDERR_FILENO);
    return options;
  }

  Lock live_processes_lock_;
  absl::flat_hash_map<ProcessHandle, CommandLine> live_processes_;
  FakeTestLauncherDelegate delegate_;
};

TEST_F(TestLauncherForkServerTest, CanUseTestLauncherForkServerChecks) {
  CommandLine same_program_cmd(CommandLine::ForCurrentProcess()->GetProgram());
  EXPECT_FALSE(CanUseTestLauncherForkServer(same_program_cmd, 1, &delegate_));

  StartForkServer();
  EXPECT_TRUE(CanUseTestLauncherForkServer(same_program_cmd, 1, &delegate_));
  EXPECT_TRUE(CanUseTestLauncherForkServer(same_program_cmd, 1, nullptr));

  // Batch size != 1 should not use the fork server.
  EXPECT_FALSE(CanUseTestLauncherForkServer(same_program_cmd, 2, &delegate_));

  // Different program binary should not use the fork server.
  CommandLine other_program_cmd(FilePath("/bin/true"));
  EXPECT_FALSE(CanUseTestLauncherForkServer(other_program_cmd, 1, &delegate_));

  // Non-empty command wrapper should not use the fork server.
  delegate_.set_wrapper("/usr/bin/env");
  EXPECT_FALSE(CanUseTestLauncherForkServer(same_program_cmd, 1, &delegate_));
  delegate_.set_wrapper("");

  // Shutdown notification disables new launches and returns -1 rather than
  // falling back to `LaunchProcess()`.
  NotifyTestLauncherForkServerShutdown();
  EXPECT_FALSE(CanUseTestLauncherForkServer(same_program_cmd, 1, &delegate_));
  bool was_timeout = false;
  std::optional<int> shutdown_exit = LaunchChildTestProcessViaForkServer(
      same_program_cmd, LaunchOptions(), TestTimeouts::action_timeout(),
      &delegate_, &was_timeout, &live_processes_lock_, &live_processes_);
  ASSERT_TRUE(shutdown_exit.has_value());
  EXPECT_EQ(-1, *shutdown_exit);

  StopTestLauncherForkServer();
  EXPECT_FALSE(CanUseTestLauncherForkServer(same_program_cmd, 1, &delegate_));
}

TEST_F(TestLauncherForkServerTest, ExitCodeAndSignalPropagation) {
  StartForkServer();

  // Child exiting with 0.
  {
    CommandLine cmd = CreateChildCommandLine("exit-code");
    cmd.AppendSwitchASCII(kChildExitCodeSwitch, "0");
    bool was_timeout = false;
    std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
        cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
        &was_timeout, &live_processes_lock_, &live_processes_);
    ASSERT_TRUE(exit_code.has_value());
    EXPECT_EQ(0, *exit_code);
    EXPECT_FALSE(was_timeout);
    EXPECT_TRUE(live_processes_.empty());
  }

  // Child exiting with non-zero status code (42).
  {
    CommandLine cmd = CreateChildCommandLine("exit-code");
    cmd.AppendSwitchASCII(kChildExitCodeSwitch, "42");
    bool was_timeout = false;
    std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
        cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
        &was_timeout, &live_processes_lock_, &live_processes_);
    ASSERT_TRUE(exit_code.has_value());
    EXPECT_EQ(42, *exit_code);
    EXPECT_FALSE(was_timeout);
    EXPECT_TRUE(live_processes_.empty());
  }

  // Child terminated by signal returns -1.
  {
    CommandLine cmd = CreateChildCommandLine("signal");
    bool was_timeout = false;
    std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
        cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
        &was_timeout, &live_processes_lock_, &live_processes_);
    ASSERT_TRUE(exit_code.has_value());
    EXPECT_EQ(-1, *exit_code);
    EXPECT_FALSE(was_timeout);
    EXPECT_TRUE(live_processes_.empty());
  }
}

TEST_F(TestLauncherForkServerTest, OptionsAndOutputRedirection) {
  StartForkServer();

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  int output_pipe[2] = {-1, -1};
  ASSERT_EQ(0, pipe2(output_pipe, O_CLOEXEC));
  ScopedFD output_read(output_pipe[0]);
  ScopedFD output_write(output_pipe[1]);

  CommandLine cmd = CreateChildCommandLine("verify-options");
  cmd.AppendSwitchASCII(kChildCustomArgSwitch, "custom_value_123");

  LaunchOptions options = CreateStdioRedirectOptions(output_write.get());
  options.current_directory = temp_dir.GetPath();
  options.environment[kTestEnvVarName] = "env_value_456";

  bool was_timeout = false;
  std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
      cmd, options, TestTimeouts::action_timeout(), &delegate_, &was_timeout,
      &live_processes_lock_, &live_processes_);
  output_write.reset();

  ASSERT_TRUE(exit_code.has_value());
  EXPECT_EQ(0, *exit_code);
  EXPECT_FALSE(was_timeout);

  std::string output = ReadAllFromFd(output_read.get());
  std::string expected_stdout = "arg=custom_value_123;env=env_value_456;cwd=" +
                                temp_dir.GetPath().value() + "\n";
  EXPECT_EQ(expected_stdout + "stderr-ok\n", output);
}

TEST_F(TestLauncherForkServerTest, KillsOrphanedProcessGroupMembers) {
  StartForkServer();

  int output_pipe[2] = {-1, -1};
  ASSERT_EQ(0, pipe2(output_pipe, O_CLOEXEC));
  ScopedFD output_read(output_pipe[0]);
  ScopedFD output_write(output_pipe[1]);

  CommandLine cmd = CreateChildCommandLine("spawn-grandchild");
  LaunchOptions options = CreateStdioRedirectOptions(output_write.get());

  bool was_timeout = false;
  std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
      cmd, options, TestTimeouts::action_timeout(), &delegate_, &was_timeout,
      &live_processes_lock_, &live_processes_);
  output_write.reset();

  ASSERT_TRUE(exit_code.has_value());
  EXPECT_EQ(0, *exit_code);
  EXPECT_FALSE(was_timeout);

  std::string output = ReadAllFromFd(output_read.get());
  int grandchild_pid = -1;
  ASSERT_TRUE(StringToInt(std::string_view(output).substr(0, output.find('\n')),
                          &grandchild_pid));
  ASSERT_GT(grandchild_pid, 0);

  // The waiter sends SIGKILL to the `-test_pid` process group when `test_pid`
  // exits. Wait briefly for the kernel/waiter to kill/reap the grandchild.
  EXPECT_TRUE(WaitForProcessDeadOrZombie(grandchild_pid));
}

TEST_F(TestLauncherForkServerTest, TimeoutKillsProcessAndNotifiesDelegate) {
  StartForkServer();

  delegate_.set_live_processes_for_inspection(&live_processes_lock_,
                                              &live_processes_);
  CommandLine cmd = CreateChildCommandLine("hang");
  bool was_timeout = false;
  std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
      cmd, LaunchOptions(), Milliseconds(100), &delegate_, &was_timeout,
      &live_processes_lock_, &live_processes_);

  ASSERT_TRUE(exit_code.has_value());
  EXPECT_EQ(-1, *exit_code);
  EXPECT_TRUE(was_timeout);
  EXPECT_EQ(1, delegate_.timed_out_count());
  EXPECT_EQ(cmd.GetCommandLineString(),
            delegate_.last_timed_out_cmd_line().GetCommandLineString());
  EXPECT_EQ(1u, delegate_.live_processes_count_on_timeout());
  EXPECT_GT(delegate_.timed_out_pid(), 0);
  // Because timeout waits for the subreaper waiter to reap `test_pid` before
  // returning, `timed_out_pid()` is already reaped without polling.
  EXPECT_TRUE(IsProcessDeadOrZombie(delegate_.timed_out_pid()));
  EXPECT_TRUE(live_processes_.empty());
}

TEST_F(TestLauncherForkServerTest, UnexpectedWaiterDeathKillsProcessGroup) {
  StartForkServer();

  int output_pipe[2] = {-1, -1};
  ASSERT_EQ(0, pipe2(output_pipe, O_CLOEXEC));
  ScopedFD output_read(output_pipe[0]);
  ScopedFD output_write(output_pipe[1]);

  CommandLine cmd = CreateChildCommandLine("spawn-grandchild-and-kill-waiter");
  LaunchOptions options = CreateStdioRedirectOptions(output_write.get());

  bool was_timeout = false;
  std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
      cmd, options, TestTimeouts::action_timeout(), &delegate_, &was_timeout,
      &live_processes_lock_, &live_processes_);
  output_write.reset();

  ASSERT_TRUE(exit_code.has_value());
  EXPECT_EQ(-1, *exit_code);
  EXPECT_FALSE(was_timeout);
  EXPECT_TRUE(live_processes_.empty());

  std::string output = ReadAllFromFd(output_read.get());
  int grandchild_pid = -1;
  ASSERT_TRUE(StringToInt(std::string_view(output).substr(0, output.find('\n')),
                          &grandchild_pid));
  ASSERT_GT(grandchild_pid, 0);

  EXPECT_TRUE(WaitForProcessDeadOrZombie(grandchild_pid));
  if (!IsProcessDeadOrZombie(grandchild_pid)) {
    kill(grandchild_pid, SIGKILL);
  }
}

TEST_F(TestLauncherForkServerTest, LauncherDeathKillsGrandchildProcessGroup) {
  int report_pipe[2] = {-1, -1};
  ASSERT_EQ(0, pipe2(report_pipe, O_CLOEXEC));
  ScopedFD report_read(report_pipe[0]);
  ScopedFD report_write(report_pipe[1]);

  pid_t sub_launcher = fork();
  ASSERT_GE(sub_launcher, 0);
  if (sub_launcher == 0) {
    report_read.reset();
    StartForkServer();

    CommandLine cmd = CreateChildCommandLine("spawn-grandchild-and-hang");
    LaunchOptions options = CreateStdioRedirectOptions(report_write.get());

    bool was_timeout = false;
    std::ignore = LaunchChildTestProcessViaForkServer(
        cmd, options, TestTimeouts::action_timeout(), &delegate_, &was_timeout,
        &live_processes_lock_, &live_processes_);
    _exit(1);
  }

  report_write.reset();
  std::string pid_line = ReadLineFromFd(report_read.get());
  int grandchild_pid = -1;
  ASSERT_TRUE(StringToInt(pid_line, &grandchild_pid));
  ASSERT_GT(grandchild_pid, 0);

  // Simulate an abrupt launcher crash/SIGKILL without calling
  // `KillSpawnedTestProcesses()`. The PDEATHSIG chain (sub_launcher ->
  // ForkServer SIGKILL -> Waiter SIGHUP handler) must kill `-test_pid`.
  ASSERT_EQ(0, kill(sub_launcher, SIGKILL));
  ASSERT_EQ(sub_launcher, HANDLE_EINTR(waitpid(sub_launcher, nullptr, 0)));

  EXPECT_TRUE(WaitForProcessDeadOrZombie(grandchild_pid));
  if (!IsProcessDeadOrZombie(grandchild_pid)) {
    kill(grandchild_pid, SIGKILL);
  }
}

TEST_F(TestLauncherForkServerTest,
       PreSpawnFailureReturnsNulloptAndDisablesForkServer) {
  StartForkServer();

  int output_pipe[2] = {-1, -1};
  ASSERT_EQ(0, pipe2(output_pipe, O_CLOEXEC));
  ScopedFD output_read(output_pipe[0]);
  ScopedFD output_write(output_pipe[1]);

  CommandLine pid_cmd = CreateChildCommandLine("report-fork-server-pid");
  LaunchOptions options = CreateStdioRedirectOptions(output_write.get());

  bool was_timeout = false;
  std::optional<int> pid_exit = LaunchChildTestProcessViaForkServer(
      pid_cmd, options, TestTimeouts::action_timeout(), &delegate_,
      &was_timeout, &live_processes_lock_, &live_processes_);
  output_write.reset();
  ASSERT_TRUE(pid_exit.has_value());
  ASSERT_EQ(0, *pid_exit);

  std::string output = ReadAllFromFd(output_read.get());
  int fork_server_pid = -1;
  ASSERT_TRUE(StringToInt(std::string_view(output).substr(0, output.find('\n')),
                          &fork_server_pid));
  ASSERT_GT(fork_server_pid, 0);

  // Kill the fork server directly so the next IPC request fails pre-spawn.
  ASSERT_EQ(0, kill(fork_server_pid, SIGKILL));
  ASSERT_TRUE(WaitForProcessDeadOrZombie(fork_server_pid));

  CommandLine cmd = CreateChildCommandLine("exit-code");
  cmd.AppendSwitchASCII(kChildExitCodeSwitch, "0");
  was_timeout = false;
  std::optional<int> fallback_exit = LaunchChildTestProcessViaForkServer(
      cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
      &was_timeout, &live_processes_lock_, &live_processes_);
  EXPECT_FALSE(fallback_exit.has_value());
  EXPECT_FALSE(was_timeout);
  EXPECT_FALSE(CanUseTestLauncherForkServer(cmd, 1, &delegate_));
}

TEST_F(TestLauncherForkServerTest,
       OversizedRequestFallsBackWithoutDisablingForkServer) {
  StartForkServer();

  CommandLine oversized_cmd = CreateChildCommandLine("exit-code");
  oversized_cmd.AppendSwitchASCII(kChildExitCodeSwitch, "0");
  oversized_cmd.AppendSwitchASCII(kChildCustomArgSwitch,
                                  std::string(70 * 1024, 'x'));

  bool was_timeout = false;
  std::optional<int> oversized_exit = LaunchChildTestProcessViaForkServer(
      oversized_cmd, LaunchOptions(), TestTimeouts::action_timeout(),
      &delegate_, &was_timeout, &live_processes_lock_, &live_processes_);
  EXPECT_FALSE(oversized_exit.has_value());
  EXPECT_FALSE(was_timeout);

  // Subsequent normal-sized launches continue using the fork server.
  CommandLine normal_cmd = CreateChildCommandLine("exit-code");
  normal_cmd.AppendSwitchASCII(kChildExitCodeSwitch, "7");
  EXPECT_TRUE(CanUseTestLauncherForkServer(normal_cmd, 1, &delegate_));
  std::optional<int> normal_exit = LaunchChildTestProcessViaForkServer(
      normal_cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
      &was_timeout, &live_processes_lock_, &live_processes_);
  ASSERT_TRUE(normal_exit.has_value());
  EXPECT_EQ(7, *normal_exit);
}

TEST_F(TestLauncherForkServerTest,
       UnsupportedLaunchOptionsFallBackWithoutDisablingForkServer) {
  StartForkServer();

  CommandLine cmd = CreateChildCommandLine("exit-code");
  cmd.AppendSwitchASCII(kChildExitCodeSwitch, "0");

  int pipe_a[2] = {-1, -1};
  int pipe_b[2] = {-1, -1};
  ASSERT_EQ(0, pipe2(pipe_a, O_CLOEXEC));
  ASSERT_EQ(0, pipe2(pipe_b, O_CLOEXEC));
  ScopedFD pipe_a_read(pipe_a[0]);
  ScopedFD pipe_a_write(pipe_a[1]);
  ScopedFD pipe_b_read(pipe_b[0]);
  ScopedFD pipe_b_write(pipe_b[1]);

  std::vector<LaunchOptions> unsupported_cases;
  {
    LaunchOptions opt;
    opt.clear_environment = true;
    unsupported_cases.push_back(std::move(opt));
  }
  {
    LaunchOptions opt;
    opt.clone_flags = 1;
    unsupported_cases.push_back(std::move(opt));
  }
  {
    LaunchOptions opt;
    opt.real_path = FilePath("/bin/true");
    unsupported_cases.push_back(std::move(opt));
  }
  {
    // Redirecting only stdout without stderr is not supported.
    LaunchOptions opt;
    opt.fds_to_remap.emplace_back(pipe_a_write.get(), STDOUT_FILENO);
    unsupported_cases.push_back(std::move(opt));
  }
  {
    // Redirecting stdout and stderr to distinct FDs is not supported.
    LaunchOptions opt;
    opt.fds_to_remap.emplace_back(pipe_a_write.get(), STDOUT_FILENO);
    opt.fds_to_remap.emplace_back(pipe_b_write.get(), STDERR_FILENO);
    unsupported_cases.push_back(std::move(opt));
  }
  {
    // Arbitrary non-stdio FD remappings are not supported.
    LaunchOptions opt = CreateStdioRedirectOptions(pipe_a_write.get());
    opt.fds_to_remap.emplace_back(pipe_b_write.get(), 3);
    unsupported_cases.push_back(std::move(opt));
  }

  for (const LaunchOptions& opt : unsupported_cases) {
    bool was_timeout = false;
    std::optional<int> res = LaunchChildTestProcessViaForkServer(
        cmd, opt, TestTimeouts::action_timeout(), &delegate_, &was_timeout,
        &live_processes_lock_, &live_processes_);
    EXPECT_FALSE(res.has_value());
    EXPECT_FALSE(was_timeout);
    EXPECT_TRUE(CanUseTestLauncherForkServer(cmd, 1, &delegate_));
  }

  bool was_timeout = false;
  std::optional<int> ok_exit = LaunchChildTestProcessViaForkServer(
      cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
      &was_timeout, &live_processes_lock_, &live_processes_);
  ASSERT_TRUE(ok_exit.has_value());
  EXPECT_EQ(0, *ok_exit);
}

TEST_F(TestLauncherForkServerTest, ClearsShardingEnvAndGTestFlagsInChildren) {
  struct ScopedShardingOverride {
    ScopedShardingOverride()
        : orig_total_env(
              getenv("GTEST_TOTAL_SHARDS")
                  ? std::optional<std::string>(getenv("GTEST_TOTAL_SHARDS"))
                  : std::nullopt),
          orig_index_env(
              getenv("GTEST_SHARD_INDEX")
                  ? std::optional<std::string>(getenv("GTEST_SHARD_INDEX"))
                  : std::nullopt),
          orig_total_flag(GTEST_FLAG_GET(total_shards)),
          orig_index_flag(GTEST_FLAG_GET(shard_index)) {
      setenv("GTEST_TOTAL_SHARDS", "4", 1);
      setenv("GTEST_SHARD_INDEX", "2", 1);
      GTEST_FLAG_SET(total_shards, 4);
      GTEST_FLAG_SET(shard_index, 2);
    }
    ~ScopedShardingOverride() {
      if (orig_total_env.has_value()) {
        setenv("GTEST_TOTAL_SHARDS", orig_total_env->c_str(), 1);
      } else {
        unsetenv("GTEST_TOTAL_SHARDS");
      }
      if (orig_index_env.has_value()) {
        setenv("GTEST_SHARD_INDEX", orig_index_env->c_str(), 1);
      } else {
        unsetenv("GTEST_SHARD_INDEX");
      }
      GTEST_FLAG_SET(total_shards, orig_total_flag);
      GTEST_FLAG_SET(shard_index, orig_index_flag);
    }
    std::optional<std::string> orig_total_env;
    std::optional<std::string> orig_index_env;
    int32_t orig_total_flag;
    int32_t orig_index_flag;
  };

  {
    ScopedShardingOverride sharding_override;
    StartForkServer();
  }

  CommandLine cmd = CreateChildCommandLine("verify-sharding-cleared");
  bool was_timeout = false;
  std::optional<int> exit_code = LaunchChildTestProcessViaForkServer(
      cmd, LaunchOptions(), TestTimeouts::action_timeout(), &delegate_,
      &was_timeout, &live_processes_lock_, &live_processes_);
  ASSERT_TRUE(exit_code.has_value());
  EXPECT_EQ(0, *exit_code);
  EXPECT_FALSE(was_timeout);
}

}  // namespace
}  // namespace base
