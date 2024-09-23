// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <utility>

#include "base/clang_profiling_buildflags.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <sys/event.h>
#endif

#if BUILDFLAG(CLANG_PROFILING)
#include "base/test/clang_profiling.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "TargetConditionals.h"
#endif

namespace {

#if !BUILDFLAG(IS_IOS) || (BUILDFLAG(IS_IOS) && TARGET_OS_SIMULATOR)
bool WaitpidWithTimeout(base::ProcessHandle handle,
                        int* status,
                        base::TimeDelta wait) {
  // This POSIX version of this function only guarantees that we wait no less
  // than |wait| for the process to exit.  The child process may
  // exit sometime before the timeout has ended but we may still block for up
  // to 256 milliseconds after the fact.
  //
  // waitpid() has no direct support on POSIX for specifying a timeout, you can
  // either ask it to block indefinitely or return immediately (WNOHANG).
  // When a child process terminates a SIGCHLD signal is sent to the parent.
  // Catching this signal would involve installing a signal handler which may
  // affect other parts of the application and would be difficult to debug.
  //
  // Our strategy is to call waitpid() once up front to check if the process
  // has already exited, otherwise to loop for |wait|, sleeping for
  // at most 256 milliseconds each time using usleep() and then calling
  // waitpid().  The amount of time we sleep starts out at 1 milliseconds, and
  // we double it every 4 sleep cycles.
  //
  // usleep() is speced to exit if a signal is received for which a handler
  // has been installed.  This means that when a SIGCHLD is sent, it will exit
  // depending on behavior external to this function.
  //
  // This function is used primarily for unit tests, if we want to use it in
  // the application itself it would probably be best to examine other routes.

  if (wait == base::TimeDelta::Max()) {
    return HANDLE_EINTR(waitpid(handle, status, 0)) > 0;
  }

  pid_t ret_pid = HANDLE_EINTR(waitpid(handle, status, WNOHANG));
  static const uint32_t kMaxSleepInMicroseconds = 1 << 18;  // ~256 ms.
  uint32_t max_sleep_time_usecs = 1 << 10;                  // ~1 ms.
  int double_sleep_time = 0;

  // If the process hasn't exited yet, then sleep and try again.
  base::TimeTicks wakeup_time = base::TimeTicks::Now() + wait;
  while (ret_pid == 0) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (now > wakeup_time)
      break;

    const uint32_t sleep_time_usecs = static_cast<uint32_t>(
        std::min(static_cast<uint64_t>((wakeup_time - now).InMicroseconds()),
                 uint64_t{max_sleep_time_usecs}));
    // usleep() will return 0 and set errno to EINTR on receipt of a signal
    // such as SIGCHLD.
    usleep(sleep_time_usecs);
    ret_pid = HANDLE_EINTR(waitpid(handle, status, WNOHANG));

    if ((max_sleep_time_usecs < kMaxSleepInMicroseconds) &&
        (double_sleep_time++ % 4 == 0)) {
      max_sleep_time_usecs *= 2;
    }
  }

  return ret_pid > 0;
}
#endif

#if BUILDFLAG(IS_MAC)
// Using kqueue on Mac so that we can wait on non-child processes.
// We can't use kqueues on child processes because we need to reap
// our own children using wait.
bool WaitForSingleNonChildProcess(base::ProcessHandle handle,
                                  base::TimeDelta wait) {
  DCHECK_GT(handle, 0);

  base::ScopedFD kq(kqueue());
  if (!kq.is_valid()) {
    DPLOG(ERROR) << "kqueue";
    return false;
  }

  struct kevent change = {0};
  EV_SET(&change, handle, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
  int result = HANDLE_EINTR(kevent(kq.get(), &change, 1, NULL, 0, NULL));
  if (result == -1) {
    if (errno == ESRCH) {
      // If the process wasn't found, it must be dead.
      return true;
    }

    DPLOG(ERROR) << "kevent (setup " << handle << ")";
    return false;
  }

  // Keep track of the elapsed time to be able to restart kevent if it's
  // interrupted.
  bool wait_forever = (wait == base::TimeDelta::Max());
  base::TimeDelta remaining_delta;
  base::TimeTicks deadline;
  if (!wait_forever) {
    remaining_delta = wait;
    deadline = base::TimeTicks::Now() + remaining_delta;
  }

  result = -1;
  struct kevent event = {0};

  do {
    struct timespec remaining_timespec;
    struct timespec* remaining_timespec_ptr;
    if (wait_forever) {
      remaining_timespec_ptr = NULL;
    } else {
      remaining_timespec = remaining_delta.ToTimeSpec();
      remaining_timespec_ptr = &remaining_timespec;
    }

    result = kevent(kq.get(), NULL, 0, &event, 1, remaining_timespec_ptr);

    if (result == -1 && errno == EINTR) {
      if (!wait_forever) {
        remaining_delta = deadline - base::TimeTicks::Now();
      }
      result = 0;
    } else {
      break;
    }
  } while (wait_forever || remaining_delta.is_positive());

  if (result < 0) {
    DPLOG(ERROR) << "kevent (wait " << handle << ")";
    return false;
  } else if (result > 1) {
    DLOG(ERROR) << "kevent (wait " << handle << "): unexpected result "
                << result;
    return false;
  } else if (result == 0) {
    // Timed out.
    return false;
  }

  DCHECK_EQ(result, 1);

  if (event.filter != EVFILT_PROC ||
      (event.fflags & NOTE_EXIT) == 0 ||
      event.ident != static_cast<uintptr_t>(handle)) {
    DLOG(ERROR) << "kevent (wait " << handle
                << "): unexpected event: filter=" << event.filter
                << ", fflags=" << event.fflags
                << ", ident=" << event.ident;
    return false;
  }

  return true;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

namespace base {

Process::Process(ProcessHandle handle) : process_(handle) {}

Process::Process(Process&& other) : process_(other.process_) {
#if BUILDFLAG(IS_CHROMEOS)
  unique_token_ = std::move(other.unique_token_);
#endif
#if BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK) && TARGET_OS_SIMULATOR
  content_process_ = other.content_process_;
#endif

  other.Close();
}

Process& Process::operator=(Process&& other) {
  process_ = other.process_;
#if BUILDFLAG(IS_CHROMEOS)
  unique_token_ = std::move(other.unique_token_);
#endif
#if BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK) && TARGET_OS_SIMULATOR
  content_process_ = other.content_process_;
#endif
  other.Close();
  return *this;
}

Process::~Process() = default;

// static
Process Process::Current() {
  return Process(GetCurrentProcessHandle());
}

// static
Process Process::Open(ProcessId pid) {
  if (pid == GetCurrentProcId())
    return Current();

  // On POSIX process handles are the same as PIDs.
  return Process(pid);
}

// static
Process Process::OpenWithExtraPrivileges(ProcessId pid) {
  // On POSIX there are no privileges to set.
  return Open(pid);
}

// static
void Process::TerminateCurrentProcessImmediately(int exit_code) {
#if BUILDFLAG(CLANG_PROFILING)
  WriteClangProfilingProfile();
#endif
  _exit(exit_code);
}

bool Process::IsValid() const {
  return process_ != kNullProcessHandle;
}

ProcessHandle Process::Handle() const {
  return process_;
}

Process Process::Duplicate() const {
  if (is_current())
    return Current();

  Process duplicate = Process(process_);
#if BUILDFLAG(IS_CHROMEOS)
  duplicate.unique_token_ = unique_token_;
#elif BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK) && TARGET_OS_SIMULATOR
  duplicate.content_process_ = content_process_;
#endif
  return duplicate;
}

ProcessHandle Process::Release() {
  return std::exchange(process_, kNullProcessHandle);
}

ProcessId Process::Pid() const {
  DCHECK(IsValid());
  return GetProcId(process_);
}

bool Process::is_current() const {
  return process_ == GetCurrentProcessHandle();
}

void Process::Close() {
  process_ = kNullProcessHandle;
  // if the process wasn't terminated (so we waited) or the state
  // wasn't already collected w/ a wait from process_utils, we're gonna
  // end up w/ a zombie when it does finally exit.
}

#if !BUILDFLAG(IS_IOS)
bool Process::Terminate(int exit_code, bool wait) const {
  // exit_code isn't supportable.
  DCHECK(IsValid());
  CHECK_GT(process_, 0);
  return TerminateInternal(exit_code, wait);
}
#endif

#if !BUILDFLAG(IS_IOS) || (BUILDFLAG(USE_BLINK) && TARGET_OS_SIMULATOR)
bool Process::TerminateInternal(int exit_code, bool wait) const {
  // RESULT_CODE_KILLED_BAD_MESSAGE == 3, but layering prevents its use.
  // |wait| is always false when terminating badly-behaved processes.
  const bool maybe_compromised = !wait && exit_code == 3;
  if (maybe_compromised) {
    // Forcibly terminate the process immediately.
    const bool was_killed = kill(process_, SIGKILL) != 0;
#if BUILDFLAG(IS_CHROMEOS)
    if (was_killed)
      CleanUpProcessAsync();
#endif
    DPLOG_IF(ERROR, !was_killed) << "Unable to terminate process " << process_;
    return was_killed;
  }

  // Terminate process giving it a chance to clean up.
  if (kill(process_, SIGTERM) != 0) {
    DPLOG(ERROR) << "Unable to terminate process " << process_;
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  CleanUpProcessAsync();
#endif

  if (!wait || WaitForExitWithTimeout(Seconds(60), nullptr)) {
    return true;
  }
  if (kill(process_, SIGKILL) != 0) {
    DPLOG(ERROR) << "Unable to kill process " << process_;
    return false;
  }
  return WaitForExit(nullptr);
}
#endif

bool Process::WaitForExit(int* exit_code) const {
  return WaitForExitWithTimeout(TimeDelta::Max(), exit_code);
}

#if !BUILDFLAG(IS_IOS)
bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const {
  if (!timeout.is_zero()) {
    // Assert that this thread is allowed to wait below. This intentionally
    // doesn't use ScopedBlockingCallWithBaseSyncPrimitives because the process
    // being waited upon tends to itself be using the CPU and considering this
    // thread non-busy causes more issue than it fixes: http://crbug.com/905788
    internal::AssertBaseSyncPrimitivesAllowed();
  }

  int local_exit_code = 0;
  bool exited = WaitForExitWithTimeoutImpl(Handle(), &local_exit_code, timeout);
  if (exited) {
    Exited(local_exit_code);
    if (exit_code)
      *exit_code = local_exit_code;
  }
  return exited;
}
#endif

#if !BUILDFLAG(IS_IOS) || (BUILDFLAG(USE_BLINK) && TARGET_OS_SIMULATOR)
bool Process::WaitForExitWithTimeoutImpl(base::ProcessHandle handle,
                                         int* exit_code,
                                         base::TimeDelta timeout) const {
  const base::ProcessHandle our_pid = base::GetCurrentProcessHandle();
  if (handle == our_pid) {
    // We won't be able to wait for ourselves to exit.
    return false;
  }

  TRACE_EVENT0("base", "Process::WaitForExitWithTimeout");

  const base::ProcessHandle parent_pid = base::GetParentProcessId(handle);
  const bool exited = (parent_pid < 0);

  if (!exited && parent_pid != our_pid) {
#if BUILDFLAG(IS_MAC)
    // On Mac we can wait on non child processes.
    return WaitForSingleNonChildProcess(handle, timeout);
#else
    // Currently on Linux we can't handle non child processes.
    NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_MAC)
  }

  int status;
  if (!WaitpidWithTimeout(handle, &status, timeout)) {
    return exited;
  }
  if (WIFSIGNALED(status)) {
    if (exit_code) {
      *exit_code = -1;
    }
    return true;
  }
  if (WIFEXITED(status)) {
    if (exit_code) {
      *exit_code = WEXITSTATUS(status);
    }
    return true;
  }
  return exited;
}
#endif

void Process::Exited(int exit_code) const {
#if BUILDFLAG(IS_CHROMEOS)
  CleanUpProcessAsync();
#endif
}

int Process::GetOSPriority() const {
  DCHECK(IsValid());
  return getpriority(PRIO_PROCESS, static_cast<id_t>(process_));
}

}  // namespace base
