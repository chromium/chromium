// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID) && !defined(__LP64__)
#include <time64.h>
#endif

// Ensure the Fuchsia and Mac builds do not include this module. Instead,
// non-POSIX implementation is used for sampling the system clocks.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
#error "This implementation is for POSIX platforms other than Fuchsia or Mac."
#endif

namespace {

int64_t ConvertTimespecToMicros(const struct timespec& ts) {
  // On 32-bit systems, the calculation cannot overflow int64_t.
  // 2**32 * 1000000 + 2**64 / 1000 < 2**63
  if (sizeof(ts.tv_sec) <= 4 && sizeof(ts.tv_nsec) <= 8) {
    int64_t result = ts.tv_sec;
    result *= base::Time::kMicrosecondsPerSecond;
    result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
    return result;
  }
  base::CheckedNumeric<int64_t> result(ts.tv_sec);
  result *= base::Time::kMicrosecondsPerSecond;
  result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
  return result.ValueOrDie();
}

// Helper function to get results from clock_gettime() and convert to a
// microsecond timebase. Minimum requirement is MONOTONIC_CLOCK to be supported
// on the system. FreeBSD 6 has CLOCK_MONOTONIC but defines
// _POSIX_MONOTONIC_CLOCK to -1.
#if (BUILDFLAG(IS_POSIX) && defined(_POSIX_MONOTONIC_CLOCK) && \
     _POSIX_MONOTONIC_CLOCK >= 0) ||                           \
    BUILDFLAG(IS_BSD) || BUILDFLAG(IS_ANDROID)
int64_t ClockNow(clockid_t clk_id) {
  struct timespec ts;
  CHECK(clock_gettime(clk_id, &ts) == 0);
  return ConvertTimespecToMicros(ts);
}

std::optional<int64_t> MaybeClockNow(clockid_t clk_id) {
  struct timespec ts;
  int res = clock_gettime(clk_id, &ts);
  if (res == 0)
    return ConvertTimespecToMicros(ts);
  return std::nullopt;
}

#else  // _POSIX_MONOTONIC_CLOCK
#error No usable tick clock function on this platform.
#endif  // _POSIX_MONOTONIC_CLOCK

}  // namespace

namespace base {

// Time -----------------------------------------------------------------------

namespace subtle {
Time TimeNowIgnoringOverride() {
  struct timeval tv;
  struct timezone tz = {0, 0};  // UTC
  CHECK(gettimeofday(&tv, &tz) == 0);
  // Combine seconds and microseconds in a 64-bit field containing microseconds
  // since the epoch.  That's enough for nearly 600 centuries.  Adjust from
  // Unix (1970) to Windows (1601) epoch.
  return Time() +
         Microseconds((tv.tv_sec * Time::kMicrosecondsPerSecond + tv.tv_usec) +
                      Time::kTimeTToMicrosecondsOffset);
}

Time TimeNowFromSystemTimeIgnoringOverride() {
  // Just use TimeNowIgnoringOverride() because it returns the system time.
  return TimeNowIgnoringOverride();
}
}  // namespace subtle

// TimeTicks ------------------------------------------------------------------

namespace subtle {
TimeTicks TimeTicksNowIgnoringOverride() {
  return TimeTicks() + Microseconds(ClockNow(CLOCK_MONOTONIC));
}

std::optional<TimeTicks> MaybeTimeTicksNowIgnoringOverride() {
  std::optional<int64_t> now = MaybeClockNow(CLOCK_MONOTONIC);
  if (now.has_value())
    return TimeTicks() + Microseconds(now.value());
  return std::nullopt;
}
}  // namespace subtle

// static
TimeTicks::Clock TimeTicks::GetClock() {
  return Clock::LINUX_CLOCK_MONOTONIC;
}

// static
bool TimeTicks::IsHighResolution() {
  return true;
}

// static
bool TimeTicks::IsConsistentAcrossProcesses() {
  return true;
}

// ThreadTicks ----------------------------------------------------------------

namespace subtle {
ThreadTicks ThreadTicksNowIgnoringOverride() {
#if (defined(_POSIX_THREAD_CPUTIME) && (_POSIX_THREAD_CPUTIME >= 0)) || \
    BUILDFLAG(IS_ANDROID)
  return ThreadTicks() + Microseconds(ClockNow(CLOCK_THREAD_CPUTIME_ID));
#else
  NOTREACHED();
#endif
}
}  // namespace subtle

}  // namespace base
