// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include <string>
#include <string_view>

#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/timezone.h"
#include "base/i18n/unicodestring.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
namespace {

const i18n::IcuBridge::DateTimeFormatter& GetDateTimeFormatter() {
  return i18n::IcuBridge::GetInstance().date_time_formatter();
}

UMeasureFormatWidth DurationWidthToMeasureWidth(DurationFormatWidth width) {
  switch (width) {
    case DURATION_WIDTH_WIDE:
      return UMEASFMT_WIDTH_WIDE;
    case DURATION_WIDTH_SHORT:
      return UMEASFMT_WIDTH_SHORT;
    case DURATION_WIDTH_NARROW:
      return UMEASFMT_WIDTH_NARROW;
    case DURATION_WIDTH_NUMERIC:
      return UMEASFMT_WIDTH_NUMERIC;
  }
  NOTREACHED();
}

}  // namespace

std::u16string TimeFormatTimeOfDay(Time time) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::T::Short().with_time_precision(
                i18n::DateTimeFormatterOptions::TimePrecision::kMinute));
}

std::u16string TimeFormatTimeOfDayWithMilliseconds(Time time) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::T::Short()
                .with_hour_clock_type(k24HourClock)
                .with_time_precision(
                    i18n::DateTimeFormatterOptions::TimePrecision::kSubsecond_3)
                .with_am_pm_clock_type(kDropAmPm));
}

std::u16string TimeFormatTimeOfDayWithHourClockType(Time time,
                                                    HourClockType type,
                                                    AmPmClockType ampm) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::T::Short()
                .with_hour_clock_type(type)
                .with_am_pm_clock_type(ampm)
                .with_time_precision(
                    i18n::DateTimeFormatterOptions::TimePrecision::kMinute));
}

std::u16string TimeFormatShortDate(Time time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMD::Medium());
}

std::u16string TimeFormatShortDateNumeric(Time time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMD::Short());
}

std::u16string TimeFormatShortDateAndTime(Time time) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::YMDT::Short().with_time_precision(
                i18n::DateTimeFormatterOptions::TimePrecision::kSecond));
}

std::u16string TimeFormatShortDateAndTimeWithTimeZone(Time time) {
  return GetDateTimeFormatter().Format(
      time,
      i18n::datetime_options::YMDT::Short()
          .with_time_precision(
              i18n::DateTimeFormatterOptions::TimePrecision::kSecond)
          .with_time_zone_style(
              i18n::DateTimeFormatterOptions::TimeZoneStyle::kShortSpecific));
}

#if BUILDFLAG(IS_CHROMEOS)
std::u16string TimeFormatMonthAndYearForTimeZone(
    Time time,
    const icu::TimeZone* time_zone) {
  DCHECK(time_zone);
  icu::UnicodeString id;
  time_zone->getID(id);
  std::string id_str;
  id.toUTF8String(id_str);

  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::YM::Long().with_time_zone(
                base::i18n::TimeZone::FromString(id_str)));
}
#endif

std::u16string TimeFormatMonthAndYear(Time time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YM::Long());
}

std::u16string TimeFormatFriendlyDateAndTime(Time time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMDET::Long());
}

std::u16string TimeFormatFriendlyDate(Time time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMDE::Long());
}

std::string TimeFormatAsIso8601(Time time) {
  return TimeFormatAsIso8601(
      time, i18n::TimeZone::GMT(),
      i18n::DateTimeFormatterOptions::TimePrecision::kSubsecond_3,
      /*include_offset_suffix=*/true);
}

std::string TimeFormatAsIso8601(
    Time time,
    const i18n::TimeZone& time_zone,
    const i18n::DateTimeFormatterOptions::TimePrecision& precision,
    bool include_offset_suffix) {
  base::TimeDelta raw_offset;
  base::TimeDelta dst_offset;
  time_zone.GetOffset(time, /*is_local=*/false, raw_offset, dst_offset);
  base::TimeDelta total_offset = raw_offset + dst_offset;

  Time local_time = time + total_offset;
  Time::Exploded exploded;
  local_time.UTCExplode(&exploded);

  std::string offset_suffix;
  if (include_offset_suffix) {
    if (time_zone == i18n::TimeZone::GMT()) {
      offset_suffix = "Z";
    } else {
      int total_minutes = total_offset.InMinutes();
      char sign = total_minutes >= 0 ? '+' : '-';
      total_minutes = std::abs(total_minutes);
      int hours = total_minutes / 60;
      int minutes = total_minutes % 60;
      offset_suffix = StringPrintf("%c%02d:%02d", sign, hours, minutes);
    }
  }

  std::string formatted_time =
      StringPrintf("%04d-%02d-%02dT%02d", exploded.year, exploded.month,
                   exploded.day_of_month, exploded.hour);
  using TimePrecision = i18n::DateTimeFormatterOptions::TimePrecision;
  if (precision == TimePrecision::kHour) {
    return formatted_time + offset_suffix;
  }
  formatted_time += StringPrintf(":%02d", exploded.minute);
  if (precision == TimePrecision::kMinute) {
    return formatted_time + offset_suffix;
  }
  formatted_time += StringPrintf(":%02d", exploded.second);
  if (precision == TimePrecision::kSecond) {
    return formatted_time + offset_suffix;
  }
  if (precision == TimePrecision::kSubsecond_2) {
    formatted_time += StringPrintf(".%02d", exploded.millisecond / 10);
    return formatted_time + offset_suffix;
  }
  formatted_time += StringPrintf(".%03d", exploded.millisecond);
  if (precision == TimePrecision::kSubsecond_4) {
    formatted_time += "0";
  }

  return formatted_time + offset_suffix;
}

std::string TimeFormatAsIso8601WithTimeZone(Time time,
                                            const i18n::TimeZone& time_zone,
                                            bool include_offset_suffix) {
  using TimePrecision = i18n::DateTimeFormatterOptions::TimePrecision;
  return TimeFormatAsIso8601(time, time_zone, TimePrecision::kSubsecond_3,
                             include_offset_suffix);
}

std::string TimeFormatUnix(Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);

  std::unique_ptr<icu::TimeZone> local_tz(icu::TimeZone::createDefault());
  int32_t raw_offset = 0;
  int32_t dst_offset = 0;
  UErrorCode status = U_ZERO_ERROR;
  local_tz->getOffset(
      static_cast<UDate>(time.InSecondsFSinceUnixEpoch() * 1000),
      /*local=*/false, raw_offset, dst_offset, status);
  base::TimeDelta total_offset =
      base::Milliseconds(raw_offset) + base::Milliseconds(dst_offset);

  int total_minutes = total_offset.InMinutes();
  char sign = total_minutes >= 0 ? '+' : '-';
  total_minutes = std::abs(total_minutes);
  int hours = total_minutes / 60;
  int minutes = total_minutes % 60;

  int64_t micros = time.ToDeltaSinceWindowsEpoch().InMicroseconds() % 1000000;

  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%06lld%c%02d:%02d", exploded.year,
      exploded.month, exploded.day_of_month, exploded.hour, exploded.minute,
      exploded.second, static_cast<long long>(micros), sign, hours, minutes);
}

std::string TimeFormatHTTP(Time time) {
  // Get the weekday and month names as unlocalized English (RFC 7231 fixes them
  // to English) in GMT (to match the `UTCExplode()` below). `Format()` would
  // otherwise use the process default locale and the local timezone: a non-
  // English locale would localize the names, and for a near-midnight-UTC
  // instant in a non-GMT zone the weekday/month would disagree with the UTC day
  // (e.g. "Sat, 01 Apr" for a Sunday, May 1 GMT instant).
  const base::i18n::TimeZone gmt = base::i18n::TimeZone::GMT();
  static constexpr i18n::LanguageTag en_us = i18n::GetKnownLanguageTag("en-US");
  std::string day_of_week = base::UTF16ToUTF8(GetDateTimeFormatter().Format(
      time, en_us, i18n::datetime_options::E::Medium().with_time_zone(gmt)));
  std::string month_long = base::UTF16ToUTF8(GetDateTimeFormatter().Format(
      time, en_us, i18n::datetime_options::M::Medium().with_time_zone(gmt)));
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  // This is mimic the skeleton: "E, dd MMM yyyy HH:mm:ss 'GMT'"
  // https://www.rfc-editor.org/rfc/rfc7231#section-7.1.1.1
  return StringPrintf("%s, %02d %s %04d %02d:%02d:%02d GMT", day_of_week,
                      exploded.day_of_month, month_long, exploded.year,
                      exploded.hour, exploded.minute, exploded.second);
}

bool TimeDurationFormat(TimeDelta time,
                        DurationFormatWidth width,
                        std::u16string* out) {
  DCHECK(out);
  UErrorCode status = U_ZERO_ERROR;
  const int total_minutes = ClampRound(time / base::Minutes(1));
  const int hours = total_minutes / 60;
  const int minutes = total_minutes % 60;
  UMeasureFormatWidth u_width = DurationWidthToMeasureWidth(width);

  const icu::Measure measures[] = {
      icu::Measure(hours, icu::MeasureUnit::createHour(status), status),
      icu::Measure(minutes, icu::MeasureUnit::createMinute(status), status)};
  icu::MeasureFormat measure_format(icu::Locale::getDefault(), u_width, status);
  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);
  measure_format.formatMeasures(measures, 2, formatted, ignore, status);
  *out = i18n::UnicodeStringToString16(formatted);
  return U_SUCCESS(status);
}

bool TimeDurationFormatWithSeconds(TimeDelta time,
                                   DurationFormatWidth width,
                                   std::u16string* out) {
  DCHECK(out);
  UErrorCode status = U_ZERO_ERROR;
  const int64_t total_seconds = ClampRound<int64_t>(time.InSecondsF());
  const int64_t hours = total_seconds / base::Time::kSecondsPerHour;
  const int64_t minutes =
      (total_seconds - hours * base::Time::kSecondsPerHour) /
      base::Time::kSecondsPerMinute;
  const int64_t seconds = total_seconds % base::Time::kSecondsPerMinute;
  UMeasureFormatWidth u_width = DurationWidthToMeasureWidth(width);

  const icu::Measure measures[] = {
      icu::Measure(hours, icu::MeasureUnit::createHour(status), status),
      icu::Measure(minutes, icu::MeasureUnit::createMinute(status), status),
      icu::Measure(seconds, icu::MeasureUnit::createSecond(status), status)};
  icu::MeasureFormat measure_format(icu::Locale::getDefault(), u_width, status);
  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);
  measure_format.formatMeasures(measures, 3, formatted, ignore, status);
  *out = i18n::UnicodeStringToString16(formatted);
  return U_SUCCESS(status);
}

bool TimeDurationCompactFormatWithSeconds(TimeDelta time,
                                          DurationFormatWidth width,
                                          std::u16string* out) {
  DCHECK(out);
  UErrorCode status = U_ZERO_ERROR;
  const int64_t total_seconds = ClampRound<int64_t>(time.InSecondsF());
  const int64_t hours = total_seconds / base::Time::kSecondsPerHour;
  const int64_t minutes =
      (total_seconds - hours * base::Time::kSecondsPerHour) /
      base::Time::kSecondsPerMinute;
  const int64_t seconds = total_seconds % base::Time::kSecondsPerMinute;
  UMeasureFormatWidth u_width = DurationWidthToMeasureWidth(width);
  const icu::Measure hours_measure =
      icu::Measure(hours, icu::MeasureUnit::createHour(status), status);
  const icu::Measure minutes_measure =
      icu::Measure(minutes, icu::MeasureUnit::createMinute(status), status);
  const icu::Measure seconds_measure =
      icu::Measure(seconds, icu::MeasureUnit::createSecond(status), status);
  icu::MeasureFormat measure_format(icu::Locale::getDefault(), u_width, status);
  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);
  if (hours != 0 || width == DurationFormatWidth::DURATION_WIDTH_NUMERIC) {
    icu::Measure input_measures[3]{hours_measure, minutes_measure,
                                   seconds_measure};
    measure_format.formatMeasures(input_measures, 3, formatted, ignore, status);
  } else if (minutes != 0) {
    icu::Measure input_measures[2]{minutes_measure, seconds_measure};
    measure_format.formatMeasures(input_measures, 2, formatted, ignore, status);
  } else {
    icu::Measure input_measures[1]{seconds_measure};
    measure_format.formatMeasures(input_measures, 1, formatted, ignore, status);
  }
  *out = i18n::UnicodeStringToString16(formatted);
  return U_SUCCESS(status);
}

HourClockType GetHourClockType() {
  return GetDateTimeFormatter().GetHourClockType();
}

}  // namespace base
