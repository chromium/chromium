// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/time/time.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <ostream>
#include <tuple>
#include <utility>

#include "partition_alloc/partition_alloc_base/time/time_override.h"

namespace partition_alloc::internal::base {

namespace internal {

std::atomic<TimeNowFunction> g_time_now_function{
    &subtle::TimeNowIgnoringOverride};

std::atomic<TimeNowFunction> g_time_now_from_system_time_function{
    &subtle::TimeNowFromSystemTimeIgnoringOverride};

std::atomic<TimeTicksNowFunction> g_time_ticks_now_function{
    &subtle::TimeTicksNowIgnoringOverride};

std::atomic<ThreadTicksNowFunction> g_thread_ticks_now_function{
    &subtle::ThreadTicksNowIgnoringOverride};

}  // namespace internal

// TimeDelta ------------------------------------------------------------------

int TimeDelta::InDays() const {
  if (!is_inf()) {
    return static_cast<int>(delta_ / Time::kMicrosecondsPerDay);
  }
  return (delta_ < 0) ? std::numeric_limits<int>::min()
                      : std::numeric_limits<int>::max();
}

int TimeDelta::InDaysFloored() const {
  if (!is_inf()) {
    const int result = delta_ / Time::kMicrosecondsPerDay;
    // Convert |result| from truncating to flooring.
    return (result * Time::kMicrosecondsPerDay > delta_) ? (result - 1)
                                                         : result;
  }
  return (delta_ < 0) ? std::numeric_limits<int>::min()
                      : std::numeric_limits<int>::max();
}

double TimeDelta::InMillisecondsF() const {
  if (!is_inf()) {
    return static_cast<double>(delta_) / Time::kMicrosecondsPerMillisecond;
  }
  return (delta_ < 0) ? -std::numeric_limits<double>::infinity()
                      : std::numeric_limits<double>::infinity();
}

int64_t TimeDelta::InMilliseconds() const {
  if (!is_inf()) {
    return delta_ / Time::kMicrosecondsPerMillisecond;
  }
  return (delta_ < 0) ? std::numeric_limits<int64_t>::min()
                      : std::numeric_limits<int64_t>::max();
}

int64_t TimeDelta::InMillisecondsRoundedUp() const {
  if (!is_inf()) {
    const int64_t result = delta_ / Time::kMicrosecondsPerMillisecond;
    // Convert |result| from truncating to ceiling.
    return (delta_ > result * Time::kMicrosecondsPerMillisecond) ? (result + 1)
                                                                 : result;
  }
  return delta_;
}

double TimeDelta::InMicrosecondsF() const {
  if (!is_inf()) {
    return static_cast<double>(delta_);
  }
  return (delta_ < 0) ? -std::numeric_limits<double>::infinity()
                      : std::numeric_limits<double>::infinity();
}

TimeDelta TimeDelta::CeilToMultiple(TimeDelta interval) const {
  if (is_inf() || interval.is_zero()) {
    return *this;
  }
  const TimeDelta remainder = *this % interval;
  if (delta_ < 0) {
    return *this - remainder;
  }
  return remainder.is_zero() ? *this
                             : (*this - remainder + interval.magnitude());
}

TimeDelta TimeDelta::FloorToMultiple(TimeDelta interval) const {
  if (is_inf() || interval.is_zero()) {
    return *this;
  }
  const TimeDelta remainder = *this % interval;
  if (delta_ < 0) {
    return remainder.is_zero() ? *this
                               : (*this - remainder - interval.magnitude());
  }
  return *this - remainder;
}

TimeDelta TimeDelta::RoundToMultiple(TimeDelta interval) const {
  if (is_inf() || interval.is_zero()) {
    return *this;
  }
  if (interval.is_inf()) {
    return TimeDelta();
  }
  const TimeDelta half = interval.magnitude() / 2;
  return (delta_ < 0) ? (*this - half).CeilToMultiple(interval)
                      : (*this + half).FloorToMultiple(interval);
}

// Time -----------------------------------------------------------------------

// static
Time Time::Now() {
  return internal::g_time_now_function.load(std::memory_order_relaxed)();
}

// static
Time Time::NowFromSystemTime() {
  // Just use g_time_now_function because it returns the system time.
  return internal::g_time_now_from_system_time_function.load(
      std::memory_order_relaxed)();
}

time_t Time::ToTimeT() const {
  if (is_null()) {
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  }
  if (!is_inf() && ((std::numeric_limits<int64_t>::max() -
                     kTimeTToMicrosecondsOffset) > us_)) {
    return (*this - UnixEpoch()).InSeconds();
  }
  return (us_ < 0) ? std::numeric_limits<time_t>::min()
                   : std::numeric_limits<time_t>::max();
}

// static
Time Time::FromSecondsSinceUnixEpoch(double dt) {
  // Preserve 0 so we can tell it doesn't exist.
  return (dt == 0 || std::isnan(dt)) ? Time() : (UnixEpoch() + Seconds(dt));
}

double Time::InSecondsFSinceUnixEpoch() const {
  if (is_null()) {
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  }
  if (!is_inf()) {
    return (*this - UnixEpoch()).InSecondsF();
  }
  return (us_ < 0) ? -std::numeric_limits<double>::infinity()
                   : std::numeric_limits<double>::infinity();
}

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
// static
Time Time::FromTimeSpec(const timespec& ts) {
  return FromSecondsSinceUnixEpoch(ts.tv_sec + static_cast<double>(ts.tv_nsec) /
                                                   kNanosecondsPerSecond);
}
#endif

// static
Time Time::FromMillisecondsSinceUnixEpoch(double ms_since_epoch) {
  // The epoch is a valid time, so this constructor doesn't interpret 0 as the
  // null time.
  return UnixEpoch() + Milliseconds(ms_since_epoch);
}

double Time::InMillisecondsFSinceUnixEpoch() const {
  // Preserve 0 so the invalid result doesn't depend on the platform.
  return is_null() ? 0 : InMillisecondsFSinceUnixEpochIgnoringNull();
}

double Time::InMillisecondsFSinceUnixEpochIgnoringNull() const {
  // Preserve max and min without offset to prevent over/underflow.
  if (!is_inf()) {
    return (*this - UnixEpoch()).InMillisecondsF();
  }
  return (us_ < 0) ? -std::numeric_limits<double>::infinity()
                   : std::numeric_limits<double>::infinity();
}

Time Time::FromMillisecondsSinceUnixEpoch(int64_t ms_since_epoch) {
  return UnixEpoch() + Milliseconds(ms_since_epoch);
}

int64_t Time::InMillisecondsSinceUnixEpoch() const {
  // Preserve 0 so the invalid result doesn't depend on the platform.
  if (is_null()) {
    return 0;
  }
  if (!is_inf()) {
    return (*this - UnixEpoch()).InMilliseconds();
  }
  return (us_ < 0) ? std::numeric_limits<int64_t>::min()
                   : std::numeric_limits<int64_t>::max();
}

// static
bool Time::FromMillisecondsSinceUnixEpoch(int64_t unix_milliseconds,
                                          Time* time) {
  // Adjust the provided time from milliseconds since the Unix epoch (1970) to
  // microseconds since the Windows epoch (1601), avoiding overflows.
  CheckedNumeric<int64_t> checked_microseconds_win_epoch = unix_milliseconds;
  checked_microseconds_win_epoch *= kMicrosecondsPerMillisecond;
  checked_microseconds_win_epoch += kTimeTToMicrosecondsOffset;
  *time = Time(checked_microseconds_win_epoch.ValueOrDefault(0));
  return checked_microseconds_win_epoch.IsValid();
}

int64_t Time::ToRoundedDownMillisecondsSinceUnixEpoch() const {
  constexpr int64_t kEpochOffsetMillis =
      kTimeTToMicrosecondsOffset / kMicrosecondsPerMillisecond;
  static_assert(kTimeTToMicrosecondsOffset % kMicrosecondsPerMillisecond == 0,
                "assumption: no epoch offset sub-milliseconds");

  // Compute the milliseconds since UNIX epoch without the possibility of
  // under/overflow. Round the result towards -infinity.
  //
  // If |us_| is negative and includes fractions of a millisecond, subtract one
  // more to effect the round towards -infinity. C-style integer truncation
  // takes care of all other cases.
  const int64_t millis = us_ / kMicrosecondsPerMillisecond;
  const int64_t submillis = us_ % kMicrosecondsPerMillisecond;
  return millis - kEpochOffsetMillis - (submillis < 0);
}

// TimeTicks ------------------------------------------------------------------

// static
TimeTicks TimeTicks::Now() {
  return internal::g_time_ticks_now_function.load(std::memory_order_relaxed)();
}

// static
TimeTicks TimeTicks::UnixEpoch() {
  static const TimeTicks epoch([] {
    return subtle::TimeTicksNowIgnoringOverride() -
           (subtle::TimeNowIgnoringOverride() - Time::UnixEpoch());
  }());
  return epoch;
}

TimeTicks TimeTicks::SnappedToNextTick(TimeTicks tick_phase,
                                       TimeDelta tick_interval) const {
  // |interval_offset| is the offset from |this| to the next multiple of
  // |tick_interval| after |tick_phase|, possibly negative if in the past.
  TimeDelta interval_offset = (tick_phase - *this) % tick_interval;
  // If |this| is exactly on the interval (i.e. offset==0), don't adjust.
  // Otherwise, if |tick_phase| was in the past, adjust forward to the next
  // tick after |this|.
  if (!interval_offset.is_zero() && tick_phase < *this) {
    interval_offset += tick_interval;
  }
  return *this + interval_offset;
}

// ThreadTicks ----------------------------------------------------------------

// static
ThreadTicks ThreadTicks::Now() {
  return internal::g_thread_ticks_now_function.load(
      std::memory_order_relaxed)();
}

}  // namespace partition_alloc::internal::base
