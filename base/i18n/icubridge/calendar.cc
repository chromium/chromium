// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/calendar.h"

#include <memory>

#include "base/check.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"

namespace base::i18n {

namespace {

IcuBridge::Calendar::Weekday ToWeekday(UCalendarDaysOfWeek day) {
  switch (day) {
    case UCAL_SUNDAY:
      return IcuBridge::Calendar::Weekday::kSunday;
    case UCAL_MONDAY:
      return IcuBridge::Calendar::Weekday::kMonday;
    case UCAL_TUESDAY:
      return IcuBridge::Calendar::Weekday::kTuesday;
    case UCAL_WEDNESDAY:
      return IcuBridge::Calendar::Weekday::kWednesday;
    case UCAL_THURSDAY:
      return IcuBridge::Calendar::Weekday::kThursday;
    case UCAL_FRIDAY:
      return IcuBridge::Calendar::Weekday::kFriday;
    case UCAL_SATURDAY:
      return IcuBridge::Calendar::Weekday::kSaturday;
  }
  return IcuBridge::Calendar::Weekday::kSunday;
}

IcuBridge::Calendar::WeekInformation GetWeekInfoWithIcuLocale(
    const icu::Locale& icu_locale) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar(
      icu::Calendar::createInstance(icu_locale, status));

  IcuBridge::Calendar::WeekInformation info;
  if (U_FAILURE(status) || !calendar) {
    // Fallback values: Sunday is first day of week, Saturday & Sunday are
    // weekend.
    info.first_weekday = IcuBridge::Calendar::Weekday::kSunday;
    info.weekend = {IcuBridge::Calendar::Weekday::kSaturday,
                    IcuBridge::Calendar::Weekday::kSunday};
    return info;
  }

  status = U_ZERO_ERROR;
  UCalendarDaysOfWeek first_day = calendar->getFirstDayOfWeek(status);
  if (U_SUCCESS(status)) {
    info.first_weekday = ToWeekday(first_day);
  } else {
    info.first_weekday = IcuBridge::Calendar::Weekday::kSunday;
  }

  for (int d = UCAL_SUNDAY; d <= UCAL_SATURDAY; ++d) {
    status = U_ZERO_ERROR;
    UCalendarWeekdayType day_type =
        calendar->getDayOfWeekType(static_cast<UCalendarDaysOfWeek>(d), status);
    if (U_SUCCESS(status) && day_type != UCAL_WEEKDAY) {
      info.weekend.insert(ToWeekday(static_cast<UCalendarDaysOfWeek>(d)));
    }
  }

  return info;
}

}  // namespace

IcuBridge::Calendar::WeekInformation IcuBridge::Calendar::GetWeekInformation()
    const {
  return GetWeekInfoWithIcuLocale(icu::Locale::getDefault());
}

}  // namespace base::i18n
