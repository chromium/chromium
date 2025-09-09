// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/spinning_mutex.h"

#include <atomic>
#include <optional>

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

namespace {

// Pointer to the `LockMetricsRecorder` that all spinning mutexes record into.
std::atomic<LockMetricsRecorderInterface*> g_lock_metrics_recorder{nullptr};

LockMetricsRecorderInterface* GetLockMetricsRecorder() {
  return g_lock_metrics_recorder.load(std::memory_order_acquire);
}

// Timer that records into a lock metrics object. Copy of
// `::base::LockMetricsRecorder::ScopedLockAcquisitionTimer` for partition alloc
// with minor modifications.
class ScopedLockAcquisitionTimer {
 public:
  ScopedLockAcquisitionTimer() : lock_metrics_(GetLockMetricsRecorder()) {
    if (!lock_metrics_ || !lock_metrics_->ShouldRecordLockAcquisitionTime())
        [[likely]] {
      return;
    }

    start_time_.emplace(base::TimeTicks::Now());
  }

  ~ScopedLockAcquisitionTimer() {
    if (!start_time_.has_value()) [[likely]] {
      return;
    }

    lock_metrics_->RecordLockAcquisitionTime(base::TimeTicks::Now() -
                                             *start_time_);
  }

 private:
  std::optional<base::TimeTicks> start_time_;

  // It is safe to hold onto the pointer to the lock metrics recorder since
  // this is not expected to be modified once set except for in tests.
  LockMetricsRecorderInterface* lock_metrics_;
};

}  // namespace

// static
void SpinningMutex::SetLockMetricsRecorder(
    LockMetricsRecorderInterface* recorder) {
  auto* old_recorder =
      g_lock_metrics_recorder.exchange(recorder, std::memory_order_release);
  PA_CHECK(old_recorder == nullptr);
}

// static
void SpinningMutex::SetLockMetricsRecorderForTesting(
    LockMetricsRecorderInterface* recorder) {
  g_lock_metrics_recorder.store(recorder, std::memory_order_release);
}

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

  ScopedLockAcquisitionTimer timer;
  LockSlow();
}

#if PA_CONFIG(HAS_LINUX_KERNEL)

namespace {
PA_ALWAYS_INLINE long FutexSyscall(volatile void* ftx, int op, int value) {
  // Save, clear and restore errno.
  int saved_errno = errno;
  errno = 0;

  long retval = syscall(SYS_futex, ftx, op | FUTEX_PRIVATE_FLAG, value, nullptr,
                        nullptr, 0);
  if (retval == -1) {
    // These are programming errors, check them.
    PA_DCHECK((errno != EPERM) || (errno != EACCES) || (errno != EINVAL) ||
              (errno != ENOSYS))
        << "FutexSyscall(" << reinterpret_cast<uintptr_t>(ftx) << ", " << op
        << ", " << value << ")  failed with errno " << errno;
  }

  errno = saved_errno;
  return retval;
}
}  // namespace

void SpinningMutex::FutexWait() {
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
  FutexSyscall(&state_, FUTEX_WAIT, kLockedContended);
}

void SpinningMutex::FutexWake() {
  long retval =
      FutexSyscall(&state_, FUTEX_WAKE, 1 /* wake up a single waiter */);
  PA_CHECK(retval != -1);
}

#if PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)
// static
std::atomic<bool> SpinningMutex::s_use_pi_futex;

// static
void SpinningMutex::EnableUsePriorityInheritance() {
  s_use_pi_futex.store(true, std::memory_order_relaxed);
}

void SpinningMutex::FutexLockPI() {
  FutexSyscall(&state_pi_, FUTEX_LOCK_PI2, 0);
}

void SpinningMutex::FutexUnlockPI() {
  FutexSyscall(&state_pi_, FUTEX_UNLOCK_PI, 0);
}

void SpinningMutex::FutexMigrate() {
  // See explanation in |LockSlow()| for why marking the lock as migrated using
  // |migrated_| is not enough and the value of the non-PI futex has to be set
  // to |kMigrated|.
  migrated_.store(true, std::memory_order_release);
  if (state_.exchange(kMigrated, std::memory_order_release) !=
      kLockedUncontended) {
    FutexSyscall(&state_, FUTEX_WAKE, INT_MAX /* wake up all waiters */);
  }
}

void SpinningMutex::LockSlow() {
  while (!IsLockMigrated()) {
    // If the current thread has reached here, it thinks the lock has not been
    // migrated. But this might not be true since the thread that owns the
    // lock can migrate the lock at any time and the migration process is not
    // atomic.
    //
    // The current thread has to always mark the lock as being contended by
    // swapping the value of the non-PI futex with |kLockedContended| in the
    // slow path of the non-PI futex since that is a crucial for the
    // correctness of the non-PI futex locking algorithm. If we handle this
    // the same as the case where there is no PI futex at all, then it is
    // possible that the current thread could sleep in |FutexWait()| forever.
    // This happens when the current thread sets |state_| to
    // |kLockedContended| just before the thread that owns the futex calls
    // into |FutexMigrate()| and issues |FUTEX_WAKE| on  waiters. That
    // would cause the current thread to miss the wake signal and sleep in the
    // kernel waiting for another thread to unlock the non-PI futex. But any
    // threads that want to acquire the lock in the future will see that lock
    // has been migrated by looking at |migrated_| and directly skip to
    // acquiring the PI futex, leaving the current thread waiting for the lock
    // forever.
    //
    // In order to overcome this, as part of the |FutexMigrate()| the non-PI
    // futex value is set to |kMigrated|. If after swapping the value of
    // non-PI futex with |kLockedContended|, the current thread sees that it
    // had previously been set to |kMigrated|, it knows that it has become the
    // unfortunate owner of a non-PI lock that has been migrated. But since
    // the lock has been marked as being contended, there might be another
    // thread that exchanged the value of |state_| with |kLockedContended|
    // just like the current thread but lost the race and is now waiting on
    // the non-PI futex. Since only the current thread is aware that this has
    // happened, it needs to repeat the migration process again the lock again
    // before trying to lock the PI-futex.
    switch (state_.exchange(kLockedContended, std::memory_order_acquire)) {
      case kUnlocked:
        return;
      case kLockedUncontended:
        [[fallthrough]];
      case kLockedContended:
        FutexWait();
        break;
      case kMigrated:
        FutexMigrate();
        break;
      default:
        PA_IMMEDIATE_CRASH();
    }
  }

  FutexLockPI();
}

#else  // PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)

void SpinningMutex::LockSlow() {
  // If this thread gets awaken but another one got the lock first, then go back
  // to sleeping. See comments in |FutexWait()| to see why a loop is required.
  while (state_.exchange(kLockedContended, std::memory_order_acquire) !=
         kUnlocked) {
    FutexWait();
  }
}

#endif  // PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)

#elif PA_BUILDFLAG(IS_WIN)

void SpinningMutex::LockSlow() {
  ::AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

#elif PA_BUILDFLAG(IS_APPLE)

// TODO(verwaest): We should use the constants from the header, but they aren't
// exposed until macOS 15. See their definition here:
// https://github.com/apple-oss-distributions/libplatform/blob/4f6349dfea579c35b8fa838d785644e441d14e0e/private/os/lock_private.h#L265
//
// The first flag prevents the runtime from creating more threads in response to
// contention. The second will spin in the kernel if the lock owner is currently
// running.
#define OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION 0x00010000
#define OS_UNFAIR_LOCK_ADAPTIVE_SPIN 0x00040000

typedef uint32_t os_unfair_lock_options_t;

extern "C" {
void __attribute__((weak))
os_unfair_lock_lock_with_options(os_unfair_lock* lock,
                                 os_unfair_lock_options_t);
}

void SpinningMutex::LockSlow() {
  if (os_unfair_lock_lock_with_options) {
    const os_unfair_lock_options_t options =
        static_cast<os_unfair_lock_options_t>(
            OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
    os_unfair_lock_lock_with_options(&unfair_lock_, options);
  } else {
    os_unfair_lock_lock(&unfair_lock_);
  }
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
