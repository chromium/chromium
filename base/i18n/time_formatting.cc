// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include <string>
#include <string_view>

#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/tags.h"
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

UDate ToUDate(const Time& time) {
  return time.InMillisecondsFSinceUnixEpoch();
}

std::u16string TimeFormat(const icu::DateFormat& formatter, const Time& time) {
  icu::UnicodeString date_string;

  formatter.format(ToUDate(time), date_string);
  return i18n::UnicodeStringToString16(date_string);
}

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

icu::SimpleDateFormat CreateSimpleDateFormatter(
    std::string_view pattern,
    bool generate_pattern = true,
    const icu::Locale& locale = icu::Locale::getDefault()) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString generated_pattern(pattern.data(), pattern.length());

  if (generate_pattern) {
    // Generate a locale-dependent format pattern. The generator will take
    // care of locale-dependent formatting issues like which separator to
    // use (some locales use '.' instead of ':'), and where to put the am/pm
    // marker.
    std::unique_ptr<icu::DateTimePatternGenerator> generator(
        icu::DateTimePatternGenerator::createInstance(status));
    DCHECK(U_SUCCESS(status));
    generated_pattern = generator->getBestPattern(generated_pattern, status);
    DCHECK(U_SUCCESS(status));
  }

  // Then, format the time using the desired pattern.
  icu::SimpleDateFormat formatter(generated_pattern, locale, status);
  DCHECK(U_SUCCESS(status));

  return formatter;
}

}  // namespace

std::u16string TimeFormatTimeOfDay(const Time& time) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::T::Short().with_time_precision(
                i18n::DateTimeFormatterOptions::TimePrecision::kMinute));
}

std::u16string TimeFormatTimeOfDayWithMilliseconds(const Time& time) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::T::Short()
                .with_hour_clock_type(k24HourClock)
                .with_time_precision(
                    i18n::DateTimeFormatterOptions::TimePrecision::kSubsecond_3)
                .with_am_pm_clock_type(kDropAmPm));
}

std::u16string TimeFormatTimeOfDayWithHourClockType(const Time& time,
                                                    HourClockType type,
                                                    AmPmClockType ampm) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::T::Short()
                .with_hour_clock_type(type)
                .with_am_pm_clock_type(ampm)
                .with_time_precision(
                    i18n::DateTimeFormatterOptions::TimePrecision::kMinute));
}

std::u16string TimeFormatShortDate(const Time& time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMD::Medium());
}

std::u16string TimeFormatShortDateNumeric(const Time& time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMD::Short());
}

std::u16string TimeFormatShortDateAndTime(const Time& time) {
  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::YMDT::Short().with_time_precision(
                i18n::DateTimeFormatterOptions::TimePrecision::kSecond));
}

std::u16string TimeFormatShortDateAndTimeWithTimeZone(const Time& time) {
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
    const Time& time,
    const icu::TimeZone* time_zone) {
  DCHECK(time_zone);
  icu::UnicodeString id;
  time_zone->getID(id);
  std::string id_str;
  id.toUTF8String(id_str);

  return GetDateTimeFormatter().Format(
      time, i18n::datetime_options::YM::Long().with_time_zone(
                i18n::TimeZone::FromString(id_str)));
}
#endif

std::u16string TimeFormatMonthAndYear(const Time& time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YM::Long());
}

std::u16string TimeFormatFriendlyDateAndTime(const Time& time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMDET::Long());
}

std::u16string TimeFormatFriendlyDate(const Time& time) {
  return GetDateTimeFormatter().Format(time,
                                       i18n::datetime_options::YMDE::Long());
}

std::u16string LocalizedTimeFormatWithPattern(const Time& time,
                                              std::string_view pattern) {
  return TimeFormat(CreateSimpleDateFormatter(pattern), time);
}

std::string UnlocalizedTimeFormatWithPattern(const Time& time,
                                             std::string_view pattern,
                                             const icu::TimeZone* time_zone) {
  icu::SimpleDateFormat formatter =
      CreateSimpleDateFormatter({}, false, icu::Locale("en_US"));
  if (time_zone) {
    formatter.setTimeZone(*time_zone);
  }

  // Formats `time` according to `pattern`.
  const auto format_time = [&formatter](const Time& time,
                                        std::string_view pattern) {
    formatter.applyPattern(
        icu::UnicodeString(pattern.data(), pattern.length()));
    return base::UTF16ToUTF8(TimeFormat(formatter, time));
  };

  // If `time` has nonzero microseconds, check if the caller requested
  // microsecond-precision output; this must be handled internally since
  // `SimpleDateFormat` won't do it.
  std::string output;
  if (const int64_t microseconds =
          time.ToDeltaSinceWindowsEpoch().InMicroseconds() %
          Time::kMicrosecondsPerMillisecond) {
    // Adds digits to `output` for each 'S' at the start of `pattern`.
    const auto format_microseconds = [&output](int64_t mutable_micros,
                                               std::string_view pattern) {
      size_t i = 0;
      for (; i < pattern.length() && pattern[i] == 'S'; ++i) {
        output += static_cast<char>('0' + mutable_micros / 100);
        mutable_micros = (mutable_micros % 100) * 10;
      }
      return i;
    };

    // Look for fractional seconds patterns with greater-than-millisecond
    // precision.
    bool in_quotes = false;
    for (size_t i = 0; i < pattern.length();) {
      if (pattern[i] == '\'') {
        in_quotes = !in_quotes;
      } else if (!in_quotes && !pattern.compare(i, 4, "SSSS")) {
        // Let ICU format everything up through milliseconds.
        const size_t fourth_s = i + 3;
        if (i != 0) {
          output += format_time(time, pattern.substr(0, fourth_s));
        }

        // Add microseconds digits, then truncate to the remaining pattern.
        pattern = pattern.substr(
            fourth_s +
            format_microseconds(microseconds, pattern.substr(fourth_s)));
        i = 0;
        continue;
      }
      ++i;
    }
  }

  // Format any remaining pattern.
  if (!pattern.empty()) {
    output += format_time(time, pattern);
  }
  return output;
}

std::string TimeFormatAsIso8601(const Time& time) {
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return StringPrintf("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded.year,
                      exploded.month, exploded.day_of_month, exploded.hour,
                      exploded.minute, exploded.second, exploded.millisecond);
}

std::string TimeFormatHTTP(const Time& time) {
  // Get the weekday and month names as unlocalized English (RFC 7231 fixes them
  // to English) in GMT (to match the `UTCExplode()` below). `Format()` would
  // otherwise use the process default locale and the local timezone: a non-
  // English locale would localize the names, and for a near-midnight-UTC
  // instant in a non-GMT zone the weekday/month would disagree with the UTC day
  // (e.g. "Sat, 01 Apr" for a Sunday, May 1 GMT instant).
  const i18n::TimeZone gmt = i18n::TimeZone::GMT();
  static constexpr i18n::LanguageTag en_us = i18n::GetKnownLanguageTag("en-US");
  std::string day_of_week = base::UTF16ToUTF8(GetDateTimeFormatter().Format(
      time, en_us, i18n::datetime_options::E::Short().with_time_zone(gmt)));
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
  // TODO(satorux,jshin): Rework this with ures_getByKeyWithFallback()
  // once it becomes public. The short time format can be found at
  // "calendar/gregorian/DateTimePatterns/3" in the resources.
  std::unique_ptr<icu::SimpleDateFormat> formatter(
      static_cast<icu::SimpleDateFormat*>(
          icu::DateFormat::createTimeInstance(icu::DateFormat::kShort)));
  // Retrieve the short time format.
  icu::UnicodeString pattern_unicode;
  formatter->toPattern(pattern_unicode);

  // Determine what hour clock type the current locale uses, by checking
  // "a" (am/pm marker) in the short time format. This is reliable as "a"
  // is used by all of 12-hour clock formats, but not any of 24-hour clock
  // formats, as shown below.
  //
  // % grep -A4 DateTimePatterns third_party/icu/source/data/locales/*.txt |
  //   grep -B1 -- -- |grep -v -- '--' |
  //   perl -nle 'print $1 if /^\S+\s+"(.*)"/' |sort -u
  //
  // H.mm
  // H:mm
  // HH.mm
  // HH:mm
  // a h:mm
  // ah:mm
  // ahh:mm
  // h-mm a
  // h:mm a
  // hh:mm a
  //
  // See http://userguide.icu-project.org/formatparse/datetime for details
  // about the date/time format syntax.
  return pattern_unicode.indexOf('a') == -1 ? k24HourClock : k12HourClock;
}

}  // namespace base
