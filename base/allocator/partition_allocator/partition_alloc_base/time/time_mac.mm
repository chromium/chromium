// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/time/time.h"

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "base/allocator/partition_allocator/partition_alloc_base/logging.h"
#include "base/allocator/partition_allocator/partition_alloc_base/numerics/safe_conversions.h"
#include "base/allocator/partition_allocator/partition_alloc_base/time/time_override.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "build/build_config.h"

namespace partition_alloc::internal::base {

namespace {

#if BUILDFLAG(IS_MAC)
// Returns a pointer to the initialized Mach timebase info struct.
mach_timebase_info_data_t* MachTimebaseInfo() {
  static mach_timebase_info_data_t timebase_info = []() {
    mach_timebase_info_data_t info;
    kern_return_t kr = mach_timebase_info(&info);
    PA_DCHECK(kr == KERN_SUCCESS) << "mach_timebase_info";
    PA_DCHECK(info.numer);
    PA_DCHECK(info.denom);
    return info;
  }();
  return &timebase_info;
}

int64_t MachTimeToMicroseconds(uint64_t mach_time) {
  // timebase_info gives us the conversion factor between absolute time tick
  // units and nanoseconds.
  mach_timebase_info_data_t* timebase_info = MachTimebaseInfo();

  // Take the fast path when the conversion is 1:1. The result will for sure fit
  // into an int_64 because we're going from nanoseconds to microseconds.
  if (timebase_info->numer == timebase_info->denom) {
    return static_cast<int64_t>(mach_time / Time::kNanosecondsPerMicrosecond);
  }

  uint64_t microseconds = 0;
  const uint64_t divisor =
      timebase_info->denom * Time::kNanosecondsPerMicrosecond;

  // Microseconds is mach_time * timebase.numer /
  // (timebase.denom * kNanosecondsPerMicrosecond). Divide first to reduce
  // the chance of overflow. Also stash the remainder right now, a likely
  // byproduct of the division.
  microseconds = mach_time / divisor;
  const uint64_t mach_time_remainder = mach_time % divisor;

  // Now multiply, keeping an eye out for overflow.
  PA_CHECK(!__builtin_umulll_overflow(microseconds, timebase_info->numer,
                                      &microseconds));

  // By dividing first we lose precision. Regain it by adding back the
  // microseconds from the remainder, with an eye out for overflow.
  uint64_t least_significant_microseconds =
      (mach_time_remainder * timebase_info->numer) / divisor;
  PA_CHECK(!__builtin_uaddll_overflow(
      microseconds, least_significant_microseconds, &microseconds));

  // Don't bother with the rollover handling that the Windows version does.
  // The returned time in microseconds is enough for 292,277 years (starting
  // from 2^63 because the returned int64_t is signed,
  // 9223372036854775807 / (1e6 * 60 * 60 * 24 * 365.2425) = 292,277).
  return checked_cast<int64_t>(microseconds);
}
#endif  // BUILDFLAG(IS_MAC)

// Returns monotonically growing number of ticks in microseconds since some
// unspecified starting point.
int64_t ComputeCurrentTicks() {
#if BUILDFLAG(IS_IOS)
  // iOS 10 supports clock_gettime(CLOCK_MONOTONIC, ...), which is
  // around 15 times faster than sysctl() call. Use it if possible;
  // otherwise, fall back to sysctl().
  if (__builtin_available(iOS 10, *)) {
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0) {
      return (int64_t)tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
    }
  }

  // On iOS mach_absolute_time stops while the device is sleeping. Instead use
  // now - KERN_BOOTTIME to get a time difference that is not impacted by clock
  // changes. KERN_BOOTTIME will be updated by the system whenever the system
  // clock change.
  struct timeval boottime;
  int mib[2] = {CTL_KERN, KERN_BOOTTIME};
  size_t size = sizeof(boottime);
  int kr = sysctl(mib, std::size(mib), &boottime, &size, nullptr, 0);
  PA_DCHECK(KERN_SUCCESS == kr);
  TimeDelta time_difference =
      subtle::TimeNowIgnoringOverride() -
      (Time::FromTimeT(boottime.tv_sec) + Microseconds(boottime.tv_usec));
  return time_difference.InMicroseconds();
#else
  // mach_absolute_time is it when it comes to ticks on the Mac.  Other calls
  // with less precision (such as TickCount) just call through to
  // mach_absolute_time.
  return MachTimeToMicroseconds(mach_absolute_time());
#endif  // BUILDFLAG(IS_IOS)
}

int64_t ComputeThreadTicks() {
#if BUILDFLAG(IS_IOS)
  PA_NOTREACHED();
  return 0;
#else
  // The pthreads library keeps a cached reference to the thread port, which
  // does not have to be released like mach_thread_self() does.
  mach_port_t thread_port = pthread_mach_thread_np(pthread_self());
  if (thread_port == MACH_PORT_NULL) {
    PA_DLOG(ERROR) << "Failed to get pthread_mach_thread_np()";
    return 0;
  }

  mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
  thread_basic_info_data_t thread_info_data;

  kern_return_t kr = thread_info(
      thread_port, THREAD_BASIC_INFO,
      reinterpret_cast<thread_info_t>(&thread_info_data), &thread_info_count);
  PA_DCHECK(kr == KERN_SUCCESS) << "thread_info";

  CheckedNumeric<int64_t> absolute_micros(thread_info_data.user_time.seconds +
                                          thread_info_data.system_time.seconds);
  absolute_micros *= Time::kMicrosecondsPerSecond;
  absolute_micros += (thread_info_data.user_time.microseconds +
                      thread_info_data.system_time.microseconds);
  return absolute_micros.ValueOrDie();
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace

// The Time routines in this file use Mach and CoreFoundation APIs, since the
// POSIX definition of time_t in Mac OS X wraps around after 2038--and
// there are already cookie expiration dates, etc., past that time out in
// the field.  Using CFDate prevents that problem, and using mach_absolute_time
// for TimeTicks gives us nice high-resolution interval timing.

// Time -----------------------------------------------------------------------

namespace subtle {
Time TimeNowIgnoringOverride() {
  return Time::FromCFAbsoluteTime(CFAbsoluteTimeGetCurrent());
}

Time TimeNowFromSystemTimeIgnoringOverride() {
  // Just use TimeNowIgnoringOverride() because it returns the system time.
  return TimeNowIgnoringOverride();
}
}  // namespace subtle

// static
Time Time::FromCFAbsoluteTime(CFAbsoluteTime t) {
  static_assert(std::numeric_limits<CFAbsoluteTime>::has_infinity,
                "CFAbsoluteTime must have an infinity value");
  if (t == 0)
    return Time();  // Consider 0 as a null Time.
  return (t == std::numeric_limits<CFAbsoluteTime>::infinity())
             ? Max()
             : (UnixEpoch() +
                Seconds(double{t + kCFAbsoluteTimeIntervalSince1970}));
}

CFAbsoluteTime Time::ToCFAbsoluteTime() const {
  static_assert(std::numeric_limits<CFAbsoluteTime>::has_infinity,
                "CFAbsoluteTime must have an infinity value");
  if (is_null())
    return 0;  // Consider 0 as a null Time.
  return is_max() ? std::numeric_limits<CFAbsoluteTime>::infinity()
                  : (CFAbsoluteTime{(*this - UnixEpoch()).InSecondsF()} -
                     kCFAbsoluteTimeIntervalSince1970);
}

// static
Time Time::FromNSDate(NSDate* date) {
  PA_DCHECK(date);
  return FromCFAbsoluteTime(date.timeIntervalSinceReferenceDate);
}

NSDate* Time::ToNSDate() const {
  return [NSDate dateWithTimeIntervalSinceReferenceDate:ToCFAbsoluteTime()];
}

// TimeDelta ------------------------------------------------------------------

#if BUILDFLAG(IS_MAC)
// static
TimeDelta TimeDelta::FromMachTime(uint64_t mach_time) {
  return Microseconds(MachTimeToMicroseconds(mach_time));
}
#endif  // BUILDFLAG(IS_MAC)

// TimeTicks ------------------------------------------------------------------

namespace subtle {
TimeTicks TimeTicksNowIgnoringOverride() {
  return TimeTicks() + Microseconds(ComputeCurrentTicks());
}
}  // namespace subtle

// static
bool TimeTicks::IsHighResolution() {
  return true;
}

// static
bool TimeTicks::IsConsistentAcrossProcesses() {
  return true;
}

#if BUILDFLAG(IS_MAC)
// static
TimeTicks TimeTicks::FromMachAbsoluteTime(uint64_t mach_absolute_time) {
  return TimeTicks(MachTimeToMicroseconds(mach_absolute_time));
}

// static
mach_timebase_info_data_t TimeTicks::SetMachTimebaseInfoForTesting(
    mach_timebase_info_data_t timebase) {
  mach_timebase_info_data_t orig_timebase = *MachTimebaseInfo();

  *MachTimebaseInfo() = timebase;

  return orig_timebase;
}

#endif  // BUILDFLAG(IS_MAC)

// static
TimeTicks::Clock TimeTicks::GetClock() {
#if BUILDFLAG(IS_IOS)
  return Clock::IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME;
#else
  return Clock::MAC_MACH_ABSOLUTE_TIME;
#endif  // BUILDFLAG(IS_IOS)
}

// ThreadTicks ----------------------------------------------------------------

namespace subtle {
ThreadTicks ThreadTicksNowIgnoringOverride() {
  return ThreadTicks() + Microseconds(ComputeThreadTicks());
}
}  // namespace subtle

}  // namespace partition_alloc::internal::base
