// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/condition_variable.h"

#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

#include <optional>

#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <atomic>

#include "base/feature_list.h"
#endif

#if BUILDFLAG(IS_ANDROID) && __ANDROID_API__ < 21
#define HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC 1
#endif

namespace {
#if BUILDFLAG(IS_APPLE)
// Under this feature a hack that was introduced to avoid crashes is skipped.
// Use to evaluate if the hack is still needed. See https://crbug.com/517681.
BASE_FEATURE(kSkipConditionVariableWakeupHack,
             "SkipConditionVariableWakeupHack",
             base::FEATURE_ENABLED_BY_DEFAULT);
std::atomic_bool g_skip_wakeup_hack = true;
#endif
}  // namespace

namespace base {

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_.native_handle())
#if DCHECK_IS_ON()
    , user_lock_(user_lock)
#endif
{
  int rv = 0;
  // http://crbug.com/293736
  // NaCl doesn't support monotonic clock based absolute deadlines.
  // On older Android platform versions, it's supported through the
  // non-standard pthread_cond_timedwait_monotonic_np. Newer platform
  // versions have pthread_condattr_setclock.
  // Mac can use relative time deadlines.
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && \
    !defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC)
  pthread_condattr_t attrs;
  rv = pthread_condattr_init(&attrs);
  DCHECK_EQ(0, rv);
  pthread_condattr_setclock(&attrs, CLOCK_MONOTONIC);
  rv = pthread_cond_init(&condition_, &attrs);
  pthread_condattr_destroy(&attrs);
#else
  rv = pthread_cond_init(&condition_, NULL);
#endif
  DCHECK_EQ(0, rv);
}

ConditionVariable::~ConditionVariable() {
#if BUILDFLAG(IS_APPLE)
  // This hack is necessary to avoid a fatal pthreads subsystem bug in the
  // Darwin kernel. http://crbug.com/517681.
  if (!g_skip_wakeup_hack.load(std::memory_order_relaxed)) {
    base::Lock lock;
    base::AutoLock l(lock);
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1;
    pthread_cond_timedwait_relative_np(&condition_, lock.lock_.native_handle(),
                                       &ts);
  }
#endif

  int rv = pthread_cond_destroy(&condition_);
  DCHECK_EQ(0, rv);
}

#if BUILDFLAG(IS_APPLE)
// static
void ConditionVariable::InitializeFeatures() {
  g_skip_wakeup_hack.store(
      base::FeatureList::IsEnabled(kSkipConditionVariableWakeupHack),
      std::memory_order_relaxed);
}
#endif

void ConditionVariable::Wait() {
  std::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (waiting_is_blocking_)
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif
  int rv = pthread_cond_wait(&condition_, user_mutex_);
  DCHECK_EQ(0, rv);
#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  std::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (waiting_is_blocking_)
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);

  int64_t usecs = max_time.InMicroseconds();
  struct timespec relative_time;
  relative_time.tv_sec =
      static_cast<time_t>(usecs / Time::kMicrosecondsPerSecond);
  relative_time.tv_nsec =
      (usecs % Time::kMicrosecondsPerSecond) * Time::kNanosecondsPerMicrosecond;

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif

#if BUILDFLAG(IS_APPLE)
  int rv = pthread_cond_timedwait_relative_np(
      &condition_, user_mutex_, &relative_time);
#else
  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timespec absolute_time;
#if BUILDFLAG(IS_NACL)
  // See comment in constructor for why this is different in NaCl.
  struct timeval now;
  gettimeofday(&now, NULL);
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_usec * Time::kNanosecondsPerMicrosecond;
#else
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_nsec;
#endif

  absolute_time.tv_sec += relative_time.tv_sec;
  absolute_time.tv_nsec += relative_time.tv_nsec;
  absolute_time.tv_sec += absolute_time.tv_nsec / Time::kNanosecondsPerSecond;
  absolute_time.tv_nsec %= Time::kNanosecondsPerSecond;
  DCHECK_GE(absolute_time.tv_sec, now.tv_sec);  // Overflow paranoia

#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC)
  int rv = pthread_cond_timedwait_monotonic_np(
      &condition_, user_mutex_, &absolute_time);
#else
  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &absolute_time);
#endif  // HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC
#endif  // BUILDFLAG(IS_APPLE)

  // On failure, we only expect the CV to timeout. Any other error value means
  // that we've unexpectedly woken up.
  DCHECK(rv == 0 || rv == ETIMEDOUT);
#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::Broadcast() {
  int rv = pthread_cond_broadcast(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Signal() {
  int rv = pthread_cond_signal(&condition_);
  DCHECK_EQ(0, rv);
}

}  // namespace base
