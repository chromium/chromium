// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/cancelable_event.h"

#include <errno.h>
#include <semaphore.h>

#include <tuple>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time.h"

namespace base {

namespace {
// Translates a base::TimeDelta (relative to now) to timespec containing
// that position in time relative to unix epoch
struct timespec TimeDeltaToAbsTimeSpec(base::TimeDelta time_delta) {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  struct timespec offset = time_delta.ToTimeSpec();
  now.tv_sec += offset.tv_sec;
  now.tv_nsec += offset.tv_nsec;
  if (now.tv_nsec >= Time::kNanosecondsPerSecond) {
    now.tv_sec++;
    now.tv_nsec -= Time::kNanosecondsPerSecond;
  }
  return now;
}
}  // namespace

CancelableEvent::CancelableEvent() {
  int result = sem_init(&native_handle_, 0, 0U);
  CHECK_EQ(result, 0);
}

CancelableEvent::~CancelableEvent() {
  int result = sem_destroy(&native_handle_);
  CHECK_EQ(result, 0);
}

void CancelableEvent::SignalImpl() {
  int result;
#if DCHECK_IS_ON()
  int sem_value = 0;
  result = sem_getvalue(&native_handle_, &sem_value);
  CHECK_EQ(result, 0);
  DCHECK_EQ(sem_value, 0);
#endif
  result = sem_post(&native_handle_);
  CHECK_EQ(result, 0);
}

bool CancelableEvent::CancelImpl() {
  int result = sem_trywait(&native_handle_);
  if (result == -1 && errno == EAGAIN) {
    return false;
  }
  if (result == 0) {
    return true;
  }

  PCHECK(false);
}

bool CancelableEvent::TimedWaitImpl(TimeDelta timeout) {
  int result;
  if (timeout.is_max()) {
    result = HANDLE_EINTR(sem_wait(&native_handle_));
  } else {
    // Compute the time for the end of the timeout.
    const struct timespec ts = TimeDeltaToAbsTimeSpec(timeout);

    // Wait for semaphore to be signalled or to timeout.
    result = HANDLE_EINTR(sem_timedwait(&native_handle_, &ts));
  }
  if (result == 0) {
    return true;  // The semaphore was signalled.
  }
  if (result == -1 && errno == ETIMEDOUT) {
    // Timed out while waiting for semaphore.
    return false;
  }
  PCHECK(false);
}

}  // namespace base
