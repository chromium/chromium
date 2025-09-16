// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_impl.h"

#include <pthread.h>

#include <atomic>
#include <cstdint>
#include <ostream>
#include <string>

#include "base/check_op.h"
#include "base/posix/safe_strerror.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/lock_metrics_recorder.h"
#include "base/synchronization/synchronization_buildflags.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/background_thread_pool_field_trial.h"
#endif


#if BUILDFLAG(IS_ANDROID)
// On Android, `pthread_mutexattr_setprotocol()` is only defined in bionic
// starting with API level 28. Make it a weak import, so that we can compile.
extern "C" {
int __attribute__((weak)) pthread_mutexattr_setprotocol(
    pthread_mutexattr_t* _Nonnull __attr,
    int __protocol);
}
#endif

namespace base {

namespace internal {
namespace {

#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE) && BUILDFLAG(IS_ANDROID)
bool IsMutexPriorityInheritanceEnabled() {
  return
#pragma clang diagnostic push  // Can be removed once our min-sdk is >= 28.
#pragma clang diagnostic ignored "-Wunguarded-availability"
      (pthread_mutexattr_setprotocol != nullptr) &&
#pragma clang diagnostic pop
      base::android::BackgroundThreadPoolFieldTrial::
          ShouldUsePriorityInheritanceLocks();
}
#endif  //  BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE) &&
        //  BUILDFLAG(IS_ANDROID)

#if DCHECK_IS_ON()
const char* AdditionalHintForSystemErrorCode(int error_code) {
  switch (error_code) {
    case EINVAL:
      return "Hint: This is often related to a use-after-free.";
    default:
      return "";
  }
}
#endif  // DCHECK_IS_ON()

std::string SystemErrorCodeToString(int error_code) {
#if DCHECK_IS_ON()
  return base::safe_strerror(error_code) + ". " +
         AdditionalHintForSystemErrorCode(error_code);
#else   // DCHECK_IS_ON()
  return std::string();
#endif  // DCHECK_IS_ON()
}

}  // namespace

#if DCHECK_IS_ON()
// These are out-of-line so that the .h file doesn't have to include ostream.
void dcheck_trylock_result(int rv) {
  DCHECK(rv == 0 || rv == EBUSY)
      << ". " << base::internal::SystemErrorCodeToString(rv);
}

void dcheck_unlock_result(int rv) {
  DCHECK_EQ(rv, 0) << ". " << strerror(rv);
}
#endif

// Determines which platforms can consider using priority inheritance locks. Use
// this define for platform code that may not compile if priority inheritance
// locks aren't available. For this platform code,
// PRIORITY_INHERITANCE_LOCKS_POSSIBLE() is a necessary but insufficient check.
// Lock::PriorityInheritanceAvailable still must be checked as the code may
// compile but the underlying platform still may not correctly support priority
// inheritance locks.
#if BUILDFLAG(IS_FUCHSIA)
#define PRIORITY_INHERITANCE_LOCKS_POSSIBLE() 0
#else
#define PRIORITY_INHERITANCE_LOCKS_POSSIBLE() 1
#endif

LockImpl::LockImpl() {
  pthread_mutexattr_t mta;
  int rv = pthread_mutexattr_init(&mta);
  DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
#if PRIORITY_INHERITANCE_LOCKS_POSSIBLE()
  if (PriorityInheritanceAvailable()) {
#pragma clang diagnostic push  // Can be removed once our min-sdk is >= 28.
#pragma clang diagnostic ignored "-Wunguarded-availability"
    rv = pthread_mutexattr_setprotocol(&mta, PTHREAD_PRIO_INHERIT);
#pragma clang diagnostic pop
    DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
  }
#endif
#ifndef NDEBUG
  // In debug, setup attributes for lock error checking.
  rv = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_ERRORCHECK);
  DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
#endif
  rv = pthread_mutex_init(&native_handle_, &mta);
  DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
  rv = pthread_mutexattr_destroy(&mta);
  DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
}

LockImpl::~LockImpl() {
  int rv = pthread_mutex_destroy(&native_handle_);
  DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
}

void LockImpl::LockInternal() {
  LockMetricsRecorder::ScopedLockAcquisitionTimer timer;
  int rv = pthread_mutex_lock(&native_handle_);
  DCHECK_EQ(rv, 0) << ". " << SystemErrorCodeToString(rv);
}

// static
bool LockImpl::PriorityInheritanceAvailable() {
#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE) && BUILDFLAG(IS_ANDROID)
  return IsMutexPriorityInheritanceEnabled();
#elif PRIORITY_INHERITANCE_LOCKS_POSSIBLE() && BUILDFLAG(IS_APPLE)
  return true;
#else
  // Security concerns prevent the use of priority inheritance mutexes on Linux.
  //   * CVE-2010-0622 - Linux < 2.6.33-rc7, wake_futex_pi possible DoS.
  //     https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2010-0622
  //   * CVE-2012-6647 - Linux < 3.5.1, futex_wait_requeue_pi possible DoS.
  //     https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2012-6647
  //   * CVE-2014-3153 - Linux <= 3.14.5, futex_requeue, privilege escalation.
  //     https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2014-3153
  //
  // If the above were all addressed, we still need a runtime check to deal with
  // the bug below.
  //   * glibc Bug 14652: https://sourceware.org/bugzilla/show_bug.cgi?id=14652
  //     Fixed in glibc 2.17.
  //     Priority inheritance mutexes may deadlock with condition variables
  //     during reacquisition of the mutex after the condition variable is
  //     signalled.
  return false;
#endif
}

}  // namespace internal

bool KernelSupportsPriorityInheritanceFutex() {
  // https://android-review.googlesource.com/c/3481472 which fixes priority
  // inheritance using rt-mutexes in the kernel landed in the 6.12.13 android
  // kernel and was backported to the 6.1.75 and 6.6.29 kernels. This change
  // hasn't been upstreamed yet.
#if BUILDFLAG(IS_ANDROID)
  static bool supports_pi_futex = [] {
    auto kernel_version = SysInfo::KernelVersionNumber::Current();
    return (kernel_version > SysInfo::KernelVersionNumber(6, 12, 13)) ||
           ((kernel_version > SysInfo::KernelVersionNumber(6, 6, 29)) &&
            (kernel_version < SysInfo::KernelVersionNumber(6, 6, INT32_MAX))) ||
           ((kernel_version > SysInfo::KernelVersionNumber(6, 1, 75)) &&
            (kernel_version < SysInfo::KernelVersionNumber(6, 1, INT32_MAX)));
  }();
  return supports_pi_futex;
#else   // BUILDFLAG(IS_ANDROID)
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace base
