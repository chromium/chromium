// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include <threads.h>
#include <zircon/syscalls.h>
#include <zircon/threads.h>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/time/time_override.h"

namespace base {

// Time -----------------------------------------------------------------------

namespace subtle {
Time TimeNowIgnoringOverride() {
  timespec ts;
  int status = timespec_get(&ts, TIME_UTC);
  CHECK(status != 0);
  return Time::FromTimeSpec(ts);
}

Time TimeNowFromSystemTimeIgnoringOverride() {
  // Just use TimeNowIgnoringOverride() because it returns the system time.
  return TimeNowIgnoringOverride();
}
}  // namespace subtle

// TimeTicks ------------------------------------------------------------------

namespace subtle {
TimeTicks TimeTicksNowIgnoringOverride() {
  const zx_time_t nanos_since_boot = zx_clock_get_monotonic();
  CHECK_NE(0, nanos_since_boot);
  return TimeTicks::FromZxTime(nanos_since_boot);
}
}  // namespace subtle

// static
TimeDelta TimeDelta::FromZxDuration(zx_duration_t nanos) {
  return Nanoseconds(nanos);
}

zx_duration_t TimeDelta::ToZxDuration() const {
  return InNanoseconds();
}

// static
Time Time::FromZxTime(zx_time_t nanos_since_unix_epoch) {
  return UnixEpoch() + Nanoseconds(nanos_since_unix_epoch);
}

zx_time_t Time::ToZxTime() const {
  return (*this - UnixEpoch()).InNanoseconds();
}

// static
TimeTicks::Clock TimeTicks::GetClock() {
  return Clock::FUCHSIA_ZX_CLOCK_MONOTONIC;
}

// static
bool TimeTicks::IsHighResolution() {
  return true;
}

// static
bool TimeTicks::IsConsistentAcrossProcesses() {
  return true;
}

// static
TimeTicks TimeTicks::FromZxTime(zx_time_t nanos_since_boot) {
  return TimeTicks() + Nanoseconds(nanos_since_boot);
}

zx_time_t TimeTicks::ToZxTime() const {
  return (*this - TimeTicks()).InNanoseconds();
}

// ThreadTicks ----------------------------------------------------------------

namespace subtle {
ThreadTicks ThreadTicksNowIgnoringOverride() {
  zx_info_thread_stats_t info;
  zx_status_t status = zx_object_get_info(thrd_get_zx_handle(thrd_current()),
                                          ZX_INFO_THREAD_STATS, &info,
                                          sizeof(info), nullptr, nullptr);
  ZX_CHECK(status == ZX_OK, status);
  return ThreadTicks() + Nanoseconds(info.total_runtime);
}
}  // namespace subtle

}  // namespace base
