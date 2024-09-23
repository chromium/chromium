// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <ostream>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/third_party/nspr/prtime.h"
#include "base/time/time_override.h"
#include "build/build_config.h"

namespace base {

namespace {

TimeTicks g_shared_time_ticks_at_unix_epoch;

}  // namespace

namespace internal {

std::atomic<TimeNowFunction> g_time_now_function{
    &subtle::TimeNowIgnoringOverride};

std::atomic<TimeNowFunction> g_time_now_from_system_time_function{
    &subtle::TimeNowFromSystemTimeIgnoringOverride};

std::atomic<TimeTicksNowFunction> g_time_ticks_now_function{
    &subtle::TimeTicksNowIgnoringOverride};

std::atomic<LiveTicksNowFunction> g_live_ticks_now_function{
    &subtle::LiveTicksNowIgnoringOverride};

std::atomic<ThreadTicksNowFunction> g_thread_ticks_now_function{
    &subtle::ThreadTicksNowIgnoringOverride};

}  // namespace internal

// TimeDelta ------------------------------------------------------------------

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
  [[maybe_unused]] const bool result =
      FromExploded(is_local, exploded, &out_time);
#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(ARCH_CPU_ARM_FAMILY)
  // TODO(crbug.com/40800460): DCHECKs have limited coverage during automated
  // testing on CrOS and this check failed when tested on an experimental
  // builder. Testing for ARCH_CPU_ARM_FAMILY prevents regressing coverage on
  // x86_64, which is already enabled. See go/chrome-dcheck-on-cros or
  // http://crbug.com/1113456 for more details.
#else
  DCHECK(result);  // This function must not fail.
#endif
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

  *parsed_time = UnixEpoch() + Microseconds(result_time);
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
  // Can't call `UnlocalizedTimeFormatWithPattern()`/`TimeFormatAsIso8601()`
  // since `//base` can't depend on `//base:i18n`.
  //
  // TODO(pkasting): Consider whether `operator<<()` should move to
  // `base/i18n/time_formatting.h` -- would let us implement in terms of
  // existing time formatting, but might be confusing.
  return os << StringPrintf("%04d-%02d-%02d %02d:%02d:%02d.%06" PRId64 " UTC",
                            exploded.year, exploded.month,
                            exploded.day_of_month, exploded.hour,
                            exploded.minute, exploded.second,
                            time.ToDeltaSinceWindowsEpoch().InMicroseconds() %
                                Time::kMicrosecondsPerSecond);
}

// TimeTicks ------------------------------------------------------------------

// static
TimeTicks TimeTicks::Now() {
  return internal::g_time_ticks_now_function.load(std::memory_order_relaxed)();
}

// static
// This method should be called once at process start and before
// TimeTicks::UnixEpoch is accessed. It is intended to make the offset between
// unix time and monotonic time consistent across processes.
void TimeTicks::SetSharedUnixEpoch(TimeTicks ticks_at_epoch) {
  DCHECK(g_shared_time_ticks_at_unix_epoch.is_null());
  g_shared_time_ticks_at_unix_epoch = ticks_at_epoch;
}

// static
TimeTicks TimeTicks::UnixEpoch() {
  struct StaticUnixEpoch {
    StaticUnixEpoch()
        : epoch(
              g_shared_time_ticks_at_unix_epoch.is_null()
                  ? subtle::TimeTicksNowIgnoringOverride() -
                        (subtle::TimeNowIgnoringOverride() - Time::UnixEpoch())
                  : g_shared_time_ticks_at_unix_epoch) {
      // Prevent future usage of `g_shared_time_ticks_at_unix_epoch`.
      g_shared_time_ticks_at_unix_epoch = TimeTicks::Max();
    }

    const TimeTicks epoch;
  };

  static StaticUnixEpoch static_epoch;
  return static_epoch.epoch;
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

// LiveTicks ------------------------------------------------------------------

// static
LiveTicks LiveTicks::Now() {
  return internal::g_live_ticks_now_function.load(std::memory_order_relaxed)();
}

#if !BUILDFLAG(IS_WIN)
namespace subtle {
LiveTicks LiveTicksNowIgnoringOverride() {
  // On non-windows platforms LiveTicks is equivalent to TimeTicks already.
  // Subtract the empty `TimeTicks` from `TimeTicks::Now()` to get a `TimeDelta`
  // that can be added to the empty `LiveTicks`.
  return LiveTicks() + (TimeTicks::Now() - TimeTicks());
}
}  // namespace subtle

#endif

std::ostream& operator<<(std::ostream& os, LiveTicks live_ticks) {
  const TimeDelta as_time_delta = live_ticks - LiveTicks();
  return os << as_time_delta.InMicroseconds() << " bogo-live-microseconds";
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
