// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "v8/src/base/platform/semaphore.cc" in v8.
// Keep in sync, especially when fixing bugs.

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/semaphore.h"

#include <windows.h>

#include "base/check.h"
#include "base/time/time.h"

namespace base {
namespace internal {

Semaphore::Semaphore(int count) {
  CHECK_GE(count, 0);
  native_handle_ = ::CreateSemaphoreA(
      nullptr, count, std::numeric_limits<LONG>::max(), nullptr);
  CHECK(!!native_handle_);
}

Semaphore::~Semaphore() {
  const bool result = CloseHandle(native_handle_);
  CHECK(result);
}

void Semaphore::Signal() {
  const bool result = ReleaseSemaphore(native_handle_, 1, nullptr);
  CHECK(result);
}

void Semaphore::Wait() {
  const DWORD result = WaitForSingleObject(native_handle_, INFINITE);
  CHECK_EQ(result, WAIT_OBJECT_0);
}

bool Semaphore::TimedWait(TimeDelta timeout) {
  const DWORD wait_ms = checked_cast<DWORD>(timeout.InMilliseconds());
  const TimeTicks start = TimeTicks::Now();
  DWORD result;
  // WaitForSingleObject has been shown to experience spurious wakeups (on the
  // order of 10ms before when it was supposed to wake up), so retry until at
  // least |timeout| has passed.
  do {
    result = WaitForSingleObject(native_handle_, wait_ms);
    if (result == WAIT_OBJECT_0) {
      return true;
    }
  } while (TimeTicks::Now() <= start + timeout);
  CHECK_EQ(result, static_cast<DWORD>(WAIT_TIMEOUT));
  return false;
}

}  // namespace internal
}  // namespace base
