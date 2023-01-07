// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {

namespace {

// Returns a new icu::Calendar instance for the local time zone if |is_local|
// and for GMT otherwise. Returns null on error.
std::unique_ptr<icu::Calendar> CreateCalendar(bool is_local) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar;
  // Always use GregorianCalendar and US locale (relevant for day_of_week,
  // Sunday is the first day) - that's what base::Time::Exploded assumes.
  if (is_local) {
    calendar =
        std::make_unique<icu::GregorianCalendar>(icu::Locale::getUS(), status);
  } else {
    calendar = std::make_unique<icu::GregorianCalendar>(
        *icu::TimeZone::getGMT(), icu::Locale::getUS(), status);
  }
  CHECK(U_SUCCESS(status));
  return calendar;
}

// Explodes the |millis_since_unix_epoch| using an icu::Calendar, and returns
// true if the conversion was successful.
bool ExplodeUsingIcuCalendar(int64_t millis_since_unix_epoch,
                             bool is_local,
                             Time::Exploded* exploded) {
  // ICU's year calculation is wrong for years too far in the past (though
  // other fields seem to be correct). Given that the Time::Explode() for
  // Windows only works for values on/after 1601-01-01 00:00:00 UTC, just use
  // that as a reasonable lower-bound here as well.
  constexpr int64_t kInputLowerBound =
      -Time::kTimeTToMicrosecondsOffset / Time::kMicrosecondsPerMillisecond;
  static_assert(
      Time::kTimeTToMicrosecondsOffset % Time::kMicrosecondsPerMillisecond == 0,
      "assumption: no epoch offset sub-milliseconds");

  // The input to icu::Calendar is a double-typed value. To ensure no loss of
  // precision when converting int64_t to double, an upper-bound must also be
  // imposed.
  static_assert(std::numeric_limits<double>::radix == 2, "");
  constexpr int64_t kInputUpperBound = uint64_t{1}
                                       << std::numeric_limits<double>::digits;

  if (millis_since_unix_epoch < kInputLowerBound ||
      millis_since_unix_epoch > kInputUpperBound) {
    return false;
  }

  std::unique_ptr<icu::Calendar> calendar = CreateCalendar(is_local);
  UErrorCode status = U_ZERO_ERROR;
  calendar->setTime(millis_since_unix_epoch, status);
  if (!U_SUCCESS(status))
    return false;

  using CalendarField = decltype(calendar->get(UCAL_YEAR, status));
  static_assert(sizeof(Time::Exploded::year) >= sizeof(CalendarField),
                "Time::Exploded members are not large enough to hold ICU "
                "calendar fields.");

  bool got_all_fields = true;
  exploded->year = calendar->get(UCAL_YEAR, status);
  got_all_fields &= !!U_SUCCESS(status);
  // ICU's UCalendarMonths is 0-based. E.g., 0 for January.
  exploded->month = calendar->get(UCAL_MONTH, status) + 1;
  got_all_fields &= !!U_SUCCESS(status);
  // ICU's UCalendarDaysOfWeek is 1-based. E.g., 1 for Sunday.
  exploded->day_of_week = calendar->get(UCAL_DAY_OF_WEEK, status) - 1;
  got_all_fields &= !!U_SUCCESS(status);
  exploded->day_of_month = calendar->get(UCAL_DAY_OF_MONTH, status);
  got_all_fields &= !!U_SUCCESS(status);
  exploded->hour = calendar->get(UCAL_HOUR_OF_DAY, status);
  got_all_fields &= !!U_SUCCESS(status);
  exploded->minute = calendar->get(UCAL_MINUTE, status);
  got_all_fields &= !!U_SUCCESS(status);
  exploded->second = calendar->get(UCAL_SECOND, status);
  got_all_fields &= !!U_SUCCESS(status);
  exploded->millisecond = calendar->get(UCAL_MILLISECOND, status);
  got_all_fields &= !!U_SUCCESS(status);
  return got_all_fields;
}

}  // namespace

// static
void Time::ExplodeUsingIcu(int64_t millis_since_unix_epoch,
                           bool is_local,
                           Exploded* exploded) {
  if (!ExplodeUsingIcuCalendar(millis_since_unix_epoch, is_local, exploded)) {
    // Error: Return an invalid Exploded.
    *exploded = {};
  }
}

// static
bool Time::FromExplodedUsingIcu(bool is_local,
                                const Exploded& exploded,
                                int64_t* millis_since_unix_epoch) {
  // ICU's UCalendarMonths is 0-based. E.g., 0 for January.
  CheckedNumeric<int> month = exploded.month;
  month--;
  if (!month.IsValid())
    return false;

  std::unique_ptr<icu::Calendar> calendar = CreateCalendar(is_local);

  // Cause getTime() to report an error if invalid dates, such as the 31st day
  // of February, are specified.
  calendar->setLenient(false);

  calendar->set(exploded.year, month.ValueOrDie(), exploded.day_of_month,
                exploded.hour, exploded.minute, exploded.second);
  calendar->set(UCAL_MILLISECOND, exploded.millisecond);
  // Ignore exploded.day_of_week

  UErrorCode status = U_ZERO_ERROR;
  UDate date = calendar->getTime(status);
  if (U_FAILURE(status))
    return false;

  *millis_since_unix_epoch = saturated_cast<int64_t>(date);
  return true;
}

#if BUILDFLAG(IS_FUCHSIA)

void Time::Explode(bool is_local, Exploded* exploded) const {
  return ExplodeUsingIcu(ToRoundedDownMillisecondsSinceUnixEpoch(), is_local,
                         exploded);
}

// static
bool Time::FromExploded(bool is_local, const Exploded& exploded, Time* time) {
  int64_t millis_since_unix_epoch;
  if (FromExplodedUsingIcu(is_local, exploded, &millis_since_unix_epoch))
    return FromMillisecondsSinceUnixEpoch(millis_since_unix_epoch, time);
  *time = Time(0);
  return false;
}

#endif  // BUILDFLAG(IS_FUCHSIA)

}  // namespace base
