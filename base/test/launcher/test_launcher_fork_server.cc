// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher_fork_server.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/set_process_title_linux.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
constexpr char kTestShardIndex[] = "GTEST_SHARD_INDEX";
// Maximum serialized `ForkRequest` payload accepted over the fork server
// socket. Larger requests fall back to `LaunchProcess()` on the client before
// `SendMsg()` so `RecvMsg()` does not truncate the datagram and close its
// received pipe FDs.
constexpr size_t kMaxForkRequestBytes = 64 * 1024;

// Client socket and PID for the early fork server process spawned by
// MaybeStartTestLauncherForkServer().
//
// Why a fork server:
// Browser test binaries on Linux/ChromeOS are ~1.4 GB. Launching every test via
// fork() + execve() repeats dynamic linking, relocation processing, and static
// initializers (~49k GTest registrations) thousands of times per shard.
// Forking before threads or Mojo/Content globals are initialized lets each test
// subprocess skip execve() and start directly with pre-initialized static state
// via copy-on-write.
//
// Process model for each test launch:
//   TestLauncher ---(UnixDomainSocket::SendMsg: Pickle + wait_pipe[1])--->
//     ForkServer --fork()-->
//       Waiter (PR_SET_CHILD_SUBREAPER) --fork()-->
//         TestProcess (setpgid(0, 0), updates CommandLine, runs test)
//
// The Waiter places TestProcess into its own process group (pgid == test_pid),
// writes `{waiter_pid, test_pid}` to `wait_pipe`, waits for `test_pid` to exit,
// sends SIGKILL to `-test_pid`, writes `exit_code` to `wait_pipe`, and
// non-blockingly reaps any exited descendants in `-test_pid` before exiting.
int g_fork_server_fd = -1;
pid_t g_fork_server_pid = -1;
std::atomic<bool> g_fork_server_disabled = false;
std::atomic<bool> g_fork_server_shutting_down = false;
volatile sig_atomic_t g_waiter_child_pgid = 0;

void WaiterParentDeathSignalHandler(int /*signo*/) {
  pid_t pgid = g_waiter_child_pgid;
  if (pgid > 0) {
    kill(-1 * pgid, SIGKILL);
  }
  _exit(1);
}

// Waits up to `timeout` for `fd` to become readable or hit EOF/HUP,
// recomputing the remaining deadline across `EINTR` so signals neither reset
// the deadline nor cause an early timeout.
bool WaitForPipeReadable(int fd, TimeDelta timeout) {
  const TimeTicks deadline = TimeTicks::Now() + timeout;
  for (TimeTicks now = TimeTicks::Now(); now < deadline;
       now = TimeTicks::Now()) {
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    struct timespec ts = (deadline - now).ToTimeSpec();
    int poll_res = ppoll(&pfd, 1, &ts, nullptr);
    if (poll_res > 0) {
      return true;
    }
    if (poll_res == 0 || errno != EINTR) {
      return false;
    }
  }
  return false;
}

struct ForkedTestPids {
  pid_t waiter_pid = -1;
  pid_t test_pid = -1;
};

struct ForkRequest {
  std::string current_dir;
  std::vector<std::pair<std::string, std::string>> env_vars;
  std::vector<std::string> args;
};

bool UnpackForkRequest(span<const uint8_t> data, ForkRequest* request) {
  PickleIterator iter = PickleIterator::WithData(data);
  uint32_t env_count = 0;
  if (!iter.ReadString(&request->current_dir) || !iter.ReadUInt32(&env_count)) {
    return false;
  }
  request->env_vars.resize(env_count);
  for (auto& [key, val] : request->env_vars) {
    if (!iter.ReadString(&key) || !iter.ReadString(&val)) {
      return false;
    }
  }
  uint32_t arg_count = 0;
  if (!iter.ReadUInt32(&arg_count)) {
    return false;
  }
  request->args.resize(arg_count);
  for (auto& arg : request->args) {
    if (!iter.ReadString(&arg)) {
      return false;
    }
  }
  return true;
}

void ReplaceArgvForTestProcess(std::vector<std::string> args,
                               int* argc,
                               char*** argv) {
  static NoDestructor<std::vector<std::string>> child_args;
  static NoDestructor<std::vector<char*>> child_argv;
  *child_args = std::move(args);
  child_argv->clear();
  child_argv->reserve(child_args->size() + 1);
  for (auto& arg : *child_args) {
    child_argv->push_back(arg.data());
  }
  *argc = static_cast<int>(child_args->size());
  child_argv->push_back(nullptr);
  *argv = child_argv->data();

  CommandLine::Reset();
  CommandLine::Init(*argc, *argv);
}

void ConfigureForkedTestChild(ForkRequest request,
                              ScopedFD output_fd,
                              const sigset_t& orig_mask,
                              pid_t waiter_pid,
                              int* argc,
                              char*** argv) {
  setpgid(0, 0);
  struct sigaction dfl_sa = {};
  dfl_sa.sa_handler = SIG_DFL;
  sigemptyset(&dfl_sa.sa_mask);
  sigaction(SIGHUP, &dfl_sa, nullptr);
  sigprocmask(SIG_SETMASK, &orig_mask, nullptr);
  prctl(PR_SET_PDEATHSIG, SIGKILL);
  if (getppid() != waiter_pid) {
    _exit(1);
  }
  signal(SIGPIPE, SIG_DFL);
  ScopedFD dev_null(HANDLE_EINTR(open("/dev/null", O_RDONLY)));
  if (dev_null.is_valid()) {
    dup2(dev_null.get(), STDIN_FILENO);
  }
  // When TestLauncher redirects stdio to an output file, redirect both stdout
  // and stderr to `output_fd`; otherwise leave the inherited stdout/stderr in
  // place so output is printed directly.
  if (output_fd.is_valid()) {
    int out = output_fd.get();
    dup2(out, STDOUT_FILENO);
    dup2(out, STDERR_FILENO);
    if (out != STDOUT_FILENO && out != STDERR_FILENO) {
      output_fd.reset();
    } else {
      std::ignore = output_fd.release();
    }
  }
  if (!request.current_dir.empty() && chdir(request.current_dir.c_str()) != 0) {
    _exit(127);
  }
  for (const auto& [key, val] : request.env_vars) {
    if (val.empty()) {
      unsetenv(key.c_str());
    } else {
      setenv(key.c_str(), val.c_str(), 1);
    }
  }
  ReplaceArgvForTestProcess(std::move(request.args), argc, argv);
}

[[noreturn]] void WaitForTestProcessAndExit(pid_t waiter_pid,
                                            pid_t test_pid,
                                            const sigset_t& hup_mask,
                                            ScopedFD wait_pipe_write) {
  setpgid(test_pid, test_pid);
  g_waiter_child_pgid = test_pid;
  sigprocmask(SIG_UNBLOCK, &hup_mask, nullptr);

  ForkedTestPids pids = {
      .waiter_pid = waiter_pid,
      .test_pid = test_pid,
  };
  std::ignore = HANDLE_EINTR(write(wait_pipe_write.get(), &pids, sizeof(pids)));

  int status = 0;
  pid_t w = HANDLE_EINTR(waitpid(test_pid, &status, 0));
  int exit_code =
      (w == test_pid && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
  kill(-1 * test_pid, SIGKILL);
  std::ignore =
      HANDLE_EINTR(write(wait_pipe_write.get(), &exit_code, sizeof(exit_code)));
  wait_pipe_write.reset();
  // Reap any already-exited descendants without blocking: if a descendant is
  // stuck in uninterruptible sleep (D state), `_exit(0)` below reparents it to
  // init with SIGKILL pending rather than hanging the waiter.
  while (waitpid(-1 * test_pid, nullptr, WNOHANG) > 0) {
  }
  _exit(0);
}

// Runs in the forked subreaper waiter process: the waiter monitors the test
// child and terminates via `_exit()`, while the forked test child configures
// itself and returns `true`.
bool RunSubreaperWaiterOrChild(ForkRequest request,
                               ScopedFD wait_pipe_write,
                               ScopedFD output_fd,
                               pid_t fork_server_pid,
                               int* argc,
                               char*** argv) {
  // Block SIGHUP until `test_pid` is forked and placed into its own process
  // group so that if the fork server exits at any point, SIGHUP cleanly
  // terminates the entire `-test_pid` process group (including grandchildren
  // that did not inherit `PR_SET_PDEATHSIG`).
  sigset_t hup_mask;
  sigset_t orig_mask;
  sigemptyset(&hup_mask);
  sigaddset(&hup_mask, SIGHUP);
  sigprocmask(SIG_BLOCK, &hup_mask, &orig_mask);

  struct sigaction hup_sa = {};
  hup_sa.sa_handler = &WaiterParentDeathSignalHandler;
  sigemptyset(&hup_sa.sa_mask);
  sigaction(SIGHUP, &hup_sa, nullptr);

  prctl(PR_SET_PDEATHSIG, SIGHUP);
  if (getppid() != fork_server_pid) {
    _exit(1);
  }
  prctl(PR_SET_CHILD_SUBREAPER, 1);

  // Restore default SIGCHLD disposition (the fork server sets `SIG_IGN`, and
  // a parent runner might also have ignored SIGCHLD); otherwise `waitpid()`
  // fails with `ECHILD` instead of returning `test_pid`'s exit status.
  struct sigaction chld_sa = {};
  chld_sa.sa_handler = SIG_DFL;
  sigemptyset(&chld_sa.sa_mask);
  sigaction(SIGCHLD, &chld_sa, nullptr);

  const pid_t waiter_self_pid = getpid();
  pid_t test_pid = fork();
  if (test_pid < 0) {
    ForkedTestPids failed_pids;
    std::ignore = HANDLE_EINTR(
        write(wait_pipe_write.get(), &failed_pids, sizeof(failed_pids)));
    _exit(1);
  }
  if (test_pid > 0) {
    output_fd.reset();
    WaitForTestProcessAndExit(waiter_self_pid, test_pid, hup_mask,
                              std::move(wait_pipe_write));
  }

  wait_pipe_write.reset();
  ConfigureForkedTestChild(std::move(request), std::move(output_fd), orig_mask,
                           waiter_self_pid, argc, argv);
  return true;
}

bool RunForkServerLoop(ScopedFD server_fd,
                       pid_t launcher_pid,
                       int* argc,
                       char*** argv) {
  prctl(PR_SET_PDEATHSIG, SIGKILL);
  if (getppid() != launcher_pid) {
    _exit(0);
  }
  // Initialize setproctitle() before mutating `environ` or replacing `*argv`
  // with heap storage in forked children, so `SetProcessTitleFromCommandLine()`
  // in each test process can update `/proc/<pid>/cmdline`.
  setproctitle_init(const_cast<const char**>(*argv));
  signal(SIGPIPE, SIG_IGN);
  // Auto-reap exited waiter processes in the kernel so idle waiters never
  // linger as zombies between requests; each waiter restores `SIG_DFL` before
  // waiting on its test child.
  signal(SIGCHLD, SIG_IGN);
  // TestLauncher handles sharding in the parent process and unsets
  // GTEST_TOTAL_SHARDS and GTEST_SHARD_INDEX before spawning children. Because
  // GoogleTest also caches these environment variables into GTest flags during
  // static initialization before main(), reset both the environment variables
  // and the cached flags here so forked single-test children do not re-shard.
  unsetenv(kTestTotalShards);
  unsetenv(kTestShardIndex);
  GTEST_FLAG_SET(total_shards, -1);
  GTEST_FLAG_SET(shard_index, -1);

  const pid_t fork_server_pid = getpid();
  std::vector<uint8_t> buf(kMaxForkRequestBytes);
  for (;;) {
    std::vector<ScopedFD> recv_fds;
    ssize_t n = UnixDomainSocket::RecvMsg(server_fd.get(), buf, &recv_fds);
    if (n < 0 && errno == EMSGSIZE) {
      // `LaunchChildTestProcessViaForkServer()` checks `kMaxForkRequestBytes`
      // before sending, so truncation should not occur. Log and exit since
      // `RecvMsg()` has closed `recv_fds` and the client will disable the fork
      // server on pipe EOF.
      PLOG(ERROR) << "Fork server received truncated request";
      _exit(1);
    }
    if (n <= 0) {
      _exit(0);
    }
    if (recv_fds.empty()) {
      continue;
    }
    ScopedFD wait_pipe_write = std::move(recv_fds[0]);
    ScopedFD output_fd;
    if (recv_fds.size() >= 2) {
      output_fd = std::move(recv_fds[1]);
    }

    ForkRequest request;
    if (!UnpackForkRequest(span(buf).first(static_cast<size_t>(n)), &request)) {
      ForkedTestPids failed_pids;
      std::ignore = HANDLE_EINTR(
          write(wait_pipe_write.get(), &failed_pids, sizeof(failed_pids)));
      continue;
    }

    pid_t waiter_pid = fork();
    if (waiter_pid < 0) {
      ForkedTestPids failed_pids;
      std::ignore = HANDLE_EINTR(
          write(wait_pipe_write.get(), &failed_pids, sizeof(failed_pids)));
      continue;
    }
    if (waiter_pid > 0) {
      continue;
    }

    server_fd.reset();
    return RunSubreaperWaiterOrChild(
        std::move(request), std::move(wait_pipe_write), std::move(output_fd),
        fork_server_pid, argc, argv);
  }
}

// Checks that `options` only uses `LaunchOptions` fields supported by the fork
// server, and extracts the optional shared stdout/stderr FD into
// `*stdio_output_fd` (-1 when stdio is not redirected).
//
// In production, `DoLaunchChildTestProcess()` on Linux/ChromeOS only sets:
// - `environment` (`TMPDIR` when per-child temp dirs are used),
// - `fds_to_remap` (mapping a single output file FD to both `STDOUT_FILENO` and
//   `STDERR_FILENO` when redirecting stdio, plus any entries from
//   `TestLauncher::LaunchOptions::fds_to_remap`),
// - `new_process_group = true` (always provided via `setpgid()`), and
// - `kill_on_parent_death = true` (always provided via `PR_SET_PDEATHSIG`),
// while `LaunchChildTestProcessWithOptions()` sets `allow_new_privs = true`
// (satisfied because the fork server never sets `PR_SET_NO_NEW_PRIVS`).
//
// If any caller specifies other `LaunchOptions` fields or arbitrary FD
// remappings, return false so `TestLauncher` falls back to `LaunchProcess()`
// rather than silently ignoring them.
bool ForkServerSupportsLaunchOptions(const LaunchOptions& options,
                                     int* stdio_output_fd) {
  *stdio_output_fd = -1;
  if (options.wait || options.clear_environment || options.clone_flags != 0 ||
      !options.fds_to_remove_cloexec.empty() || !options.real_path.empty() ||
      options.pre_exec_delegate != nullptr ||
      options.maximize_rlimits != nullptr) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (options.ctrl_terminal_fd >= 0) {
    return false;
  }
#endif
  if (options.fds_to_remap.empty()) {
    return true;
  }
  if (options.fds_to_remap.size() != 2) {
    return false;
  }
  int stdout_src = -1;
  int stderr_src = -1;
  for (const auto& [src_fd, dst_fd] : options.fds_to_remap) {
    if (src_fd < 0) {
      return false;
    }
    if (dst_fd == STDOUT_FILENO) {
      stdout_src = src_fd;
    } else if (dst_fd == STDERR_FILENO) {
      stderr_src = src_fd;
    } else {
      return false;
    }
  }
  if (stdout_src < 0 || stdout_src != stderr_src) {
    return false;
  }
  *stdio_output_fd = stdout_src;
  return true;
}

}  // namespace

bool MaybeStartTestLauncherForkServer(int* argc,
                                      char*** argv,
                                      bool force_for_testing) {
  // Disabled by default; will be enabled in a follow-up CL.
  constexpr bool kForkServerEnabled = false;
  if ((!kForkServerEnabled && !force_for_testing) || g_fork_server_fd >= 0) {
    return false;
  }
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kGTestListTestsFlag) ||
      command_line->HasSwitch(kGTestHelpFlag) ||
      (!force_for_testing &&
       (command_line->HasSwitch(switches::kSingleProcessTests) ||
        command_line->HasSwitch("single-process")))) {
    return false;
  }

  ScopedFD client_fd;
  ScopedFD server_fd;
  if (!CreateSocketPair(&client_fd, &server_fd)) {
    return false;
  }
  fcntl(client_fd.get(), F_SETFD, FD_CLOEXEC);
  fcntl(server_fd.get(), F_SETFD, FD_CLOEXEC);
  fflush(stdout);
  fflush(stderr);
  const pid_t launcher_pid = getpid();
  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid > 0) {
    g_fork_server_fd = client_fd.release();
    g_fork_server_pid = pid;
    g_fork_server_disabled.store(false, std::memory_order_relaxed);
    g_fork_server_shutting_down.store(false, std::memory_order_relaxed);
    return false;
  }

  client_fd.reset();
  return RunForkServerLoop(std::move(server_fd), launcher_pid, argc, argv);
}

void StopTestLauncherForkServer() {
  g_fork_server_disabled.store(false, std::memory_order_relaxed);
  g_fork_server_shutting_down.store(false, std::memory_order_relaxed);
  if (g_fork_server_fd >= 0) {
    close(g_fork_server_fd);
    g_fork_server_fd = -1;
  }
  if (g_fork_server_pid > 0) {
    HANDLE_EINTR(waitpid(g_fork_server_pid, nullptr, 0));
    g_fork_server_pid = -1;
  }
}

void NotifyTestLauncherForkServerShutdown() {
  g_fork_server_shutting_down.store(true, std::memory_order_relaxed);
}

bool CanUseTestLauncherForkServer(const CommandLine& command_line,
                                  size_t num_tests,
                                  TestLauncherDelegate* delegate) {
  if (g_fork_server_fd < 0) {
    return false;
  }
  // Disabled after an unexpected fork-server IPC failure so remaining tests
  // fall back to `LaunchProcess()`.
  if (g_fork_server_disabled.load(std::memory_order_relaxed)) {
    return false;
  }
  // Once `KillSpawnedTestProcesses()` starts tearing down the launcher, do not
  // route new launches to the fork server.
  if (g_fork_server_shutting_down.load(std::memory_order_relaxed)) {
    return false;
  }
  // Multi-test batches (e.g. `UnitTestLauncherDelegate`) monitor progress with
  // `ProcessResultWatcher` on a direct child `base::Process`, which the fork
  // server does not expose; single-test launches (such as browser tests) wait
  // with a single per-process timeout.
  if (num_tests != 1) {
    return false;
  }
  // Forked children are copies of the current process that never call
  // `execve()`, so they can only run the current test binary.
  if (command_line.GetProgram() !=
      CommandLine::ForCurrentProcess()->GetProgram()) {
    return false;
  }
  // Command-line wrappers (`--test-launcher-wrapper`, e.g. `xvfb-run` or
  // debuggers) require a real `execve()` to wrap the child process.
  if (delegate && !delegate->GetWrapper().empty()) {
    return false;
  }
  return true;
}

std::optional<int> LaunchChildTestProcessViaForkServer(
    const CommandLine& command_line,
    const LaunchOptions& options,
    TimeDelta timeout_per_test,
    TestLauncherDelegate* delegate,
    bool* was_timeout,
    Lock* live_processes_lock,
    absl::flat_hash_map<ProcessHandle, CommandLine>* live_processes) {
  if (g_fork_server_shutting_down.load(std::memory_order_relaxed)) {
    return -1;
  }

  int stdio_output_fd = -1;
  if (!ForkServerSupportsLaunchOptions(options, &stdio_output_fd)) {
    return std::nullopt;
  }

  Pickle pickle;
  pickle.WriteString(options.current_directory.value());
  pickle.WriteUInt32(checked_cast<uint32_t>(options.environment.size()));
  for (const auto& [key, val] : options.environment) {
    pickle.WriteString(key);
    pickle.WriteString(val);
  }
  const auto& argv = command_line.argv();
  pickle.WriteUInt32(checked_cast<uint32_t>(argv.size()));
  for (const auto& arg : argv) {
    pickle.WriteString(arg);
  }
  if (pickle.AsBytes().size() > kMaxForkRequestBytes) {
    return std::nullopt;
  }

  int wait_pipe[2] = {-1, -1};
  if (pipe2(wait_pipe, O_CLOEXEC) != 0) {
    return -1;
  }
  ScopedFD wait_read(wait_pipe[0]);
  ScopedFD wait_write(wait_pipe[1]);

  std::vector<int> send_fds = {wait_write.get()};
  if (stdio_output_fd >= 0) {
    send_fds.push_back(stdio_output_fd);
  }

  bool sent = UnixDomainSocket::SendMsg(g_fork_server_fd, pickle, send_fds);
  wait_write.reset();
  // Pipe writes of <= PIPE_BUF bytes (>= 512 per POSIX, 4096 on Linux) are
  // atomic, so this single 8-byte write from the waiter is read in full or
  // returns 0 (EOF) if the fork server / waiter closed the pipe before writing.
  ForkedTestPids pids;
  ssize_t pids_read = HANDLE_EINTR(read(wait_read.get(), &pids, sizeof(pids)));
  if (!sent || pids_read != sizeof(pids)) {
    if (g_fork_server_shutting_down.load(std::memory_order_relaxed)) {
      return -1;
    }
    g_fork_server_disabled.store(true, std::memory_order_relaxed);
    return std::nullopt;
  }
  if (pids.waiter_pid <= 0 || pids.test_pid <= 0) {
    // The fork server is still alive and reported a transient single-spawn
    // failure (e.g. `fork()` EAGAIN); fall back to `LaunchProcess()` for this
    // test without disabling the fork server.
    return std::nullopt;
  }

  {
    AutoLock lock(*live_processes_lock);
    if (g_fork_server_shutting_down.load(std::memory_order_relaxed)) {
      kill(-1 * pids.test_pid, SIGKILL);
      kill(pids.test_pid, SIGKILL);
      kill(pids.waiter_pid, SIGKILL);
      return -1;
    }
    live_processes->insert({pids.test_pid, command_line});
  }

  int exit_code = -1;
  bool did_exit = false;
  {
    ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    // Since `test_pid` is a child of the waiter process rather than
    // TestLauncher, we cannot `waitpid()` on it directly. Instead, the waiter
    // holds `wait_pipe_write` open while `test_pid` runs, then writes the
    // `int` exit code and closes the pipe when `test_pid` exits.
    if (WaitForPipeReadable(wait_read.get(), timeout_per_test)) {
      did_exit = true;
      int status_code = -1;
      // `sizeof(status_code)` <= PIPE_BUF, so the waiter's single pipe write is
      // atomic and `read()` returns either the full `int`, 0 on unexpected
      // waiter EOF, or -1 on error (never a partial read).
      ssize_t r = HANDLE_EINTR(
          read(wait_read.get(), &status_code, sizeof(status_code)));
      CHECK(r == sizeof(status_code) || r <= 0);
      if (r == sizeof(status_code)) {
        exit_code = status_code;
      } else {
        // The waiter died unexpectedly before reaping `test_pid` and killing
        // its process group; clean up `-test_pid` directly.
        kill(-1 * pids.test_pid, SIGKILL);
        kill(pids.test_pid, SIGKILL);
      }
    }
  }

  if (!did_exit) {
    if (delegate) {
      delegate->OnTestTimedOut(command_line);
    }
    *was_timeout = true;
    exit_code = -1;
    kill(-1 * pids.test_pid, SIGKILL);
    kill(pids.test_pid, SIGKILL);
    {
      ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
      // Give the subreaper waiter a brief grace period to reap `test_pid`
      // (ensuring `test_pid` has finished kernel teardown and closed its FDs)
      // before falling back to SIGKILLing the waiter itself if `test_pid` is
      // stuck in uninterruptible sleep.
      constexpr TimeDelta kWaiterReapGracePeriod = Seconds(2);
      if (!WaitForPipeReadable(wait_read.get(), kWaiterReapGracePeriod)) {
        kill(pids.waiter_pid, SIGKILL);
      }
    }
    LOG(ERROR) << "Timed out waiting for test process to exit ("
               << timeout_per_test << "). Terminating child process.";
  }

  {
    AutoLock lock(*live_processes_lock);
    live_processes->erase(pids.test_pid);
  }
  return exit_code;
}

}  // namespace base
