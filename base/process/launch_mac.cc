// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include <crt_externs.h>
#include <mach/mach.h>
#include <os/availability.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>

#include "base/apple/mach_port_rendezvous.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/environment_internal.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/base_tracing.h"

extern "C" {
// Changes the current thread's directory to a path or directory file
// descriptor.
int pthread_chdir_np(const char* dir);

int pthread_fchdir_np(int fd);

int responsibility_spawnattrs_setdisclaim(posix_spawnattr_t attrs,
                                          int disclaim);
}  // extern "C"

namespace base {

void CheckPThreadStackMinIsSafe() {
  static_assert(__builtin_constant_p(PTHREAD_STACK_MIN),
                "Always constant on mac");
}

namespace {

// DPSXCHECK is a Debug Posix Spawn Check macro. The posix_spawn* family of
// functions return an errno value, as opposed to setting errno directly. This
// macro emulates a DPCHECK().
#define DPSXCHECK(expr)                                              \
  do {                                                               \
    int rv = (expr);                                                 \
    DCHECK_EQ(rv, 0) << #expr << ": -" << rv << " " << strerror(rv); \
  } while (0)

class PosixSpawnAttr {
 public:
  PosixSpawnAttr() { DPSXCHECK(posix_spawnattr_init(&attr_)); }

  ~PosixSpawnAttr() { DPSXCHECK(posix_spawnattr_destroy(&attr_)); }

  posix_spawnattr_t* get() { return &attr_; }

 private:
  posix_spawnattr_t attr_;
};

class PosixSpawnFileActions {
 public:
  PosixSpawnFileActions() {
    DPSXCHECK(posix_spawn_file_actions_init(&file_actions_));
  }

  PosixSpawnFileActions(const PosixSpawnFileActions&) = delete;
  PosixSpawnFileActions& operator=(const PosixSpawnFileActions&) = delete;

  ~PosixSpawnFileActions() {
    DPSXCHECK(posix_spawn_file_actions_destroy(&file_actions_));
  }

  void Open(int filedes, const char* path, int mode) {
    DPSXCHECK(posix_spawn_file_actions_addopen(&file_actions_, filedes, path,
                                               mode, 0));
  }

  void Dup2(int filedes, int newfiledes) {
    DPSXCHECK(
        posix_spawn_file_actions_adddup2(&file_actions_, filedes, newfiledes));
  }

  void Inherit(int filedes) {
    DPSXCHECK(posix_spawn_file_actions_addinherit_np(&file_actions_, filedes));
  }

#if BUILDFLAG(IS_MAC)
  void Chdir(const char* path) {
    DPSXCHECK(posix_spawn_file_actions_addchdir_np(&file_actions_, path));
  }
#endif

  const posix_spawn_file_actions_t* get() const { return &file_actions_; }

 private:
  posix_spawn_file_actions_t file_actions_;
};

#if !BUILDFLAG(IS_MAC)
int ChangeCurrentThreadDirectory(const char* path) {
  return pthread_chdir_np(path);
}

// The recommended way to unset a per-thread cwd is to set a new value to an
// invalid file descriptor, per libpthread-218.1.3/private/private.h.
int ResetCurrentThreadDirectory() {
  return pthread_fchdir_np(-1);
}
#endif

struct GetAppOutputOptions {
  // Whether to pipe stderr to stdout in |output|.
  bool include_stderr = false;
  // Caller-supplied string poiter for the output.
  raw_ptr<std::string> output = nullptr;
  // Result exit code of Process::Wait().
  int exit_code = 0;
};

bool GetAppOutputInternal(const std::vector<std::string>& argv,
                          GetAppOutputOptions* gao_options) {
  TRACE_EVENT0("base", "GetAppOutput");

  ScopedFD read_fd, write_fd;
  {
    int pipefds[2];
    if (pipe(pipefds) != 0) {
      DPLOG(ERROR) << "pipe";
      return false;
    }
    read_fd.reset(pipefds[0]);
    write_fd.reset(pipefds[1]);
  }

  LaunchOptions launch_options;
  launch_options.fds_to_remap.emplace_back(write_fd.get(), STDOUT_FILENO);
  if (gao_options->include_stderr) {
    launch_options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);
  }

  Process process = LaunchProcess(argv, launch_options);
  if (!process.IsValid()) {
    return false;
  }

  // Close the parent process' write descriptor, so that EOF is generated in
  // read loop below.
  write_fd.reset();

  // Read the child's output before waiting for its exit, otherwise the pipe
  // buffer may fill up if the process is producing a lot of output.
  std::string* output = gao_options->output;
  output->clear();

  const size_t kBufferSize = 1024;
  size_t total_bytes_read = 0;
  ssize_t read_this_pass = 0;
  do {
    output->resize(output->size() + kBufferSize);
    read_this_pass = HANDLE_EINTR(
        read(read_fd.get(), &(*output)[total_bytes_read], kBufferSize));
    if (read_this_pass >= 0) {
      total_bytes_read += static_cast<size_t>(read_this_pass);
      output->resize(total_bytes_read);
    }
  } while (read_this_pass > 0);

  // It is okay to allow this process to wait on the launched process as a
  // process launched with GetAppOutput*() shouldn't wait back on the process
  // that launched it.
  internal::GetAppOutputScopedAllowBaseSyncPrimitives allow_wait;
  if (!process.WaitForExit(&gao_options->exit_code)) {
    return false;
  }

  return read_this_pass == 0;
}

}  // namespace

Process LaunchProcess(const CommandLine& cmdline,
                      const LaunchOptions& options) {
  return LaunchProcess(cmdline.argv(), options);
}

Process LaunchProcess(const std::vector<std::string>& argv,
                      const LaunchOptions& options) {
  TRACE_EVENT0("base", "LaunchProcess");

  PosixSpawnAttr attr;

  short flags = POSIX_SPAWN_CLOEXEC_DEFAULT;
  if (options.new_process_group) {
    flags |= POSIX_SPAWN_SETPGROUP;
    DPSXCHECK(posix_spawnattr_setpgroup(attr.get(), 0));
  }
  DPSXCHECK(posix_spawnattr_setflags(attr.get(), flags));

  PosixSpawnFileActions file_actions;

  // Process file descriptors for the child. By default, LaunchProcess will
  // open stdin to /dev/null and inherit stdout and stderr.
  bool inherit_stdout = true, inherit_stderr = true;
  bool null_stdin = true;
  for (const auto& dup2_pair : options.fds_to_remap) {
    if (dup2_pair.second == STDIN_FILENO) {
      null_stdin = false;
    } else if (dup2_pair.second == STDOUT_FILENO) {
      inherit_stdout = false;
    } else if (dup2_pair.second == STDERR_FILENO) {
      inherit_stderr = false;
    }

    if (dup2_pair.first == dup2_pair.second) {
      file_actions.Inherit(dup2_pair.second);
    } else {
      file_actions.Dup2(dup2_pair.first, dup2_pair.second);
    }
  }

  if (null_stdin) {
    file_actions.Open(STDIN_FILENO, "/dev/null", O_RDONLY);
  }
  if (inherit_stdout) {
    file_actions.Inherit(STDOUT_FILENO);
  }
  if (inherit_stderr) {
    file_actions.Inherit(STDERR_FILENO);
  }

#if BUILDFLAG(IS_MAC)
  if (options.disclaim_responsibility) {
    DPSXCHECK(responsibility_spawnattrs_setdisclaim(attr.get(), 1));
  }
#endif

  std::vector<char*> argv_cstr;
  argv_cstr.reserve(argv.size() + 1);
  for (const auto& arg : argv)
    argv_cstr.push_back(const_cast<char*>(arg.c_str()));
  argv_cstr.push_back(nullptr);

  base::HeapArray<char*> owned_environ;
  char* empty_environ = nullptr;
  char** new_environ =
      options.clear_environment ? &empty_environ : *_NSGetEnviron();
  if (!options.environment.empty()) {
    owned_environ =
        internal::AlterEnvironment(new_environ, options.environment);
    new_environ = owned_environ.data();
  }

  const char* executable_path = !options.real_path.empty()
                                    ? options.real_path.value().c_str()
                                    : argv_cstr[0];

  if (options.enable_cpu_security_mitigations) {
    DPSXCHECK(posix_spawnattr_set_csm_np(attr.get(), POSIX_SPAWN_NP_CSM_ALL));
  }

  if (!options.current_directory.empty()) {
    const char* chdir_str = options.current_directory.value().c_str();
#if BUILDFLAG(IS_MAC)
    file_actions.Chdir(chdir_str);
#else
    // If the chdir posix_spawn_file_actions extension is not available,
    // change the thread-specific working directory. The new process will
    // inherit it during posix_spawnp().
    int rv = ChangeCurrentThreadDirectory(chdir_str);
    if (rv != 0) {
      DPLOG(ERROR) << "pthread_chdir_np";
      return Process();
    }
#endif
  }

  int rv;
  pid_t pid;
  {
    const bool has_mach_ports_for_rendezvous =
        !options.mach_ports_for_rendezvous.empty();
#if BUILDFLAG(IS_IOS)
    // This code is only used for the iOS simulator to launch tests. We do not
    // support setting MachPorts on launch. You should look at
    // content::ChildProcessLauncherHelper (for iOS) if you are trying to spawn
    // a non-test and need ports.
    CHECK(!has_mach_ports_for_rendezvous);
#else
    // If |options.mach_ports_for_rendezvous| is specified : the server's lock
    // must be held for the duration of posix_spawnp() so that new child's PID
    // can be recorded with the set of ports.
    AutoLockMaybe rendezvous_lock(
        has_mach_ports_for_rendezvous
            ? &MachPortRendezvousServerMac::GetInstance()->GetLock()
            : nullptr);
#endif
    // Use posix_spawnp as some callers expect to have PATH consulted.
    rv = posix_spawnp(&pid, executable_path, file_actions.get(), attr.get(),
                      &argv_cstr[0], new_environ);

#if !BUILDFLAG(IS_IOS)
    if (has_mach_ports_for_rendezvous) {
      if (rv == 0) {
        MachPortRendezvousServerMac::GetInstance()->GetLock().AssertAcquired();
        MachPortRendezvousServerMac::GetInstance()->RegisterPortsForPid(
            pid, options.mach_ports_for_rendezvous);
      } else {
        // Because |options| is const-ref, the collection has to be copied here.
        // The caller expects to relinquish ownership of any strong rights if
        // LaunchProcess() were to succeed, so these rights should be manually
        // destroyed on failure.
        MachPortsForRendezvous ports = options.mach_ports_for_rendezvous;
        for (auto& port : ports) {
          port.second.Destroy();
        }
      }
    }
#endif
  }

#if !BUILDFLAG(IS_MAC)
  // Restore the thread's working directory if it was changed.
  if (!options.current_directory.empty()) {
    ResetCurrentThreadDirectory();
  }
#endif

  if (rv != 0) {
    DLOG(ERROR) << "posix_spawnp(" << executable_path << "): -" << rv << " "
                << strerror(rv);
    return Process();
  }

  if (options.wait) {
    // While this isn't strictly disk IO, waiting for another process to
    // finish is the sort of thing ThreadRestrictions is trying to prevent.
    ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
    pid_t ret = HANDLE_EINTR(waitpid(pid, nullptr, 0));
    DPCHECK(ret > 0);
  }

  return Process(pid);
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  return GetAppOutput(cl.argv(), output);
}

bool GetAppOutputAndError(const CommandLine& cl, std::string* output) {
  return GetAppOutputAndError(cl.argv(), output);
}

bool GetAppOutputWithExitCode(const CommandLine& cl,
                              std::string* output,
                              int* exit_code) {
  GetAppOutputOptions options;
  options.output = output;
  bool rv = GetAppOutputInternal(cl.argv(), &options);
  *exit_code = options.exit_code;
  return rv;
}

bool GetAppOutput(const std::vector<std::string>& argv, std::string* output) {
  GetAppOutputOptions options;
  options.output = output;
  return GetAppOutputInternal(argv, &options) &&
         options.exit_code == EXIT_SUCCESS;
}

bool GetAppOutputAndError(const std::vector<std::string>& argv,
                          std::string* output) {
  GetAppOutputOptions options;
  options.include_stderr = true;
  options.output = output;
  return GetAppOutputInternal(argv, &options) &&
         options.exit_code == EXIT_SUCCESS;
}

void RaiseProcessToHighPriority() {
  // Historically this has not been implemented on POSIX and macOS. This could
  // influence the Mach task policy in the future.
}

}  // namespace base
