// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/date_time_formatter.h"

#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/icu_bridge_helpers.h"
#include "base/i18n/language_tag.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base::i18n {

namespace {

// DateTime Formatting Helpers
UDate ToUDate(const base::Time& time) {
  return time.InMillisecondsFSinceUnixEpoch();
}

std::u16string DateTimeFormat(
    const icu::DateFormat& formatter,
    const base::Time& time,
    std::optional<base::AmPmClockType> am_pm_type = std::nullopt) {
  icu::UnicodeString date_string;

  if (am_pm_type == base::kDropAmPm) {
    icu::FieldPosition ampm_field(icu::DateFormat::kAmPmField);
    formatter.format(ToUDate(time), date_string, ampm_field);
    int ampm_length = ampm_field.getEndIndex() - ampm_field.getBeginIndex();
    if (ampm_length) {
      int begin = ampm_field.getBeginIndex();
      // Doesn't include any spacing before the field.
      if (begin) {
        begin--;
      }
      date_string.removeBetween(begin, ampm_field.getEndIndex());
    }
  } else {
    formatter.format(ToUDate(time), date_string);
  }

  return base::i18n::UnicodeStringToString16(date_string);
}

icu::SimpleDateFormat CreateSimpleDateFormatter(
    const icu::UnicodeString& pattern,
    const icu::Locale& locale = icu::Locale::getDefault()) {
  UErrorCode status = U_ZERO_ERROR;
  // Then, format the time using the desired pattern.
  icu::SimpleDateFormat formatter(pattern, locale, status);
  if (U_SUCCESS(status)) {
    return formatter;
  }

  // Fallback if the generated pattern failed (e.g. due to unsupported fields
  // in some locales on limited ICU data platforms).
  status = U_ZERO_ERROR;
  return icu::SimpleDateFormat(icu::UnicodeString("yyyy-MM-dd HH:mm:ss"),
                               locale, status);
}

icu::DateFormat::EStyle ToIcuStyle(
    DateTimeFormatterOptions::ItemLength length) {
  switch (length) {
    case DateTimeFormatterOptions::ItemLength::kLong:
      return icu::DateFormat::kFull;
    case DateTimeFormatterOptions::ItemLength::kMedium:
      return icu::DateFormat::kMedium;
    case DateTimeFormatterOptions::ItemLength::kShort:
      return icu::DateFormat::kShort;
    case DateTimeFormatterOptions::ItemLength::kNone:
      return icu::DateFormat::kNone;
  }
  NOTREACHED();
}

// Constructs a pattern using icu::DateFormat for the given length.
icu::UnicodeString GetPatternForLength(
    const icu::Locale& locale,
    DateTimeFormatterOptions::ItemLength length) {
  icu::DateFormat::EStyle style_length = ToIcuStyle(length);
  std::unique_ptr<icu::DateFormat> fmt(icu::DateFormat::createDateTimeInstance(
      style_length, style_length, locale));
  if (fmt->getDynamicClassID() != icu::SimpleDateFormat::getStaticClassID()) {
    return u"";
  }
  auto* simpleFmt = static_cast<icu::SimpleDateFormat*>(fmt.get());
  if (simpleFmt == nullptr) {
    return u"";
  }
  icu::UnicodeString icu_pattern;
  simpleFmt->toPattern(icu_pattern);
  return icu_pattern;
}

struct SkeletonOptions {
  bool has_year;
  bool has_month;
  bool has_day;
  bool has_weekday;
  bool has_time;
};

SkeletonOptions GetSkeletonOptions(
    DateTimeFormatterOptions::FormatIdentifier format_identifier) {
  SkeletonOptions options{.has_year = false,
                          .has_month = false,
                          .has_day = false,
                          .has_weekday = false,
                          .has_time = false};

  switch (format_identifier) {
    case DateTimeFormatterOptions::FormatIdentifier::kD:
      options.has_day = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kDE:
      options.has_day = true;
      options.has_weekday = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kDT:
      options.has_day = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kDET:
      options.has_day = true;
      options.has_weekday = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kE:
      options.has_weekday = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kET:
      options.has_weekday = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kM:
      options.has_month = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMD:
      options.has_month = true;
      options.has_day = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMDE:
      options.has_month = true;
      options.has_day = true;
      options.has_weekday = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMDT:
      options.has_month = true;
      options.has_day = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMDET:
      options.has_month = true;
      options.has_day = true;
      options.has_weekday = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kT:
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kY:
      options.has_year = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYM:
      options.has_year = true;
      options.has_month = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMD:
      options.has_year = true;
      options.has_month = true;
      options.has_day = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMDT:
      options.has_year = true;
      options.has_month = true;
      options.has_day = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMDE:
      options.has_year = true;
      options.has_month = true;
      options.has_day = true;
      options.has_weekday = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMDET:
      options.has_year = true;
      options.has_month = true;
      options.has_day = true;
      options.has_weekday = true;
      options.has_time = true;
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kNone:
      break;
  }
  return options;
}

size_t GetMonthSkeletonCount(const std::string& skeleton,
                             DateTimeFormatterOptions::ItemLength length,
                             SkeletonOptions options) {
  if (!options.has_month) {
    return 0;
  }

  // If there is no day or year, a separated logic is applied.
  if (!options.has_day && !options.has_year) {
    switch (length) {
      case DateTimeFormatterOptions::ItemLength::kNone:
      case DateTimeFormatterOptions::ItemLength::kShort:
        return 1;
      case DateTimeFormatterOptions::ItemLength::kMedium:
        return 3;
      case DateTimeFormatterOptions::ItemLength::kLong:
        return 4;
    }
  }

  size_t month_count = std::ranges::count(skeleton, 'M');
  if (length == DateTimeFormatterOptions::ItemLength::kLong) {
    month_count = std::max(month_count, static_cast<size_t>(3));
  }
  return month_count;
}

size_t GetYearSkeletonCount(const std::string& skeleton,
                            DateTimeFormatterOptions::ItemLength length,
                            SkeletonOptions options) {
  if (!options.has_year) {
    return 0;
  }
  // If there is no day or year, a separated logic is applied.
  if (!options.has_day && !options.has_month) {
    switch (length) {
      case DateTimeFormatterOptions::ItemLength::kNone:
      case DateTimeFormatterOptions::ItemLength::kLong:
      case DateTimeFormatterOptions::ItemLength::kMedium:
        return 1;
      case DateTimeFormatterOptions::ItemLength::kShort:
        return 2;
    }
  }

  return std::ranges::count(skeleton, 'y');
}

size_t GetDaySkeletonCount(const std::string& skeleton,
                           DateTimeFormatterOptions::ItemLength length,
                           SkeletonOptions options) {
  if (!options.has_day) {
    return 0;
  }

  // If there is no day or year, a separated logic is applied.
  if (!options.has_year && !options.has_month) {
    switch (length) {
      case DateTimeFormatterOptions::ItemLength::kShort:
      case DateTimeFormatterOptions::ItemLength::kMedium:
        return 1;
      case DateTimeFormatterOptions::ItemLength::kNone:
      case DateTimeFormatterOptions::ItemLength::kLong:
        return 2;
    }
  }

  return std::ranges::count(skeleton, 'd');
}

// Takes a complete skeleton and returns a new one containing only the fields
// that must be present.
icu::UnicodeString GetFormattedSkeleton(
    const icu::UnicodeString& complete_skeleton,
    DateTimeFormatterOptions options) {
  SkeletonOptions skeleton_options =
      GetSkeletonOptions(options.format_identifier);
  std::string skeleton =
      base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(complete_skeleton));

  size_t year_count =
      GetYearSkeletonCount(skeleton, options.length, skeleton_options);
  size_t month_count =
      GetMonthSkeletonCount(skeleton, options.length, skeleton_options);
  size_t day_count =
      GetDaySkeletonCount(skeleton, options.length, skeleton_options);
  size_t weekday_count = std::ranges::count(skeleton, 'E');

  icu::UnicodeString output_skeleton;
  if (skeleton_options.has_year) {
    output_skeleton.append(std::u16string(year_count, 'y'));
  }
  if (skeleton_options.has_month) {
    output_skeleton.append(std::u16string(month_count, 'M'));
  }
  if (skeleton_options.has_day) {
    output_skeleton.append(std::u16string(day_count, 'd'));
  }
  if (skeleton_options.has_weekday) {
    // Max between 1u and weekday_count is used to force its presence.
    output_skeleton.append(
        std::u16string(std::max<size_t>(weekday_count, 1u), 'E'));
  }
  if (options.year_style == DateTimeFormatterOptions::YearStyle::kWithEra) {
    output_skeleton += "G";
  }

  // Early return as from here, only time-formatting skeleton is built.
  if (skeleton_options.has_time) {
    size_t hour_12_count = std::ranges::count(skeleton, 'h');
    size_t hour_24_count = std::ranges::count(skeleton, 'H');
    size_t minute_count = std::ranges::count(skeleton, 'm');
    size_t second_count = std::ranges::count(skeleton, 's');
    size_t subsecond_count = std::ranges::count(skeleton, 'S');
    // Hour
    if (hour_12_count) {
      output_skeleton.append(std::u16string(hour_12_count, 'h'));
    }
    if (hour_24_count) {
      output_skeleton.append(std::u16string(hour_24_count, 'H'));
    }

    if (options.time_precision !=
        DateTimeFormatterOptions::TimePrecision::kHour) {
      output_skeleton.append(std::u16string(minute_count, 'm'));
      if (options.time_precision !=
          DateTimeFormatterOptions::TimePrecision::kMinute) {
        output_skeleton.append(
            std::u16string(std::max<size_t>(second_count, 1), 's'));
        if (options.time_precision !=
            DateTimeFormatterOptions::TimePrecision::kSecond) {
          // If no precision is informed, return the fields that were present in
          // the skeleton generated by ICU.
          if (options.time_precision ==
              DateTimeFormatterOptions::TimePrecision::kNone) {
            output_skeleton.append(std::u16string(subsecond_count, 'S'));
          } else {
            output_skeleton.append(u"SS");
            if (options.time_precision !=
                DateTimeFormatterOptions::TimePrecision::kSubsecond_2) {
              output_skeleton += "S";
              if (options.time_precision !=
                  DateTimeFormatterOptions::TimePrecision::kSubsecond_3) {
                output_skeleton += "S";
              }
            }
          }
        }
      }
    }
  }

  if (options.time_zone_style !=
      DateTimeFormatterOptions::TimeZoneStyle::kNone) {
    switch (options.time_zone_style) {
      case DateTimeFormatterOptions::TimeZoneStyle::kShortSpecific:
        output_skeleton += "z";
        break;
      case DateTimeFormatterOptions::TimeZoneStyle::kLongSpecific:
        output_skeleton += "zzzz";
        break;
      case DateTimeFormatterOptions::TimeZoneStyle::kShortGeneric:
        output_skeleton += "v";
        break;
      case DateTimeFormatterOptions::TimeZoneStyle::kLongGeneric:
        output_skeleton += "vvvv";
        break;
      case DateTimeFormatterOptions::TimeZoneStyle::kNone:
        NOTREACHED();
    }
  }

  return output_skeleton;
}

// Getting the best pattern involves a couple of steps.
// - Get an initial pattern for the preferred length using
// `GetPatternForLength`.
// - Get an initial skeleton for the initial pattern.
// - Remove not-wanted fields from the initial skeleton.
// - Apply some adhoc fixes to the skeleton to obtain a formatted skeleton.
// - Use DateTimePatternGenerator::getBestPattern to obtain the best pattern for
// the formatted skeleton.
icu::UnicodeString GetBestPattern(const icu::Locale& locale,
                                  DateTimeFormatterOptions options) {
  icu::UnicodeString icu_pattern = GetPatternForLength(locale, options.length);
  if (icu_pattern.isEmpty()) {
    return "";
  }
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(locale, status));
  if (!U_SUCCESS(status)) {
    return "";
  }
  status = U_ZERO_ERROR;
  icu::UnicodeString complete_skeleton =
      generator->getSkeleton(icu_pattern, status);
  if (U_FAILURE(status)) {
    return "";
  }

  icu::UnicodeString formatted_skeleton =
      GetFormattedSkeleton(complete_skeleton, options);
  icu::UnicodeString best_pattern =
      generator->getBestPattern(formatted_skeleton, status);
  return best_pattern;
}

icu::Locale GetLocaleWithHourClockType(
    const icu::Locale& locale_arg,
    std::optional<base::HourClockType> hour_clock_type) {
  icu::Locale locale = locale_arg;
  if (!hour_clock_type) {
    return locale;
  }

  UErrorCode status = U_ZERO_ERROR;
  locale.setUnicodeKeywordValue(
      "hc", (*hour_clock_type == base::k12HourClock) ? "h12" : "h23", status);
  return locale;
}

std::u16string FormatWithLocale(const base::Time& time,
                                const DateTimeFormatterOptions& options,
                                const icu::Locale& locale_arg) {
  icu::Locale locale =
      GetLocaleWithHourClockType(locale_arg, options.hour_clock_type);

  if (options.format_identifier ==
      DateTimeFormatterOptions::FormatIdentifier::kNone) {
    return FormatWithLocale(time, datetime_options::YMD::Medium(), locale);
  }

  icu::UnicodeString best_pattern = GetBestPattern(locale, options);
  auto formatter = CreateSimpleDateFormatter(best_pattern, locale);
  if (options.time_zone) {
    std::unique_ptr<icu::TimeZone> icu_tz(icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(options.time_zone->GetID())));
    formatter.setTimeZone(*icu_tz);
  }
  return DateTimeFormat(formatter, time, options.am_pm_clock_type);
}

}  // namespace

// DateTime Formatting
std::u16string IcuBridge::DateTimeFormatter::Format(
    const base::Time& time,
    const DateTimeFormatterOptions& options) const {
  return FormatWithLocale(time, options, icu::Locale::getDefault());
}

std::u16string IcuBridge::DateTimeFormatter::Format(
    const base::Time& time,
    const LanguageTag& locale,
    const DateTimeFormatterOptions& options) const {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale = icu::Locale::forLanguageTag(
      std::string(locale.tag_string()).c_str(), status);
  if (U_FAILURE(status) || icu_locale.isBogus()) {
    icu_locale = icu::Locale::getDefault();
  }
  return FormatWithLocale(time, options, icu_locale);
}

}  // namespace base::i18n
