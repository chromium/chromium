// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/date_helper.h"

#include "ash/shell.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/containers/contains.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/dtintrv.h"
#include "third_party/icu/source/i18n/unicode/dtitvfmt.h"
#include "third_party/icu/source/i18n/unicode/fieldpos.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"

namespace ash {

namespace {

// Milliseconds per minute.
constexpr int kMillisecondsPerMinute = 60000;

// Default week title for a few special languages that cannot find the start of
// a week. So far the known languages that cannot return their day of week are:
// 'bn', 'fa', 'mr', 'pa-PK'.
const std::vector<std::u16string> kDefaultWeekTitle = {u"S", u"M", u"T", u"W",
                                                       u"T", u"F", u"S"};

UDate TimeToUDate(const base::Time& time) {
  return static_cast<UDate>(time.InSecondsFSinceUnixEpoch() *
                            base::Time::kMillisecondsPerSecond);
}

// Receives an input `unicode_pattern` in the "Hm" format (HH:mm, aK:mm, h:mm a,
// a hh:mm, etc.) and extracts the hours part of the pattern.
icu::UnicodeString getHoursPattern(const icu::UnicodeString& unicode_pattern) {
  std::string pattern;
  unicode_pattern.toUTF8String(pattern);

  if (base::Contains(pattern, "hh")) {
    return icu::UnicodeString("hh");
  }
  if (base::Contains(pattern, "h")) {
    return icu::UnicodeString("h");
  }
  if (base::Contains(pattern, "HH")) {
    return icu::UnicodeString("HH");
  }
  if (base::Contains(pattern, "H")) {
    return icu::UnicodeString("H");
  }
  if (base::Contains(pattern, "KK")) {
    return icu::UnicodeString("KK");
  }
  if (base::Contains(pattern, "K")) {
    return icu::UnicodeString("K");
  }
  if (base::Contains(pattern, "kk")) {
    return icu::UnicodeString("kk");
  }
  if (base::Contains(pattern, "k")) {
    return icu::UnicodeString("k");
  }

  NOTREACHED() << "Hours pattern not found.";
}

}  // namespace

// static
DateHelper* DateHelper::GetInstance() {
  return base::Singleton<DateHelper>::get();
}

icu::SimpleDateFormat DateHelper::CreateSimpleDateFormatter(
    const char* pattern) {
  // Generate a locale-dependent format pattern. The generator will take
  // care of locale-dependent formatting issues like which separator to
  // use (some locales use '.' instead of ':'), and where to put the am/pm
  // marker.
  UErrorCode status = U_ZERO_ERROR;
  DCHECK(U_SUCCESS(status));
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(pattern), status);
  DCHECK(U_SUCCESS(status));

  // Then, create a formatter object using the generated pattern.
  icu::SimpleDateFormat formatter(generated_pattern, status);
  DCHECK(U_SUCCESS(status));

  return formatter;
}

icu::SimpleDateFormat DateHelper::CreateSimpleDateFormatterWithoutBestPattern(
    const char* pattern) {
  UErrorCode status = U_ZERO_ERROR;
  DCHECK(U_SUCCESS(status));
  icu::SimpleDateFormat formatter(icu::UnicodeString(pattern), status);
  DCHECK(U_SUCCESS(status));
  return formatter;
}

std::unique_ptr<icu::DateIntervalFormat>
DateHelper::CreateDateIntervalFormatter(const char* pattern) {
  UErrorCode status = U_ZERO_ERROR;
  icu::DateIntervalFormat* formatter =
      icu::DateIntervalFormat::createInstance(pattern, status);
  DCHECK(U_SUCCESS(status));
  return base::WrapUnique(formatter);
}

icu::SimpleDateFormat DateHelper::CreateHoursFormatter(const char* pattern) {
  UErrorCode status = U_ZERO_ERROR;
  DCHECK(U_SUCCESS(status));
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(pattern), status);
  DCHECK(U_SUCCESS(status));
  // Since ICU 74, getBestPattern can return a gibberish pattern ""H
  // ├'Minute': m┤ ├'Dayperiod': a┤"" if the locale resource is missing. Instead
  // of using the gibberish pattern, this should fallback to the proposed
  // pattern.
  std::string gen_string;
  generated_pattern.toUTF8String(gen_string);
  if (base::Contains(gen_string, "├")) {
    // Fallback to the suggested pattern.
    generated_pattern = icu::UnicodeString(pattern);
  }
  // Extract the hours from the generated pattern.
  icu::UnicodeString hours_pattern = getHoursPattern(generated_pattern);
  icu::SimpleDateFormat formatter(hours_pattern, status);
  DCHECK(U_SUCCESS(status));

  return formatter;
}

std::u16string DateHelper::GetFormattedTime(const icu::DateFormat* formatter,
                                            const base::Time& time) {
  DCHECK(formatter);
  icu::UnicodeString date_string;

  formatter->format(TimeToUDate(time), date_string);
  return base::i18n::UnicodeStringToString16(date_string);
}

std::u16string DateHelper::GetFormattedInterval(
    const icu::DateIntervalFormat* formatter,
    const base::Time& start_time,
    const base::Time& end_time) {
  DCHECK(formatter);
  UErrorCode status = U_ZERO_ERROR;
  icu::DateInterval interval(TimeToUDate(start_time), TimeToUDate(end_time));
  icu::FieldPosition position = 0;
  icu::UnicodeString interval_string;
  formatter->format(&interval, interval_string, position, status);
  DCHECK(U_SUCCESS(status));
  return base::i18n::UnicodeStringToString16(interval_string);
}

base::TimeDelta DateHelper::GetTimeDifference(base::Time date) const {
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  const base::TimeDelta raw_time_diff =
      base::Minutes(time_zone.getRawOffset() / kMillisecondsPerMinute);

  // Calculates the time difference adjust by the possible daylight savings
  // offset. If the status of any step fails, returns the default time
  // difference without considering daylight savings.
  if (!gregorian_calendar_) {
    return raw_time_diff;
  }

  UDate current_date = TimeToUDate(date);
  UErrorCode status = U_ZERO_ERROR;
  gregorian_calendar_->setTime(current_date, status);
  if (U_FAILURE(status)) {
    return raw_time_diff;
  }

  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar_->inDaylightTime(status);
  if (U_FAILURE(status)) {
    return raw_time_diff;
  }

  int gmt_offset = time_zone.getRawOffset();
  if (day_light) {
    gmt_offset += time_zone.getDSTSavings();
  }

  return base::Minutes(gmt_offset / kMillisecondsPerMinute);
}

base::Time DateHelper::GetLocalMidnight(base::Time date) {
  base::TimeDelta time_difference = GetTimeDifference(date);
  return (date + time_difference).UTCMidnight() - time_difference;
}

DateHelper::DateHelper()
    : day_of_month_formatter_(CreateSimpleDateFormatter("d")),
      month_day_formatter_(CreateSimpleDateFormatter("MMMMd")),
      month_day_year_formatter_(CreateSimpleDateFormatter("MMMMdyyyy")),
      month_day_year_week_formatter_(
          CreateSimpleDateFormatter("MMMMEEEEdyyyy")),
      month_name_formatter_(CreateSimpleDateFormatter("MMMM")),
      month_name_year_formatter_(CreateSimpleDateFormatter("MMMM yyyy")),
      time_zone_formatter_(CreateSimpleDateFormatter("zzzz")),
      twelve_hour_clock_formatter_(CreateSimpleDateFormatter("h:mm a")),
      twenty_four_hour_clock_formatter_(CreateSimpleDateFormatter("HH:mm")),
      day_of_week_formatter_(CreateSimpleDateFormatter("ee")),
      week_title_formatter_(CreateSimpleDateFormatter("EEEEE")),
      // Note: "yyyy" represents a four-digit calendar year (e.g. "2023"),
      // while "YYYY" represents a so called 'week year' (which might be "2022"
      // if the first day is on the last week of 2022).
      year_formatter_(CreateSimpleDateFormatter("yyyy")),
      twelve_hour_clock_hours_formatter_(CreateHoursFormatter("h:mm a")),
      twenty_four_hour_clock_hours_formatter_(CreateHoursFormatter("HH:mm")),
      minutes_formatter_(CreateSimpleDateFormatterWithoutBestPattern("mm")),
      twelve_hour_clock_interval_formatter_(CreateDateIntervalFormatter("hm")),
      twenty_four_hour_clock_interval_formatter_(
          CreateDateIntervalFormatter("Hm")) {
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();

  UErrorCode status = U_ZERO_ERROR;
  gregorian_calendar_ =
      std::make_unique<icu::GregorianCalendar>(time_zone, status);
  DCHECK(U_SUCCESS(status));
  CalculateLocalWeekTitles();
  time_zone_settings_observer_.Observe(system::TimezoneSettings::GetInstance());

  // Not using a scoped observer since the Shell can be destructed before this
  // `DateHelper` instance gets destructed.
  Shell::Get()->locale_update_controller()->AddObserver(this);
}

DateHelper::~DateHelper() {
  if (Shell::HasInstance()) {
    Shell::Get()->locale_update_controller()->RemoveObserver(this);
  }
}

void DateHelper::ResetFormatters() {
  day_of_month_formatter_ = CreateSimpleDateFormatter("d");
  month_day_formatter_ = CreateSimpleDateFormatter("MMMMd");
  month_day_year_formatter_ = CreateSimpleDateFormatter("MMMMdyyyy");
  month_day_year_week_formatter_ = CreateSimpleDateFormatter("MMMMEEEEdyyyy");
  month_name_formatter_ = CreateSimpleDateFormatter("MMMM");
  month_name_year_formatter_ = CreateSimpleDateFormatter("MMMM yyyy");
  time_zone_formatter_ = CreateSimpleDateFormatter("zzzz");
  twelve_hour_clock_formatter_ = CreateSimpleDateFormatter("h:mm a");
  twenty_four_hour_clock_formatter_ = CreateSimpleDateFormatter("HH:mm");
  day_of_week_formatter_ = CreateSimpleDateFormatter("ee");
  week_title_formatter_ = CreateSimpleDateFormatter("EEEEE");
  year_formatter_ = CreateSimpleDateFormatter("yyyy");
  twelve_hour_clock_hours_formatter_ = CreateHoursFormatter("h:mm a");
  twenty_four_hour_clock_hours_formatter_ = CreateHoursFormatter("HH:mm");
  minutes_formatter_ = CreateSimpleDateFormatterWithoutBestPattern("mm");
  twelve_hour_clock_interval_formatter_ = CreateDateIntervalFormatter("hm");
  twenty_four_hour_clock_interval_formatter_ =
      CreateDateIntervalFormatter("Hm");
}

void DateHelper::ResetForTesting() {
  ResetFormatters();
  CalculateLocalWeekTitles();
  gregorian_calendar_->setTimeZone(
      system::TimezoneSettings::GetInstance()->GetTimezone());
}

void DateHelper::CalculateLocalWeekTitles() {
  week_titles_.clear();

  // To avoid the DST difference, use a certain date here to calculate the week
  // titles, since there are no daylight saving starts/ends in June worldwide.
  // If the `DCHECK` fails, use `Now()`.
  base::Time start_date = base::Time::Now();
  bool result = base::Time::FromString("15 Jun 2021 10:00 GMT", &start_date);
  DCHECK(result);
  start_date = GetLocalMidnight(start_date);
  std::u16string day_of_week =
      GetFormattedTime(&day_of_week_formatter_, start_date);

  // For a few special locales the day of week is not in a number. In these
  // cases, use the default week titles.
  int day_int;
  if (!base::StringToInt(day_of_week, &day_int)) {
    week_titles_ = kDefaultWeekTitle;
    return;
  }

  int safe_index = 0;
  // Find a first day of a week.
  while (day_int != 1) {
    start_date += base::Hours(25);
    day_of_week = GetFormattedTime(&day_of_week_formatter_, start_date);
    result = base::StringToInt(day_of_week, &day_int);
    DCHECK(result);
    ++safe_index;
    if (safe_index == calendar_utils::kDateInOneWeek) {
      NOTREACHED() << "Should already find the first day within 7 times, since "
                      "there are only 7 days in a week";
    }
  }

  int day_index = 0;
  while (day_index < calendar_utils::kDateInOneWeek) {
    week_titles_.push_back(
        GetFormattedTime(&week_title_formatter_, start_date));
    start_date += base::Hours(25);
    ++day_index;
  }
}

void DateHelper::TimezoneChanged(const icu::TimeZone& timezone) {
  ResetFormatters();
  gregorian_calendar_->setTimeZone(
      system::TimezoneSettings::GetInstance()->GetTimezone());
  Shell::Get()->system_tray_model()->calendar_model()->RedistributeEvents();
  Shell::Get()->system_tray_model()->clock()->NotifyRefreshClock();
}

void DateHelper::OnLocaleChanged() {
  ResetFormatters();
  CalculateLocalWeekTitles();
}

}  // namespace ash
