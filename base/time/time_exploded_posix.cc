// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#if defined(OS_ANDROID) && !defined(__LP64__)
#include <time64.h>
#endif
#include <unistd.h>

#include <limits>

#include "base/numerics/safe_math.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#elif defined(OS_NACL)
#include "base/os_compat_nacl.h"
#endif

#if defined(OS_FUCHSIA)
#include <fuchsia/deprecatedtimezone/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/no_destructor.h"
#include "base/numerics/clamped_math.h"
#endif

#if defined(OS_MACOSX) || defined(OS_IOS)
static_assert(sizeof(time_t) >= 8, "Y2038 problem!");
#endif

namespace {

#if !defined(OS_FUCHSIA)
// This prevents a crash on traversing the environment global and looking up
// the 'TZ' variable in libc. See: crbug.com/390567.
base::Lock* GetSysTimeToTimeStructLock() {
  static auto* lock = new base::Lock();
  return lock;
}
#endif  // !defined(OS_FUCHSIA)

// Define a system-specific SysTime that wraps either to a time_t or
// a time64_t depending on the host system, and associated convertion.
// See crbug.com/162007
#if defined(OS_ANDROID) && !defined(__LP64__)
typedef time64_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  base::AutoLock locked(*GetSysTimeToTimeStructLock());
  if (is_local)
    return mktime64(timestruct);
  else
    return timegm64(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  base::AutoLock locked(*GetSysTimeToTimeStructLock());
  if (is_local)
    localtime64_r(&t, timestruct);
  else
    gmtime64_r(&t, timestruct);
}

#elif defined(OS_FUCHSIA)
typedef time_t SysTime;

fuchsia::deprecatedtimezone::TimezoneSyncPtr ConnectTimeZoneServiceSync() {
  fuchsia::deprecatedtimezone::TimezoneSyncPtr timezone;
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      timezone.NewRequest());
  return timezone;
}

SysTime GetTimezoneOffset(SysTime utc_time) {
  static base::NoDestructor<fuchsia::deprecatedtimezone::TimezoneSyncPtr>
      timezone(ConnectTimeZoneServiceSync());

  int64_t milliseconds_since_epoch =
      base::ClampMul(utc_time, base::Time::kMillisecondsPerSecond);
  int32_t local_offset_minutes = 0;
  int32_t dst_offset_minutes = 0;
  zx_status_t status = (*timezone.get())
                           ->GetTimezoneOffsetMinutes(milliseconds_since_epoch,
                                                      &local_offset_minutes,
                                                      &dst_offset_minutes);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "Failed to get current timezone offset.";
    return 0;
  }
  return (local_offset_minutes + dst_offset_minutes) *
         base::Time::kSecondsPerMinute;
}

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  SysTime result = timegm(timestruct);
  if (is_local) {
    // Local->UTC conversion may be ambiguous, particularly when local clock is
    // changed back (e.g. in when DST ends). In such cases there are 2 correct
    // results and this function will return one of them. Also some local time
    // values may be invalid. Specifically when local time is rolled forward
    // (when DST starts) the values in the transitional period are invalid and
    // don't have corresponding values in the UTC timeline. In those cases using
    // timezone offset either before or after transition is acceptable.
    //
    // fuchsia::deprecatedtimezone API returns offset based on UTC time. It may
    // return incorrect result when called with a value that also includes
    // timezone offset. Particularly this is a problem when the time is close to
    // DST transitions. For example, when transitioning from PST (UTC-8,
    // non-DST) to PDT (UTC-7, DST) GetTimezoneOffset(local_time) will return a
    // value that's off by 1 hour for 8 hours after the transition. To avoid
    // this problem the offset is estimated as GetTimezoneOffset(local_time)
    // from which |approx_utc_time| is calculated. Then
    // GetTimezoneOffset(approx_utc_time) is used to calculate the actual
    // offset. This works correctly assuming timezone transition can happen at
    // most once per day. When both before and after offsets are in the [-1H,
    // 1H] range then the |approx_utc_time| is correct (see the note above for
    // definition of what is considered correct). Otherwise |approx_utc_time|
    // may be off by 1 hour. In those cases GetTimezoneOffset(approx_utc_time)
    // will return correct offset because we can assume there are no timezone
    // changes in the [UTC-1H, UTC+1H] period (the transition is scheduled
    // either before UTC-1H or after UTC+1H).
    int64_t approx_utc_time = result - GetTimezoneOffset(result);
    result -= GetTimezoneOffset(approx_utc_time);
  }
  return result;
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  if (is_local)
    t += GetTimezoneOffset(t);
  gmtime_r(&t, timestruct);
}
#elif defined(OS_AIX)

// The function timegm is not available on AIX.
time_t aix_timegm(struct tm* tm) {
  time_t ret;
  char* tz;

  tz = getenv("TZ");
  if (tz) {
    tz = strdup(tz);
  }
  setenv("TZ", "GMT0", 1);
  tzset();
  ret = mktime(tm);
  if (tz) {
    setenv("TZ", tz, 1);
    free(tz);
  } else {
    unsetenv("TZ");
  }
  tzset();
  return ret;
}

typedef time_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  base::AutoLock locked(*GetSysTimeToTimeStructLock());
  if (is_local)
    return mktime(timestruct);
  else
    return aix_timegm(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  base::AutoLock locked(*GetSysTimeToTimeStructLock());
  if (is_local)
    localtime_r(&t, timestruct);
  else
    gmtime_r(&t, timestruct);
}

#else   // OS_ANDROID && !__LP64__
typedef time_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  base::AutoLock locked(*GetSysTimeToTimeStructLock());
  return is_local ? mktime(timestruct) : timegm(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  base::AutoLock locked(*GetSysTimeToTimeStructLock());
  if (is_local)
    localtime_r(&t, timestruct);
  else
    gmtime_r(&t, timestruct);
}
#endif  // OS_ANDROID

}  // namespace

namespace base {

void Time::Explode(bool is_local, Exploded* exploded) const {
  // Time stores times with microsecond resolution, but Exploded only carries
  // millisecond resolution, so begin by being lossy.  Adjust from Windows
  // epoch (1601) to Unix epoch (1970);
  int64_t microseconds = us_ - kTimeTToMicrosecondsOffset;
  // The following values are all rounded towards -infinity.
  int64_t milliseconds;  // Milliseconds since epoch.
  SysTime seconds;       // Seconds since epoch.
  int millisecond;       // Exploded millisecond value (0-999).
  if (microseconds >= 0) {
    // Rounding towards -infinity <=> rounding towards 0, in this case.
    milliseconds = microseconds / kMicrosecondsPerMillisecond;
    seconds = milliseconds / kMillisecondsPerSecond;
    millisecond = milliseconds % kMillisecondsPerSecond;
  } else {
    // Round these *down* (towards -infinity).
    milliseconds = (microseconds - kMicrosecondsPerMillisecond + 1) /
                   kMicrosecondsPerMillisecond;
    seconds =
        (milliseconds - kMillisecondsPerSecond + 1) / kMillisecondsPerSecond;
    // Make this nonnegative (and between 0 and 999 inclusive).
    millisecond = milliseconds % kMillisecondsPerSecond;
    if (millisecond < 0)
      millisecond += kMillisecondsPerSecond;
  }

  struct tm timestruct;
  SysTimeToTimeStruct(seconds, &timestruct, is_local);

  exploded->year = timestruct.tm_year + 1900;
  exploded->month = timestruct.tm_mon + 1;
  exploded->day_of_week = timestruct.tm_wday;
  exploded->day_of_month = timestruct.tm_mday;
  exploded->hour = timestruct.tm_hour;
  exploded->minute = timestruct.tm_min;
  exploded->second = timestruct.tm_sec;
  exploded->millisecond = millisecond;
}

// static
bool Time::FromExploded(bool is_local, const Exploded& exploded, Time* time) {
  CheckedNumeric<int> month = exploded.month;
  month--;
  CheckedNumeric<int> year = exploded.year;
  year -= 1900;
  if (!month.IsValid() || !year.IsValid()) {
    *time = Time(0);
    return false;
  }

  struct tm timestruct;
  timestruct.tm_sec = exploded.second;
  timestruct.tm_min = exploded.minute;
  timestruct.tm_hour = exploded.hour;
  timestruct.tm_mday = exploded.day_of_month;
  timestruct.tm_mon = month.ValueOrDie();
  timestruct.tm_year = year.ValueOrDie();
  timestruct.tm_wday = exploded.day_of_week;  // mktime/timegm ignore this
  timestruct.tm_yday = 0;                     // mktime/timegm ignore this
  timestruct.tm_isdst = -1;                   // attempt to figure it out
#if !defined(OS_NACL) && !defined(OS_SOLARIS) && !defined(OS_AIX)
  timestruct.tm_gmtoff = 0;   // not a POSIX field, so mktime/timegm ignore
  timestruct.tm_zone = nullptr;  // not a POSIX field, so mktime/timegm ignore
#endif

  SysTime seconds;

  // Certain exploded dates do not really exist due to daylight saving times,
  // and this causes mktime() to return implementation-defined values when
  // tm_isdst is set to -1. On Android, the function will return -1, while the
  // C libraries of other platforms typically return a liberally-chosen value.
  // Handling this requires the special code below.

  // SysTimeFromTimeStruct() modifies the input structure, save current value.
  struct tm timestruct0 = timestruct;

  seconds = SysTimeFromTimeStruct(&timestruct, is_local);
  if (seconds == -1) {
    // Get the time values with tm_isdst == 0 and 1, then select the closest one
    // to UTC 00:00:00 that isn't -1.
    timestruct = timestruct0;
    timestruct.tm_isdst = 0;
    int64_t seconds_isdst0 = SysTimeFromTimeStruct(&timestruct, is_local);

    timestruct = timestruct0;
    timestruct.tm_isdst = 1;
    int64_t seconds_isdst1 = SysTimeFromTimeStruct(&timestruct, is_local);

    // seconds_isdst0 or seconds_isdst1 can be -1 for some timezones.
    // E.g. "CLST" (Chile Summer Time) returns -1 for 'tm_isdt == 1'.
    if (seconds_isdst0 < 0)
      seconds = seconds_isdst1;
    else if (seconds_isdst1 < 0)
      seconds = seconds_isdst0;
    else
      seconds = std::min(seconds_isdst0, seconds_isdst1);
  }

  // Handle overflow.  Clamping the range to what mktime and timegm might
  // return is the best that can be done here.  It's not ideal, but it's better
  // than failing here or ignoring the overflow case and treating each time
  // overflow as one second prior to the epoch.
  int64_t milliseconds = 0;
  if (seconds == -1 && (exploded.year < 1969 || exploded.year > 1970)) {
    // If exploded.year is 1969 or 1970, take -1 as correct, with the
    // time indicating 1 second prior to the epoch.  (1970 is allowed to handle
    // time zone and DST offsets.)  Otherwise, return the most future or past
    // time representable.  Assumes the time_t epoch is 1970-01-01 00:00:00 UTC.
    //
    // The minimum and maximum representible times that mktime and timegm could
    // return are used here instead of values outside that range to allow for
    // proper round-tripping between exploded and counter-type time
    // representations in the presence of possible truncation to time_t by
    // division and use with other functions that accept time_t.
    //
    // When representing the most distant time in the future, add in an extra
    // 999ms to avoid the time being less than any other possible value that
    // this function can return.

    // On Android, SysTime is int64_t, special care must be taken to avoid
    // overflows.
    const int64_t min_seconds = (sizeof(SysTime) < sizeof(int64_t))
                                    ? std::numeric_limits<SysTime>::min()
                                    : std::numeric_limits<int32_t>::min();
    const int64_t max_seconds = (sizeof(SysTime) < sizeof(int64_t))
                                    ? std::numeric_limits<SysTime>::max()
                                    : std::numeric_limits<int32_t>::max();
    if (exploded.year < 1969) {
      milliseconds = min_seconds * kMillisecondsPerSecond;
    } else {
      milliseconds = max_seconds * kMillisecondsPerSecond;
      milliseconds += (kMillisecondsPerSecond - 1);
    }
  } else {
    base::CheckedNumeric<int64_t> checked_millis = seconds;
    checked_millis *= kMillisecondsPerSecond;
    checked_millis += exploded.millisecond;
    if (!checked_millis.IsValid()) {
      *time = base::Time(0);
      return false;
    }
    milliseconds = checked_millis.ValueOrDie();
  }

  // Adjust from Unix (1970) to Windows (1601) epoch avoiding overflows.
  base::CheckedNumeric<int64_t> checked_microseconds_win_epoch = milliseconds;
  checked_microseconds_win_epoch *= kMicrosecondsPerMillisecond;
  checked_microseconds_win_epoch += kTimeTToMicrosecondsOffset;
  if (!checked_microseconds_win_epoch.IsValid()) {
    *time = base::Time(0);
    return false;
  }
  base::Time converted_time(checked_microseconds_win_epoch.ValueOrDie());

  // If |exploded.day_of_month| is set to 31 on a 28-30 day month, it will
  // return the first day of the next month. Thus round-trip the time and
  // compare the initial |exploded| with |utc_to_exploded| time.
  base::Time::Exploded to_exploded;
  if (!is_local)
    converted_time.UTCExplode(&to_exploded);
  else
    converted_time.LocalExplode(&to_exploded);

  if (ExplodedMostlyEquals(to_exploded, exploded)) {
    *time = converted_time;
    return true;
  }

  *time = Time(0);
  return false;
}

}  // namespace base
