// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "v8/src/base/platform/semaphore.cc" in v8.
// Keep in sync, especially when fixing bugs.

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/semaphore.h"

#include <errno.h>
#include <semaphore.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time.h"

namespace base {
namespace internal {

namespace {
// Translates a base::TimeDelta (relative to now) to struct timedelta containing
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

Semaphore::Semaphore(int count) {
  CHECK_GE(count, 0);
  int result = sem_init(&native_handle_, 0, static_cast<unsigned int>(count));
  CHECK_EQ(result, 0);
}

Semaphore::~Semaphore() {
  int result = sem_destroy(&native_handle_);
  CHECK_EQ(result, 0);
}

void Semaphore::Signal() {
  int result = sem_post(&native_handle_);
  CHECK_EQ(result, 0);
}

void Semaphore::Wait() {
  int result = HANDLE_EINTR(sem_wait(&native_handle_));
  if (result == 0) {
    return;  // Semaphore was signalled.
  }
  PCHECK(false);
}

bool Semaphore::TimedWait(TimeDelta timeout) {
  if (timeout.is_max()) {
    Wait();
    return true;
  }

  // Compute the time for end of timeout.
  const struct timespec ts = TimeDeltaToAbsTimeSpec(timeout);

  // Wait for semaphore signalled or timeout.
  int result = HANDLE_EINTR(sem_timedwait(&native_handle_, &ts));
  if (result == 0) {
    return true;  // Semaphore was signalled.
  }
  if (result == -1 && errno == ETIMEDOUT) {
    // Timed out while waiting for semaphore.
    return false;
  }
  PCHECK(false);
  return false;
}

}  // namespace internal
}  // namespace base
