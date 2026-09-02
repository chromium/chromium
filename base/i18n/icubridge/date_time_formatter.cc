// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/date_time_formatter.h"

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/bcp47_extensions.h"
#include "base/i18n/icu4c_tag_converter.h"  // nogncheck
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/icu_bridge_helpers.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
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
UDate ToUDate(base::Time time) {
  return time.InMillisecondsFSinceUnixEpoch();
}

std::u16string DateTimeFormat(
    const icu::DateFormat& formatter,
    base::Time time,
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
    const LanguageTag& locale) {
  UErrorCode status = U_ZERO_ERROR;
  // Then, format the time using the desired pattern.
  icu::SimpleDateFormat formatter(
      pattern, IcuLocaleConverter::GetInstance().FromLanguageTag(locale),
      status);
  if (U_SUCCESS(status)) {
    return formatter;
  }

  // Fallback if the generated pattern failed (e.g. due to unsupported fields
  // in some locales on limited ICU data platforms).
  status = U_ZERO_ERROR;
  return icu::SimpleDateFormat(
      icu::UnicodeString("yyyy-MM-dd HH:mm:ss"),
      IcuLocaleConverter::GetInstance().FromLanguageTag(locale), status);
}

icu::DateFormat::EStyle ToIcuStyle(DateTimeFormatterOptions::ItemLength length,
                                   bool has_weekday) {
  switch (length) {
    case DateTimeFormatterOptions::ItemLength::kLong:
      return has_weekday ? icu::DateFormat::kFull : icu::DateFormat::kLong;
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
    const LanguageTag& locale,
    DateTimeFormatterOptions::ItemLength length,
    bool has_time,
    bool has_weekday) {
  icu::DateFormat::EStyle style_length = ToIcuStyle(length, has_weekday);
  std::unique_ptr<icu::DateFormat> fmt =
      has_time
          ? std::unique_ptr<icu::DateFormat>(
                icu::DateFormat::createDateTimeInstance(
                    style_length, style_length,
                    IcuLocaleConverter::GetInstance().FromLanguageTag(locale)))
          : std::unique_ptr<icu::DateFormat>(
                icu::DateFormat::createDateInstance(
                    style_length,
                    IcuLocaleConverter::GetInstance().FromLanguageTag(locale)));

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

std::u16string GetYearSkeleton(const std::string& skeleton,
                               DateTimeFormatterOptions::ItemLength length,
                               SkeletonOptions options) {
  if (!options.has_year) {
    return u"";
  }
  // If there is no day or year, a separated logic is applied.
  if (!options.has_day && !options.has_month) {
    switch (length) {
      case DateTimeFormatterOptions::ItemLength::kNone:
      case DateTimeFormatterOptions::ItemLength::kLong:
      case DateTimeFormatterOptions::ItemLength::kMedium:
        return u"y";
      case DateTimeFormatterOptions::ItemLength::kShort:
        return u"yy";
    }
  }

  return std::u16string(std::ranges::count(skeleton, 'y'), 'y');
}

std::u16string GetMonthSkeleton(const icu::UnicodeString& icu_initial_pattern,
                                const std::string& skeleton,
                                DateTimeFormatterOptions::ItemLength length,
                                SkeletonOptions options) {
  if (!options.has_month) {
    return u"";
  }

  // If there is no day or year, a separated logic is applied.
  if (!options.has_day && !options.has_year) {
    switch (length) {
      case DateTimeFormatterOptions::ItemLength::kNone:
      case DateTimeFormatterOptions::ItemLength::kShort:
        return u"M";
      case DateTimeFormatterOptions::ItemLength::kMedium:
        return u"MMM";
      case DateTimeFormatterOptions::ItemLength::kLong:
        return u"MMMM";
    }
  }

  size_t month_count = std::ranges::count(skeleton, 'M');
  if (length == DateTimeFormatterOptions::ItemLength::kLong) {
    month_count = std::max(month_count, static_cast<size_t>(4));
  }
  // If there is weekday in the initial skeleton and the length is medium, the
  // month (M) count is set to a minimum of 3.
  if (length == DateTimeFormatterOptions::ItemLength::kMedium &&
      month_count < 3) {
    // This is a special character that if present in the initial pattern, we
    // can force it to appear again by increasing the number of 'M' symbols.
    if (icu_initial_pattern.indexOf(u'年') != -1) {
      month_count = 3;
    }
  }
  return std::u16string(month_count, 'M');
}

std::u16string GetDaySkeleton(const std::string& skeleton,
                              DateTimeFormatterOptions::ItemLength length,
                              SkeletonOptions options) {
  if (!options.has_day) {
    return u"";
  }

  // If there is no day or year, a separated logic is applied.
  if (!options.has_year && !options.has_month) {
    switch (length) {
      case DateTimeFormatterOptions::ItemLength::kShort:
      case DateTimeFormatterOptions::ItemLength::kMedium:
        return u"d";
      case DateTimeFormatterOptions::ItemLength::kNone:
      case DateTimeFormatterOptions::ItemLength::kLong:
        return u"dd";
    }
  }

  return std::u16string(std::ranges::count(skeleton, 'd'), 'd');
}

std::u16string GetWeekDaySkeleton(const std::string& skeleton,
                                  DateTimeFormatterOptions options,
                                  SkeletonOptions skeleton_options) {
  if (!skeleton_options.has_weekday) {
    return u"";
  }
  if (options.format_identifier ==
          DateTimeFormatterOptions::FormatIdentifier::kE &&
      options.length == DateTimeFormatterOptions::ItemLength::kShort) {
    return u"EEEEE";
  }
  char weekday_symbol = 'E';
  size_t e_count = std::ranges::count(skeleton, 'E');
  size_t c_count = std::ranges::count(skeleton, 'c');
  if (c_count > e_count) {
    weekday_symbol = 'c';
  }
  size_t weekday_count = std::max(size_t{1u}, std::max(e_count, c_count));
  return std::u16string(weekday_count, weekday_symbol);
}

std::u16string GetHourSkeleton(const std::string& skeleton,
                               DateTimeFormatterOptions options,
                               const LanguageTag& locale) {
  size_t hour_h_count = std::ranges::count(skeleton, 'h');
  size_t hour_H_count = std::ranges::count(skeleton, 'H');
  size_t hour_K_count = std::ranges::count(skeleton, 'K');
  size_t hour_k_count = std::ranges::count(skeleton, 'k');

  std::u16string output_hour_skeleton;
  size_t total_hour_count =
      hour_h_count + hour_H_count + hour_K_count + hour_k_count;
  if (options.hour_clock_type == base::k12HourClock) {
    // Only Japanese ("ja") natively prefers the 'K' cycle.
    bool prefers_K = (locale.language_subtag() == "ja");
    char16_t symbol = (hour_K_count > 0 || prefers_K) ? 'K' : 'h';
    output_hour_skeleton.append(
        std::u16string(std::max<size_t>(total_hour_count, 1), symbol));
  } else if (options.hour_clock_type == base::k24HourClock) {
    char16_t symbol = (hour_k_count > 0) ? 'k' : 'H';
    output_hour_skeleton.append(
        std::u16string(std::max<size_t>(total_hour_count, 1), symbol));
  } else {
    if (hour_h_count) {
      output_hour_skeleton.append(std::u16string(hour_h_count, 'h'));
    }
    if (hour_H_count) {
      output_hour_skeleton.append(std::u16string(hour_H_count, 'H'));
    }
    if (hour_K_count) {
      output_hour_skeleton.append(std::u16string(hour_K_count, 'K'));
    }
    if (hour_k_count) {
      output_hour_skeleton.append(std::u16string(hour_k_count, 'k'));
    }
  }
  return output_hour_skeleton;
}

// Takes a complete skeleton and returns a new one containing only the fields
// that must be present.
icu::UnicodeString GetFormattedSkeleton(
    const icu::UnicodeString& icu_initial_pattern,
    const icu::UnicodeString& complete_skeleton,
    const SkeletonOptions& skeleton_options,
    DateTimeFormatterOptions options,
    const LanguageTag& locale) {
  std::string skeleton =
      base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(complete_skeleton));

  icu::UnicodeString output_skeleton;
  if (skeleton_options.has_year) {
    output_skeleton.append(
        GetYearSkeleton(skeleton, options.length, skeleton_options));
  }
  if (skeleton_options.has_month) {
    output_skeleton.append(GetMonthSkeleton(icu_initial_pattern, skeleton,
                                            options.length, skeleton_options));
  }
  if (skeleton_options.has_day) {
    output_skeleton.append(
        GetDaySkeleton(skeleton, options.length, skeleton_options));
  }
  if (skeleton_options.has_weekday) {
    // Max between 1u and weekday_count is used to force its presence.
    output_skeleton.append(
        GetWeekDaySkeleton(skeleton, options, skeleton_options));
  }
  if (options.year_style == DateTimeFormatterOptions::YearStyle::kWithEra) {
    output_skeleton += "G";
  }

  // Early return as from here, only time-formatting skeleton is built.
  if (skeleton_options.has_time) {
    output_skeleton.append(GetHourSkeleton(skeleton, options, locale));

    size_t minute_count = std::ranges::count(skeleton, 'm');
    size_t second_count = std::ranges::count(skeleton, 's');
    size_t subsecond_count = std::ranges::count(skeleton, 'S');

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
icu::UnicodeString GetBestPattern(const LanguageTag& locale,
                                  DateTimeFormatterOptions options) {
  SkeletonOptions skeleton_options =
      GetSkeletonOptions(options.format_identifier);
  icu::UnicodeString icu_pattern =
      GetPatternForLength(locale, options.length, skeleton_options.has_time,
                          skeleton_options.has_weekday);

  if (!skeleton_options.has_time && skeleton_options.has_day &&
      skeleton_options.has_month && skeleton_options.has_year) {
    const bool has_weekday_in_icu_pattern =
        icu_pattern.indexOf('E') != -1 || icu_pattern.indexOf('c') != -1;
    // There could happen that the default pattern for a date might have a 'G'
    // (Era) field.
    if (options.year_style != DateTimeFormatterOptions::YearStyle::kWithEra) {
      icu_pattern.findAndReplace("G", "");
      // Trims extra spaces that could happen after removing "G".
      icu_pattern.findAndReplace("  ", " ");
    }

    const bool has_era = icu_pattern.indexOf('G') != -1;
    const bool has_era_in_options =
        options.year_style == DateTimeFormatterOptions::YearStyle::kWithEra;
    if (skeleton_options.has_weekday == has_weekday_in_icu_pattern &&
        has_era == has_era_in_options) {
      return icu_pattern;
    }
  }

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(
          IcuLocaleConverter::GetInstance().FromLanguageTag(locale), status));
  if (!U_SUCCESS(status)) {
    return "";
  }
  status = U_ZERO_ERROR;
  icu::UnicodeString complete_skeleton =
      generator->getSkeleton(icu_pattern, status);
  if (U_FAILURE(status)) {
    return "";
  }

  icu::UnicodeString formatted_skeleton = GetFormattedSkeleton(
      icu_pattern, complete_skeleton, skeleton_options, options, locale);
  icu::UnicodeString best_pattern =
      generator->getBestPattern(formatted_skeleton, status);

  // Workaround for the "bg" locale because ICU removes the 'ч' from its end
  // when the timezone (zzzz) is not asked for.
  if (skeleton_options.has_time && icu_pattern.endsWith(u"'ч'. zzzz") &&
      best_pattern.indexOf(u"'ч'") == -1) {
    best_pattern.append(u" ч.");
  }

  return best_pattern;
}

LanguageTag GetLocaleWithHourClockType(
    const LanguageTag& locale,
    std::optional<base::HourClockType> hour_clock_type) {
  if (!hour_clock_type) {
    return locale;
  }

  std::string hour_unicode_keyword_value;
  if (*hour_clock_type == base::k12HourClock) {
    // Only Japanese ("ja") natively prefers the h11 (K) cycle.
    bool prefers_K = (locale.language_subtag() == "ja");
    hour_unicode_keyword_value = prefers_K ? "h11" : "h12";
  } else {
    // No locales natively prefer the h24 (k) cycle; they all use h23 (H).
    hour_unicode_keyword_value = "h23";
  }
  std::optional<UnicodeExtension> u_ext =
      UnicodeExtension::FromString("u-hc-" + hour_unicode_keyword_value);
  CHECK(u_ext);
  return locale.WithExtension(*u_ext);
}

std::u16string FormatWithLocale(base::Time time,
                                const DateTimeFormatterOptions& options,
                                const LanguageTag& locale_arg) {
  LanguageTag locale =
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
    base::Time time,
    const DateTimeFormatterOptions& options) const {
  LanguageTag default_tag = IcuLocaleConverter::GetInstance().ToLanguageTag(
      icu::Locale::getDefault());
  return FormatWithLocale(time, options, default_tag);
}

std::u16string IcuBridge::DateTimeFormatter::Format(
    base::Time time,
    const LanguageTag& locale,
    const DateTimeFormatterOptions& options) const {
  return FormatWithLocale(time, options, locale);
}

base::HourClockType IcuBridge::DateTimeFormatter::GetHourClockType() const {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString pattern =
      generator->getBestPattern(icu::UnicodeString("j"), status);
  DCHECK(U_SUCCESS(status));
  return pattern.indexOf('a') == -1 ? base::k24HourClock : base::k12HourClock;
}

}  // namespace base::i18n
