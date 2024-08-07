// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/spinning_mutex.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if PA_BUILDFLAG(IS_POSIX)
#include <pthread.h>
#endif

#if PA_CONFIG(HAS_LINUX_KERNEL)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#endif  // PA_CONFIG(HAS_LINUX_KERNEL)

#if !PA_CONFIG(HAS_LINUX_KERNEL) && !PA_BUILDFLAG(IS_WIN) && \
    !PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(IS_POSIX) &&    \
    !PA_BUILDFLAG(IS_FUCHSIA)
#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"

#if PA_BUILDFLAG(IS_POSIX)
#include <sched.h>
#define PA_YIELD_THREAD sched_yield()
#else  // Other OS
#warning "Thread yield not supported on this OS."
#define PA_YIELD_THREAD ((void)0)
#endif

#endif

namespace partition_alloc::internal {

void SpinningMutex::Reinit() {
#if !PA_BUILDFLAG(IS_APPLE)
  // On most platforms, no need to re-init the lock, can just unlock it.
  Release();
#else
  unfair_lock_ = OS_UNFAIR_LOCK_INIT;
#endif  // PA_BUILDFLAG(IS_APPLE)
}

void SpinningMutex::AcquireSpinThenBlock() {
  int tries = 0;
  int backoff = 1;
  do {
    if (Try()) [[likely]] {
      return;
    }
    // Note: Per the intel optimization manual
    // (https://software.intel.com/content/dam/develop/public/us/en/documents/64-ia-32-architectures-optimization-manual.pdf),
    // the "pause" instruction is more costly on Skylake Client than on previous
    // architectures. The latency is found to be 141 cycles
    // there (from ~10 on previous ones, nice 14x).
    //
    // According to Agner Fog's instruction tables, the latency is still >100
    // cycles on Ice Lake, and from other sources, seems to be high as well on
    // Adler Lake. Separately, it is (from
    // https://agner.org/optimize/instruction_tables.pdf) also high on AMD Zen 3
    // (~65). So just assume that it's this way for most x86_64 architectures.
    //
    // Also, loop several times here, following the guidelines in section 2.3.4
    // of the manual, "Pause latency in Skylake Client Microarchitecture".
    for (int yields = 0; yields < backoff; yields++) {
      PA_YIELD_PROCESSOR;
      tries++;
    }
    constexpr int kMaxBackoff = 16;
    backoff = std::min(kMaxBackoff, backoff << 1);
  } while (tries < kSpinCount);

  LockSlow();
}

#if PA_CONFIG(HAS_LINUX_KERNEL)

void SpinningMutex::FutexWait() {
  // Save and restore errno.
  int saved_errno = errno;
  // Don't check the return value, as we will not be awaken by a timeout, since
  // none is specified.
  //
  // Ignoring the return value doesn't impact correctness, as this acts as an
  // immediate wakeup. For completeness, the possible errors for FUTEX_WAIT are:
  // - EACCES: state_ is not readable. Should not happen.
  // - EAGAIN: the value is not as expected, that is not |kLockedContended|, in
  //           which case retrying the loop is the right behavior.
  // - EINTR: signal, looping is the right behavior.
  // - EINVAL: invalid argument.
  //
  // Note: not checking the return value is the approach used in bionic and
  // glibc as well.
  //
  // Will return immediately if |state_| is no longer equal to
  // |kLockedContended|. Otherwise, sleeps and wakes up when |state_| may not be
  // |kLockedContended| anymore. Note that even without spurious wakeups, the
  // value of |state_| is not guaranteed when this returns, as another thread
  // may get the lock before we get to run.
  int err = syscall(SYS_futex, &state_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
                    kLockedContended, nullptr, nullptr, 0);

  if (err) {
    // These are programming error, check them.
    PA_DCHECK(errno != EACCES);
    PA_DCHECK(errno != EINVAL);
  }
  errno = saved_errno;
}

void SpinningMutex::FutexWake() {
  int saved_errno = errno;
  long retval = syscall(SYS_futex, &state_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                        1 /* wake up a single waiter */, nullptr, nullptr, 0);
  PA_CHECK(retval != -1);
  errno = saved_errno;
}

void SpinningMutex::LockSlow() {
  // If this thread gets awaken but another one got the lock first, then go back
  // to sleeping. See comments in |FutexWait()| to see why a loop is required.
  while (state_.exchange(kLockedContended, std::memory_order_acquire) !=
         kUnlocked) {
    FutexWait();
  }
}

#elif PA_BUILDFLAG(IS_WIN)

void SpinningMutex::LockSlow() {
  ::AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

#elif PA_BUILDFLAG(IS_APPLE)

void SpinningMutex::LockSlow() {
  return os_unfair_lock_lock(&unfair_lock_);
}

#elif PA_BUILDFLAG(IS_POSIX)

void SpinningMutex::LockSlow() {
  int retval = pthread_mutex_lock(&lock_);
  PA_DCHECK(retval == 0);
}

#elif PA_BUILDFLAG(IS_FUCHSIA)

void SpinningMutex::LockSlow() {
  sync_mutex_lock(&lock_);
}

#else

void SpinningMutex::LockSlow() {
  int yield_thread_count = 0;
  do {
    if (yield_thread_count < 10) {
      PA_YIELD_THREAD;
      yield_thread_count++;
    } else {
      // At this point, it's likely that the lock is held by a lower priority
      // thread that is unavailable to finish its work because of higher
      // priority threads spinning here. Sleeping should ensure that they make
      // progress.
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  } while (!Try());
}

#endif

}  // namespace partition_alloc::internal
