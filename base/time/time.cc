// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#if defined(OS_LINUX)
// time.h is a widely included header and its size impacts build time.
// Try not to raise this limit unless necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#pragma clang max_tokens_here 390000
#endif  // defined(OS_LINUX)

#include <atomic>
#include <cmath>
#include <limits>
#include <ostream>
#include <tuple>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/third_party/nspr/prtime.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

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
  if (!is_inf())
    return static_cast<int>(delta_ / Time::kMicrosecondsPerDay);
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
  if (!is_inf())
    return static_cast<double>(delta_) / Time::kMicrosecondsPerMillisecond;
  return (delta_ < 0) ? -std::numeric_limits<double>::infinity()
                      : std::numeric_limits<double>::infinity();
}

int64_t TimeDelta::InMilliseconds() const {
  if (!is_inf())
    return delta_ / Time::kMicrosecondsPerMillisecond;
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
  if (!is_inf())
    return static_cast<double>(delta_);
  return (delta_ < 0) ? -std::numeric_limits<double>::infinity()
                      : std::numeric_limits<double>::infinity();
}

TimeDelta TimeDelta::CeilToMultiple(TimeDelta interval) const {
  if (is_inf() || interval.is_zero())
    return *this;
  const TimeDelta remainder = *this % interval;
  if (delta_ < 0)
    return *this - remainder;
  return remainder.is_zero() ? *this
                             : (*this - remainder + interval.magnitude());
}

TimeDelta TimeDelta::FloorToMultiple(TimeDelta interval) const {
  if (is_inf() || interval.is_zero())
    return *this;
  const TimeDelta remainder = *this % interval;
  if (delta_ < 0) {
    return remainder.is_zero() ? *this
                               : (*this - remainder - interval.magnitude());
  }
  return *this - remainder;
}

TimeDelta TimeDelta::RoundToMultiple(TimeDelta interval) const {
  if (is_inf() || interval.is_zero())
    return *this;
  if (interval.is_inf())
    return TimeDelta();
  const TimeDelta half = interval.magnitude() / 2;
  return (delta_ < 0) ? (*this - half).CeilToMultiple(interval)
                      : (*this + half).FloorToMultiple(interval);
}

std::ostream& operator<<(std::ostream& os, TimeDelta time_delta) {
  return os << time_delta.InSecondsF() << " s";
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

// static
Time Time::FromDeltaSinceWindowsEpoch(TimeDelta delta) {
  return Time(delta.InMicroseconds());
}

TimeDelta Time::ToDeltaSinceWindowsEpoch() const {
  return TimeDelta::FromMicroseconds(us_);
}

// static
Time Time::FromTimeT(time_t tt) {
  if (tt == 0)
    return Time();  // Preserve 0 so we can tell it doesn't exist.
  return (tt == std::numeric_limits<time_t>::max())
             ? Max()
             : (UnixEpoch() + TimeDelta::FromSeconds(tt));
}

time_t Time::ToTimeT() const {
  if (is_null())
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  if (!is_inf() && ((std::numeric_limits<int64_t>::max() -
                     kTimeTToMicrosecondsOffset) > us_))
    return (*this - UnixEpoch()).InSeconds();
  return (us_ < 0) ? std::numeric_limits<time_t>::min()
                   : std::numeric_limits<time_t>::max();
}

// static
Time Time::FromDoubleT(double dt) {
  // Preserve 0 so we can tell it doesn't exist.
  return (dt == 0 || std::isnan(dt))
             ? Time()
             : (UnixEpoch() + TimeDelta::FromSecondsD(dt));
}

double Time::ToDoubleT() const {
  if (is_null())
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  if (!is_inf())
    return (*this - UnixEpoch()).InSecondsF();
  return (us_ < 0) ? -std::numeric_limits<double>::infinity()
                   : std::numeric_limits<double>::infinity();
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
// static
Time Time::FromTimeSpec(const timespec& ts) {
  return FromDoubleT(ts.tv_sec +
                     static_cast<double>(ts.tv_nsec) / kNanosecondsPerSecond);
}
#endif

// static
Time Time::FromJsTime(double ms_since_epoch) {
  // The epoch is a valid time, so this constructor doesn't interpret 0 as the
  // null time.
  return UnixEpoch() + TimeDelta::FromMillisecondsD(ms_since_epoch);
}

double Time::ToJsTime() const {
  // Preserve 0 so the invalid result doesn't depend on the platform.
  return is_null() ? 0 : ToJsTimeIgnoringNull();
}

double Time::ToJsTimeIgnoringNull() const {
  // Preserve max and min without offset to prevent over/underflow.
  if (!is_inf())
    return (*this - UnixEpoch()).InMillisecondsF();
  return (us_ < 0) ? -std::numeric_limits<double>::infinity()
                   : std::numeric_limits<double>::infinity();
}

Time Time::FromJavaTime(int64_t ms_since_epoch) {
  return UnixEpoch() + TimeDelta::FromMilliseconds(ms_since_epoch);
}

int64_t Time::ToJavaTime() const {
  // Preserve 0 so the invalid result doesn't depend on the platform.
  if (is_null())
    return 0;
  if (!is_inf())
    return (*this - UnixEpoch()).InMilliseconds();
  return (us_ < 0) ? std::numeric_limits<int64_t>::min()
                   : std::numeric_limits<int64_t>::max();
}

// static
Time Time::UnixEpoch() {
  return Time(kTimeTToMicrosecondsOffset);
}

Time Time::Midnight(bool is_local) const {
  Exploded exploded;
  Explode(is_local, &exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  Time out_time;
  if (FromExploded(is_local, exploded, &out_time))
    return out_time;

  // Reaching here means 00:00:00am of the current day does not exist (due to
  // Daylight Saving Time in some countries where clocks are shifted at
  // midnight). In this case, midnight should be defined as 01:00:00am.
  DCHECK(is_local);
  exploded.hour = 1;
  const bool result = FromExploded(is_local, exploded, &out_time);
  DCHECK(result);  // This function must not fail.
  return out_time;
}

// static
bool Time::FromStringInternal(const char* time_string,
                              bool is_local,
                              Time* parsed_time) {
  DCHECK(time_string);
  DCHECK(parsed_time);

  if (time_string[0] == '\0')
    return false;

  PRTime result_time = 0;
  PRStatus result = PR_ParseTimeString(time_string,
                                       is_local ? PR_FALSE : PR_TRUE,
                                       &result_time);
  if (result != PR_SUCCESS)
    return false;

  *parsed_time = UnixEpoch() + TimeDelta::FromMicroseconds(result_time);
  return true;
}

// static
bool Time::ExplodedMostlyEquals(const Exploded& lhs, const Exploded& rhs) {
  return std::tie(lhs.year, lhs.month, lhs.day_of_month, lhs.hour, lhs.minute,
                  lhs.second, lhs.millisecond) ==
         std::tie(rhs.year, rhs.month, rhs.day_of_month, rhs.hour, rhs.minute,
                  rhs.second, rhs.millisecond);
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

std::ostream& operator<<(std::ostream& os, Time time) {
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  // Use StringPrintf because iostreams formatting is painful.
  return os << StringPrintf("%04d-%02d-%02d %02d:%02d:%02d.%03d UTC",
                            exploded.year,
                            exploded.month,
                            exploded.day_of_month,
                            exploded.hour,
                            exploded.minute,
                            exploded.second,
                            exploded.millisecond);
}

// TimeTicks ------------------------------------------------------------------

// static
TimeTicks TimeTicks::Now() {
  return internal::g_time_ticks_now_function.load(std::memory_order_relaxed)();
}

// static
TimeTicks TimeTicks::UnixEpoch() {
  static const TimeTicks epoch([]() {
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
  if (!interval_offset.is_zero() && tick_phase < *this)
    interval_offset += tick_interval;
  return *this + interval_offset;
}

std::ostream& operator<<(std::ostream& os, TimeTicks time_ticks) {
  // This function formats a TimeTicks object as "bogo-microseconds".
  // The origin and granularity of the count are platform-specific, and may very
  // from run to run. Although bogo-microseconds usually roughly correspond to
  // real microseconds, the only real guarantee is that the number never goes
  // down during a single run.
  const TimeDelta as_time_delta = time_ticks - TimeTicks();
  return os << as_time_delta.InMicroseconds() << " bogo-microseconds";
}

// ThreadTicks ----------------------------------------------------------------

// static
ThreadTicks ThreadTicks::Now() {
  return internal::g_thread_ticks_now_function.load(
      std::memory_order_relaxed)();
}

std::ostream& operator<<(std::ostream& os, ThreadTicks thread_ticks) {
  const TimeDelta as_time_delta = thread_ticks - ThreadTicks();
  return os << as_time_delta.InMicroseconds() << " bogo-thread-microseconds";
}

// Time::Exploded -------------------------------------------------------------

bool Time::Exploded::HasValidValues() const {
  // clang-format off
  return (1 <= month) && (month <= 12) &&
         (0 <= day_of_week) && (day_of_week <= 6) &&
         (1 <= day_of_month) && (day_of_month <= 31) &&
         (0 <= hour) && (hour <= 23) &&
         (0 <= minute) && (minute <= 59) &&
         (0 <= second) && (second <= 60) &&
         (0 <= millisecond) && (millisecond <= 999);
  // clang-format on
}

}  // namespace base
