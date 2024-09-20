// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/i18n/unicodestring.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtitvfmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/fmtable.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
namespace {

UDate ToUDate(const Time& time) {
  // TODO(crbug.com/40247732): Consider using the `...IgnoringNull` variant and
  // adding a `CHECK(!time.is_null())`; trying to format a null Time as a string
  // is almost certainly an indication that the caller has made a mistake.
  return time.InMillisecondsFSinceUnixEpoch();
}

std::u16string TimeFormat(const icu::DateFormat& formatter, const Time& time) {
  icu::UnicodeString date_string;

  formatter.format(ToUDate(time), date_string);
  return i18n::UnicodeStringToString16(date_string);
}

std::u16string TimeFormatWithoutAmPm(const icu::DateFormat* formatter,
                                     const Time& time) {
  DCHECK(formatter);
  icu::UnicodeString time_string;

  icu::FieldPosition ampm_field(icu::DateFormat::kAmPmField);
  formatter->format(ToUDate(time), time_string, ampm_field);
  int ampm_length = ampm_field.getEndIndex() - ampm_field.getBeginIndex();
  if (ampm_length) {
    int begin = ampm_field.getBeginIndex();
    // Doesn't include any spacing before the field.
    if (begin)
      begin--;
    time_string.removeBetween(begin, ampm_field.getEndIndex());
  }
  return i18n::UnicodeStringToString16(time_string);
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

UMeasureFormatWidth DurationWidthToMeasureWidth(DurationFormatWidth width) {
  switch (width) {
    case DURATION_WIDTH_WIDE: return UMEASFMT_WIDTH_WIDE;
    case DURATION_WIDTH_SHORT: return UMEASFMT_WIDTH_SHORT;
    case DURATION_WIDTH_NARROW: return UMEASFMT_WIDTH_NARROW;
    case DURATION_WIDTH_NUMERIC: return UMEASFMT_WIDTH_NUMERIC;
  }
  NOTREACHED();
}

const char* DateFormatToString(DateFormat format) {
  switch (format) {
    case DATE_FORMAT_YEAR_MONTH:
      return UDAT_YEAR_MONTH;
    case DATE_FORMAT_MONTH_WEEKDAY_DAY:
      return UDAT_MONTH_WEEKDAY_DAY;
  }
  NOTREACHED();
}

}  // namespace

std::u16string TimeFormatTimeOfDay(const Time& time) {
  // We can omit the locale parameter because the default should match
  // Chrome's application locale.
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createTimeInstance(icu::DateFormat::kShort));
  return TimeFormat(*formatter, time);
}

std::u16string TimeFormatTimeOfDayWithMilliseconds(const Time& time) {
  icu::SimpleDateFormat formatter = CreateSimpleDateFormatter("HmsSSS");
  return TimeFormatWithoutAmPm(&formatter, time);
}

std::u16string TimeFormatTimeOfDayWithHourClockType(const Time& time,
                                                    HourClockType type,
                                                    AmPmClockType ampm) {
  // Just redirect to the normal function if the default type matches the
  // given type.
  HourClockType default_type = GetHourClockType();
  if (default_type == type && (type == k24HourClock || ampm == kKeepAmPm)) {
    return TimeFormatTimeOfDay(time);
  }

  const char* base_pattern = (type == k12HourClock ? "ahm" : "Hm");
  icu::SimpleDateFormat formatter = CreateSimpleDateFormatter(base_pattern);

  return (ampm == kKeepAmPm) ? TimeFormat(formatter, time)
                             : TimeFormatWithoutAmPm(&formatter, time);
}

std::u16string TimeFormatShortDate(const Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  return TimeFormat(*formatter, time);
}

std::u16string TimeFormatShortDateNumeric(const Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kShort));
  return TimeFormat(*formatter, time);
}

std::u16string TimeFormatShortDateAndTime(const Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kShort));
  return TimeFormat(*formatter, time);
}

std::u16string TimeFormatShortDateAndTimeWithTimeZone(const Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kShort,
                                              icu::DateFormat::kLong));
  return TimeFormat(*formatter, time);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string TimeFormatMonthAndYearForTimeZone(
    const Time& time,
    const icu::TimeZone* time_zone) {
  icu::SimpleDateFormat formatter =
      CreateSimpleDateFormatter(DateFormatToString(DATE_FORMAT_YEAR_MONTH));
  formatter.setTimeZone(*time_zone);
  return TimeFormat(formatter, time);
}
#endif

std::u16string TimeFormatMonthAndYear(const Time& time) {
  return TimeFormat(
      CreateSimpleDateFormatter(DateFormatToString(DATE_FORMAT_YEAR_MONTH)),
      time);
}

std::u16string TimeFormatFriendlyDateAndTime(const Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kFull));
  return TimeFormat(*formatter, time);
}

std::u16string TimeFormatFriendlyDate(const Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kFull));
  return TimeFormat(*formatter, time);
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
  return UnlocalizedTimeFormatWithPattern(time, "yyyy-MM-dd'T'HH:mm:ss.SSSX",
                                          icu::TimeZone::getGMT());
}

std::string TimeFormatHTTP(const Time& time) {
  return UnlocalizedTimeFormatWithPattern(time, "E, dd MMM yyyy HH:mm:ss O",
                                          icu::TimeZone::getGMT());
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

std::u16string DateIntervalFormat(const Time& begin_time,
                                  const Time& end_time,
                                  DateFormat format) {
  UErrorCode status = U_ZERO_ERROR;

  std::unique_ptr<icu::DateIntervalFormat> formatter(
      icu::DateIntervalFormat::createInstance(DateFormatToString(format),
                                              status));

  icu::FieldPosition pos = 0;
  UDate start_date = ToUDate(begin_time);
  UDate end_date = ToUDate(end_time);
  icu::DateInterval interval(start_date, end_date);
  icu::UnicodeString formatted;
  formatter->format(&interval, formatted, pos, status);
  return i18n::UnicodeStringToString16(formatted);
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
