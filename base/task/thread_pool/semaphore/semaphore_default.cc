// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "v8/src/base/platform/semaphore.cc" in v8.
// Keep in sync, especially when fixing bugs.

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/semaphore.h"

#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {
namespace internal {

Semaphore::Semaphore(int count) : native_handle_(count) {
  native_handle().condition_var.declare_only_used_while_idle();
}

Semaphore::~Semaphore() = default;

void Semaphore::Signal() {
  AutoLock lock(native_handle().lock);
  ++native_handle().value;
  native_handle().condition_var.Signal();
}

void Semaphore::Wait() {
  AutoLock lock(native_handle().lock);
  while (native_handle().value < 1) {
    native_handle().condition_var.Wait();
  }
  --native_handle().value;
}

bool Semaphore::TimedWait(TimeDelta timeout) {
  AutoLock lock(native_handle().lock);
  const TimeTicks before_wait = TimeTicks::Now();
  const TimeTicks wait_end = before_wait + timeout;
  TimeDelta remaining_sleep = timeout;
  while (native_handle().value < 1) {
    native_handle().condition_var.TimedWait(remaining_sleep);

    // Since condition variables experience spurious wakeups, adjust the
    // remaining wait time to prepare for sleeping once more, and return if a
    // timeout occurred.
    remaining_sleep = wait_end - TimeTicks::Now();
    if (!remaining_sleep.is_positive()) {
      return false;
    }
  }
  // In this case, the lock has been successfully acquired with a positive
  // semaphore value.
  --native_handle().value;
  return true;
}

}  // namespace internal
}  // namespace base
