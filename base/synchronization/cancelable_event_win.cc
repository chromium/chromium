// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/cancelable_event.h"

#include <windows.h>

#include <synchapi.h>

#include <tuple>

#include "base/synchronization/lock.h"
#include "base/win/winbase_shim.h"

namespace base {

CancelableEvent::CancelableEvent() {
  native_handle_ = ::CreateSemaphoreA(nullptr, 0, 1, nullptr);
  PCHECK(!!native_handle_);
}

CancelableEvent::~CancelableEvent() {
  const bool result = ::CloseHandle(native_handle_);
  PCHECK(result);
}

void CancelableEvent::SignalImpl() {
  LONG prev_count;
  const bool result = ::ReleaseSemaphore(native_handle_, 1, &prev_count);
  PCHECK(result);
  CHECK_EQ(prev_count, 0);
}

bool CancelableEvent::CancelImpl() {
  const DWORD result = ::WaitForSingleObject(native_handle_, 0);
  return result == WAIT_OBJECT_0;
}

bool CancelableEvent::TimedWaitImpl(TimeDelta timeout) {
  const DWORD wait_ms = saturated_cast<DWORD>(timeout.InMilliseconds());
  const TimeTicks start = TimeTicks::Now();
  DWORD result;
  // WaitForSingleObject has been shown to experience spurious wakeups (on the
  // order of 10ms before when it was supposed to wake up), so retry until at
  // least `timeout` has passed.
  do {
    result = ::WaitForSingleObject(native_handle_, wait_ms);
    if (result == WAIT_OBJECT_0) {
      return true;
    }
  } while (TimeTicks::Now() <= start + timeout);
  CHECK_EQ(result, static_cast<DWORD>(WAIT_TIMEOUT));
  return false;
}

}  // namespace base
