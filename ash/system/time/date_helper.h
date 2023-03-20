// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_DATE_HELPER_H_
#define ASH_SYSTEM_TIME_DATE_HELPER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/dtitvfmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

// A singleton class used to create and cache `GregorianCalendar`,
// `icu::SimpleDateFormat` and `icu::DateIntervalFormat` objects, so that they
// don't have to be recreated each time when querying the time difference or
// formatting a time. This improves performance since creating
// `icu::SimpleDateFormat` and `icu::DateIntervalFormat` objects is expensive.
class DateHelper : public LocaleChangeObserver,
                   public system::TimezoneSettings::Observer {
 public:
  // Returns the singleton instance.
  ASH_EXPORT static DateHelper* GetInstance();

  // Creates a formatter object used to format dates from the given `pattern`.
  icu::SimpleDateFormat CreateSimpleDateFormatter(const char* pattern);

  // Creates a formatter object used to format dates without calling the
  // `getBestPattern` function, which resolves the input pattern to the best
  // fit, which is not always what we want. e.g. 'mm' returns 'm' even though we
  // want it zero-padded (03 vs. 3 when given 12:03)
  icu::SimpleDateFormat CreateSimpleDateFormatterWithoutBestPattern(
      const char* pattern);

  // Creates a formatter object that extracts the hours field from a given date.
  // Uses `pattern` to differentiate between 12 and 24 hour clock formats.
  icu::SimpleDateFormat CreateHoursFormatter(const char* pattern);

  // Creates a date interval formatter object that formats a `DateInterval` into
  // text as compactly as possible.
  // Note that even if a pattern does not request a certain date part, it will
  // be automatically included if that part is different between two dates (e.g.
  // for `pattern=hm` (hours and minutes in twelve hour clock format),
  // "18 Nov 2021 8:30"..."18 Nov 2021 9:30" => "8:30 – 9:30 AM", but
  // "18 Nov 2021 8:30"..."19 Nov 2021 7:20" =>
  // "11/18/2021, 8:30 AM – 11/19/2021, 7:20 AM").
  std::unique_ptr<icu::DateIntervalFormat> CreateDateIntervalFormatter(
      const char* pattern);

  // Returns a formatted string of a `time` using the given `formatter`.
  std::u16string GetFormattedTime(const icu::DateFormat* formatter,
                                  const base::Time& time);

  // Returns a formatted interval string using the given `formatter`.
  ASH_EXPORT std::u16string GetFormattedInterval(
      const icu::DateIntervalFormat* formatter,
      const base::Time& start_time,
      const base::Time& end_time);

  // Get the time difference to UTC time based on the time passed in and the
  // system timezone. Daylight saving is considered.
  ASH_EXPORT base::TimeDelta GetTimeDifference(base::Time date) const;

  // Gets the local midnight in UTC time of the `date`.
  // e.g. If the `date` is Apr 1st 1:00 (which is Mar 31st 18:00 PST), the
  // local timezone is PST and time difference is 7 hrs. It returns Mar 31st
  // 7:00, which is Mar 31st 00:00 PST.
  ASH_EXPORT base::Time GetLocalMidnight(base::Time date);

  icu::SimpleDateFormat& day_of_month_formatter() {
    return day_of_month_formatter_;
  }

  icu::SimpleDateFormat& month_day_formatter() { return month_day_formatter_; }

  icu::SimpleDateFormat& month_day_year_formatter() {
    return month_day_year_formatter_;
  }

  icu::SimpleDateFormat& month_day_year_week_formatter() {
    return month_day_year_week_formatter_;
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

  icu::SimpleDateFormat& day_of_week_formatter() {
    return day_of_week_formatter_;
  }

  icu::SimpleDateFormat& week_title_formatter() {
    return week_title_formatter_;
  }

  icu::SimpleDateFormat& year_formatter() { return year_formatter_; }

  icu::SimpleDateFormat& twelve_hour_clock_hours_formatter() {
    return twelve_hour_clock_hours_formatter_;
  }

  icu::SimpleDateFormat& twenty_four_hour_clock_hours_formatter() {
    return twenty_four_hour_clock_hours_formatter_;
  }

  icu::SimpleDateFormat& minutes_formatter() { return minutes_formatter_; }

  const icu::DateIntervalFormat* twelve_hour_clock_interval_formatter() {
    return twelve_hour_clock_interval_formatter_.get();
  }

  const icu::DateIntervalFormat* twenty_four_hour_clock_interval_formatter() {
    return twenty_four_hour_clock_interval_formatter_.get();
  }

  std::vector<std::u16string> week_titles() { return week_titles_; }

  // Reset after a locale change in the test.
  ASH_EXPORT void ResetForTesting();

 private:
  friend base::DefaultSingletonTraits<DateHelper>;
  friend class DateHelperUnittest;
  DateHelper();

  DateHelper(const DateHelper& other) = delete;
  DateHelper& operator=(const DateHelper& other) = delete;

  ~DateHelper() override;

  // Resets the icu::SimpleDateFormat objects after a time zone change.
  ASH_EXPORT void ResetFormatters();

  // Calculates the week titles based on the language setting.
  ASH_EXPORT void CalculateLocalWeekTitles();

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // LocaleChangeObserver:
  // Although the device will restart whenever there's locale change and this
  // instance will be re-constructed, however this dose not cover all the cases.
  // The locale between the login screen and the user's screen can be different.
  // (For example: different languages are set in different accounts, and the
  // login screen will use the owener's locale setting.)
  void OnLocaleChanged() override;

  // Formatter for getting the day of month.
  icu::SimpleDateFormat day_of_month_formatter_;

  // Formatter for getting the month name and day of month.
  icu::SimpleDateFormat month_day_formatter_;

  // Formatter for getting the month name, day of month, and year.
  icu::SimpleDateFormat month_day_year_formatter_;

  // Formatter for getting the month, day, year and day of week.
  icu::SimpleDateFormat month_day_year_week_formatter_;

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

  // Formatter for getting the day of week. Returns 1 - 7.
  icu::SimpleDateFormat day_of_week_formatter_;

  // Formatter for getting the week title. e.g. M, T, W.
  icu::SimpleDateFormat week_title_formatter_;

  // Formatter for getting the year.
  icu::SimpleDateFormat year_formatter_;

  // Formatter for getting the hours in a 12 hour clock format.
  icu::SimpleDateFormat twelve_hour_clock_hours_formatter_;

  // Formatter for getting the hours in a 24 hour clock format.
  icu::SimpleDateFormat twenty_four_hour_clock_hours_formatter_;

  // Formatter for getting the minutes.
  icu::SimpleDateFormat minutes_formatter_;

  // Interval formatter for two dates. Formats time in twelve
  // hour clock format (e.g. 8:30 – 9:30 PM or 11:30 AM – 2:30 PM).
  std::unique_ptr<icu::DateIntervalFormat>
      twelve_hour_clock_interval_formatter_;

  // Interval formatter for two dates. Formats time in twenty
  // four hour clock format (e.g. 20:30 – 21:30).
  std::unique_ptr<icu::DateIntervalFormat>
      twenty_four_hour_clock_interval_formatter_;

  // Week title list based on the language setting. e.g. SMTWTFS in English.
  std::vector<std::u16string> week_titles_;

  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar_;

  base::ScopedObservation<system::TimezoneSettings,
                          system::TimezoneSettings::Observer>
      time_zone_settings_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_DATE_HELPER_H_
