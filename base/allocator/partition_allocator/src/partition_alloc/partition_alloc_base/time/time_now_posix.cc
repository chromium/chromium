// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/time.h>

#include <cstdint>
#include <ctime>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#if PA_BUILDFLAG(IS_ANDROID) && !defined(__LP64__)
#include <time64.h>
#endif
#include <unistd.h>

#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_base/numerics/safe_math.h"
#include "partition_alloc/partition_alloc_base/time/time_override.h"

// Ensure the Fuchsia and Mac builds do not include this module. Instead,
// non-POSIX implementation is used for sampling the system clocks.
#if PA_BUILDFLAG(IS_FUCHSIA) || PA_BUILDFLAG(IS_APPLE)
#error "This implementation is for POSIX platforms other than Fuchsia or Mac."
#endif

namespace partition_alloc::internal::base {

namespace {

int64_t ConvertTimespecToMicros(const struct timespec& ts) {
  // On 32-bit systems, the calculation cannot overflow int64_t.
  // 2**32 * 1000000 + 2**64 / 1000 < 2**63
  if (sizeof(ts.tv_sec) <= 4 && sizeof(ts.tv_nsec) <= 8) {
    int64_t result = ts.tv_sec;
    result *= Time::kMicrosecondsPerSecond;
    result += (ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
    return result;
  }
  CheckedNumeric<int64_t> result(ts.tv_sec);
  result *= Time::kMicrosecondsPerSecond;
  result += (ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
  return result.ValueOrDie();
}

// Helper function to get results from clock_gettime() and convert to a
// microsecond timebase. Minimum requirement is MONOTONIC_CLOCK to be supported
// on the system. FreeBSD 6 has CLOCK_MONOTONIC but defines
// _POSIX_MONOTONIC_CLOCK to -1.
#if (PA_BUILDFLAG(IS_POSIX) && defined(_POSIX_MONOTONIC_CLOCK) && \
     _POSIX_MONOTONIC_CLOCK >= 0) ||                              \
    PA_BUILDFLAG(IS_BSD) || PA_BUILDFLAG(IS_ANDROID)
int64_t ClockNow(clockid_t clk_id) {
  struct timespec ts;
  PA_BASE_CHECK(clock_gettime(clk_id, &ts) == 0);
  return ConvertTimespecToMicros(ts);
}
#else  // _POSIX_MONOTONIC_CLOCK
#error No usable tick clock function on this platform.
#endif  // _POSIX_MONOTONIC_CLOCK

}  // namespace

// Time -----------------------------------------------------------------------

namespace subtle {
Time TimeNowIgnoringOverride() {
  struct timeval tv;
  struct timezone tz = {0, 0};  // UTC
  PA_BASE_CHECK(gettimeofday(&tv, &tz) == 0);
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
    PA_BUILDFLAG(IS_ANDROID)
  return ThreadTicks() + Microseconds(ClockNow(CLOCK_THREAD_CPUTIME_ID));
#else
  PA_NOTREACHED();
#endif
}
}  // namespace subtle

}  // namespace partition_alloc::internal::base
