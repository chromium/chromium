// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "v8/src/base/platform/semaphore.cc" in v8.
// Keep in sync, especially when fixing bugs.

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/semaphore.h"

#include <dispatch/dispatch.h>

#include "base/check.h"
#include "base/time/time.h"

namespace base {
namespace internal {

Semaphore::Semaphore(int count) {
  native_handle_ = dispatch_semaphore_create(count);
  CHECK(native_handle_);
}

Semaphore::~Semaphore() {
  dispatch_release(native_handle_);
}

void Semaphore::Signal() {
  dispatch_semaphore_signal(native_handle_);
}

void Semaphore::Wait() {
  CHECK_EQ(dispatch_semaphore_wait(native_handle_, DISPATCH_TIME_FOREVER), 0);
}

bool Semaphore::TimedWait(TimeDelta timeout) {
  const dispatch_time_t wait_time =
      timeout.is_max()
          ? DISPATCH_TIME_FOREVER
          : dispatch_time(DISPATCH_TIME_NOW, timeout.InNanoseconds());
  return dispatch_semaphore_wait(native_handle_, wait_time) == 0;
}

}  // namespace internal
}  // namespace base
