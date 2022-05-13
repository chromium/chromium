// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/threading/platform_thread.h"

#include <stddef.h>

#include "base/allocator/buildflags.h"
#include "base/time/time.h"

#include <windows.h>

namespace partition_alloc::internal::base {

// static
PlatformThreadId PlatformThread::CurrentId() {
  return ::GetCurrentThreadId();
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(::GetCurrentThreadId());
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  // When measured with a high resolution clock, Sleep() sometimes returns much
  // too early. We may need to call it repeatedly to get the desired duration.
  // PlatformThread::Sleep doesn't support mock-time, so this always uses
  // real-time.
  const TimeTicks end = TimeTicksNowIgnoringOverride() + duration;
  for (TimeTicks now = TimeTicksNowIgnoringOverride(); now < end;
       now = TimeTicksNowIgnoringOverride()) {
    ::Sleep(static_cast<DWORD>((end - now).InMillisecondsRoundedUp()));
  }
}

}  // namespace partition_alloc::internal::base
