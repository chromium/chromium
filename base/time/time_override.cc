// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time_override.h"

namespace base {
namespace subtle {

// static
bool ScopedTimeClockOverrides::overrides_active_ = false;

ScopedTimeClockOverrides::ScopedTimeClockOverrides(
    TimeNowFunction time_override,
    TimeTicksNowFunction time_ticks_override,
    ThreadTicksNowFunction thread_ticks_override) {
  DCHECK(!overrides_active_);
  overrides_active_ = true;
  if (time_override) {
    internal::g_time_now_function.store(time_override,
                                        std::memory_order_relaxed);
    internal::g_time_now_from_system_time_function.store(
        time_override, std::memory_order_relaxed);
  }
  if (time_ticks_override) {
    internal::g_time_ticks_now_function.store(time_ticks_override,
                                              std::memory_order_relaxed);
  }
  if (thread_ticks_override) {
    internal::g_thread_ticks_now_function.store(thread_ticks_override,
                                                std::memory_order_relaxed);
  }
}

ScopedTimeClockOverrides::~ScopedTimeClockOverrides() {
  internal::g_time_now_function.store(&TimeNowIgnoringOverride);
  internal::g_time_now_from_system_time_function.store(
      &TimeNowFromSystemTimeIgnoringOverride);
  internal::g_time_ticks_now_function.store(&TimeTicksNowIgnoringOverride);
  internal::g_thread_ticks_now_function.store(&ThreadTicksNowIgnoringOverride);
  overrides_active_ = false;
}

}  // namespace subtle
}  // namespace base
