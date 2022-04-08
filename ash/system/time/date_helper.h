// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_DATE_HELPER_H_
#define ASH_SYSTEM_TIME_DATE_HELPER_H_

#include <string>
#include "ash/components/settings/timezone_settings.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

// A singleton class used to create and cache `GregorianCalendar ` and
// `icu::SimpleDateFormat` objects, so that they don't have to be recreated each
// time when querying the time difference or formating a time. This improves
// performance since creating icu::SimpleDateFormat objects is expensive.
class DateHelper : public system::TimezoneSettings::Observer {
 public:
  // Returns the singleton instance.
  static DateHelper* GetInstance();

  // Creates a formatter object used to format dates from the given `pattern`.
  icu::SimpleDateFormat CreateSimpleDateFormatter(const char* pattern);

  // Returns a formatted string of a `time` using the given `formatter`.
  std::u16string GetFormattedTime(const icu::DateFormat* formatter,
                                  const base::Time& time);

  // Get the time difference to UTC time based on the time passed in and the
  // system timezone. Daylight saving is considered.
  int GetTimeDifferenceInMinutes(base::Time date);

  icu::SimpleDateFormat& day_of_month_formatter() {
    return day_of_month_formatter_;
  }

  icu::SimpleDateFormat& month_day_formatter() { return month_day_formatter_; }

  icu::SimpleDateFormat& month_day_year_formatter() {
    return month_day_year_formatter_;
  }

  icu::SimpleDateFormat& month_name_formatter() {
    return month_name_formatter_;
  }

  icu::SimpleDateFormat& month_name_year_formatter() {
    return month_name_year_formatter_;
  }

  icu::SimpleDateFormat& time_zone_formatter() { return time_zone_formatter_; }

  icu::SimpleDateFormat& twelve_hour_clock_formatter() {
    return twelve_hour_clock_formatter_;
  }

  icu::SimpleDateFormat& twenty_four_hour_clock_formatter() {
    return twenty_four_hour_clock_formatter_;
  }

  icu::SimpleDateFormat& year_formatter() { return year_formatter_; }

 private:
  friend base::DefaultSingletonTraits<DateHelper>;
  DateHelper();

  DateHelper(const DateHelper& other) = delete;
  DateHelper& operator=(const DateHelper& other) = delete;

  ~DateHelper() override;

  // Resets the icu::SimpleDateFormat objects after a time zone change.
  void ResetFormatters();

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // Formatter for getting the day of month.
  icu::SimpleDateFormat day_of_month_formatter_;

  // Formatter for getting the month name and day of month.
  icu::SimpleDateFormat month_day_formatter_;

  // Formatter for getting the month name, day of month, and year.
  icu::SimpleDateFormat month_day_year_formatter_;

  // Formatter for getting the name of month.
  icu::SimpleDateFormat month_name_formatter_;

  // Formatter for getting the month name and year.
  icu::SimpleDateFormat month_name_year_formatter_;

  // Formatter for getting the time zone.
  icu::SimpleDateFormat time_zone_formatter_;

  // Formatter for 12 hour clock hours and minutes.
  icu::SimpleDateFormat twelve_hour_clock_formatter_;

  // Formatter for 24 hour clock hours and minutes.
  icu::SimpleDateFormat twenty_four_hour_clock_formatter_;

  // Formatter for getting the year.
  icu::SimpleDateFormat year_formatter_;

  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar_;

  base::ScopedObservation<system::TimezoneSettings,
                          system::TimezoneSettings::Observer>
      time_zone_settings_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_DATE_HELPER_H_
