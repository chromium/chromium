// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/synchronization/waitable_event.h"

#include <windows.h>

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace base {

namespace {
NOINLINE void ReportInvalidWaitableEventResult(DWORD result) {
  const auto last_error = ::GetLastError();
  base::debug::Alias(&last_error);
  base::debug::Alias(&result);
  base::debug::DumpWithoutCrashing();  // https://crbug.com/1478972.
}
}  // namespace

WaitableEvent::WaitableEvent(ResetPolicy reset_policy,
                             InitialState initial_state)
    : handle_(CreateEvent(nullptr,
                          reset_policy == ResetPolicy::MANUAL,
                          initial_state == InitialState::SIGNALED,
                          nullptr)) {
  // We're probably going to crash anyways if this is ever NULL, so we might as
  // well make our stack reports more informative by crashing here.
  CHECK(handle_.is_valid());
}

WaitableEvent::WaitableEvent(win::ScopedHandle handle)
    : handle_(std::move(handle)) {
  CHECK(handle_.is_valid()) << "Tried to create WaitableEvent from NULL handle";
}

void WaitableEvent::Reset() {
  ResetEvent(handle_.get());
}

void WaitableEvent::SignalImpl() {
  SetEvent(handle_.get());
}

bool WaitableEvent::IsSignaled() const {
  DWORD result = WaitForSingleObject(handle_.get(), 0);
  if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT) {
    ReportInvalidWaitableEventResult(result);
  }
  return result == WAIT_OBJECT_0;
}

bool WaitableEvent::TimedWaitImpl(TimeDelta wait_delta) {
  // TimeTicks takes care of overflow but we special case is_max() nonetheless
  // to avoid invoking TimeTicksNowIgnoringOverride() unnecessarily.
  // WaitForSingleObject(handle_.Get(), INFINITE) doesn't spuriously wakeup so
  // we don't need to worry about is_max() for the increment phase of the loop.
  const TimeTicks end_time =
      wait_delta.is_max() ? TimeTicks::Max()
                          : subtle::TimeTicksNowIgnoringOverride() + wait_delta;
  for (TimeDelta remaining = wait_delta; remaining.is_positive();
       remaining = end_time - subtle::TimeTicksNowIgnoringOverride()) {
    // Truncate the timeout to milliseconds, rounded up to avoid spinning
    // (either by returning too early or because a < 1ms timeout on Windows
    // tends to return immediately).
    const DWORD timeout_ms =
        remaining.is_max()
            ? INFINITE
            : saturated_cast<DWORD>(remaining.InMillisecondsRoundedUp());
    const DWORD result = WaitForSingleObject(handle_.get(), timeout_ms);
    if (result == WAIT_OBJECT_0) {
      // The object is signaled.
      return true;
    }

    if (result == WAIT_TIMEOUT) {
      // TimedWait can time out earlier than the specified |timeout| on
      // Windows. To make this consistent with the posix implementation we
      // should guarantee that TimedWait doesn't return earlier than the
      // specified |max_time| and wait again for the remaining time.
      continue;
    }

    // The only other documented result values are `WAIT_ABANDONED` and
    // `WAIT_FAILED`. Neither of these nor any other result should ever be
    // emitted unless there is a double free or another entity is tampering
    // with this instance's event handle. Only fails if the timeout was
    // INFINITE.
    if (wait_delta.is_max()) {
      ReportInvalidWaitableEventResult(result);
      // The code may infinite loop and then hang if the returned value
      // continues being `WAIT_FAILED`.
    }
  }
  return false;
}

// static
size_t WaitableEvent::WaitManyImpl(WaitableEvent** events, size_t count) {
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  CHECK_LE(count, static_cast<size_t>(MAXIMUM_WAIT_OBJECTS))
      << "Can only wait on " << MAXIMUM_WAIT_OBJECTS << " with WaitMany";

  for (size_t i = 0; i < count; ++i)
    handles[i] = events[i]->handle();

  // The cast is safe because count is small - see the CHECK above.
  DWORD result =
      WaitForMultipleObjects(static_cast<DWORD>(count),
                             handles,
                             FALSE,      // don't wait for all the objects
                             INFINITE);  // no timeout
  if (result >= WAIT_OBJECT_0 + count) {
    DPLOG(FATAL) << "WaitForMultipleObjects failed";
    return 0;
  }

  return result - WAIT_OBJECT_0;
}

}  // namespace base
