// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/date_time_formatter.h"

#include <stdint.h>

#include "base/check.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/icu_bridge_helpers.h"
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
    std::string_view pattern,
    const icu::Locale& locale = icu::Locale::getDefault()) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString generated_pattern(pattern.data(),
                                       static_cast<int32_t>(pattern.length()));

  // Generate a locale-dependent format pattern. The generator will take
  // care of locale-dependent formatting issues like which separator to
  // use (some locales use '.' instead of ':'), and where to put the am/pm
  // marker.
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(locale, status));
  DCHECK(U_SUCCESS(status));
  generated_pattern = generator->getBestPattern(generated_pattern, status);
  DCHECK(U_SUCCESS(status));

  // Then, format the time using the desired pattern.
  icu::SimpleDateFormat formatter(generated_pattern, locale, status);
  DCHECK(U_SUCCESS(status));

  return formatter;
}

icu::DateFormat::EStyle ToIcuStyle(
    DateTimeFormatterOptions::ItemLength length) {
  switch (length) {
    case DateTimeFormatterOptions::ItemLength::kFull:
      return icu::DateFormat::kFull;
    case DateTimeFormatterOptions::ItemLength::kLong:
      return icu::DateFormat::kLong;
    case DateTimeFormatterOptions::ItemLength::kMedium:
      return icu::DateFormat::kMedium;
    case DateTimeFormatterOptions::ItemLength::kShort:
      return icu::DateFormat::kShort;
    case DateTimeFormatterOptions::ItemLength::kNone:
      return icu::DateFormat::kNone;
  }
  NOTREACHED();
}

icu::Locale GetLocaleWithHourClockType(
    std::optional<base::HourClockType> hour_clock_type) {
  icu::Locale locale = icu::Locale::getDefault();
  if (hour_clock_type) {
    UErrorCode status = U_ZERO_ERROR;
    locale.setUnicodeKeywordValue(
        "hc", (*hour_clock_type == base::k12HourClock) ? "h12" : "h23", status);
  }
  return locale;
}

}  // namespace

// DateTime Formatting
std::u16string IcuBridge::DateTimeFormatter::Format(
    const base::Time& time,
    const DateTimeFormatterOptions& options) const {
  icu::Locale locale = GetLocaleWithHourClockType(options.hour_clock_type);

  if (options.format_identifier ==
      DateTimeFormatterOptions::FormatIdentifier::kNone) {
    if (options.length != DateTimeFormatterOptions::ItemLength::kNone) {
      std::unique_ptr<icu::DateFormat> formatter(
          icu::DateFormat::createDateTimeInstance(
              ToIcuStyle(options.length), ToIcuStyle(options.length), locale));
      DCHECK(formatter);
      return DateTimeFormat(*formatter, time, options.am_pm_clock_type);
    }
  }

  std::string pattern;
  switch (options.format_identifier) {
    case DateTimeFormatterOptions::FormatIdentifier::kD:
      pattern = "d";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kDE:
      pattern = "Ed";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kDET:
      pattern = "Ed";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kDT:
      pattern = "d";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kE:
      pattern = "E";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kET:
      pattern = "E";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kM:
      pattern = "MMMM";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMD:
      pattern = "MMMMd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMDE:
      pattern = "EMMMMd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMDET:
      pattern = "EMMMMd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kMDT:
      pattern = "MMMMd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kT:
      pattern = "";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kY:
      pattern = "y";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYM:
      pattern = "yMMMM";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMD:
      pattern = "yMd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMDE:
      pattern = "yMEd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMDET:
      pattern = "yMEd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kYMDT:
      pattern = "yMd";
      break;
    case DateTimeFormatterOptions::FormatIdentifier::kNone:
      break;
  }

  if (options.year_style == DateTimeFormatterOptions::YearStyle::kWithEra) {
    pattern += "G";
  }

  bool has_time = false;
  switch (options.format_identifier) {
    case DateTimeFormatterOptions::FormatIdentifier::kDET:
    case DateTimeFormatterOptions::FormatIdentifier::kDT:
    case DateTimeFormatterOptions::FormatIdentifier::kET:
    case DateTimeFormatterOptions::FormatIdentifier::kMDET:
    case DateTimeFormatterOptions::FormatIdentifier::kMDT:
    case DateTimeFormatterOptions::FormatIdentifier::kT:
    case DateTimeFormatterOptions::FormatIdentifier::kYMDET:
    case DateTimeFormatterOptions::FormatIdentifier::kYMDT:
      has_time = true;
      break;
    default:
      break;
  }

  DateTimeFormatterOptions::TimePrecision precision = options.time_precision;
  if (has_time && precision == DateTimeFormatterOptions::TimePrecision::kNone) {
    precision = DateTimeFormatterOptions::TimePrecision::kSecond;
  }

  switch (precision) {
    case DateTimeFormatterOptions::TimePrecision::kHour:
      pattern += "j";
      break;
    case DateTimeFormatterOptions::TimePrecision::kMinute:
      pattern += "jm";
      break;
    case DateTimeFormatterOptions::TimePrecision::kSecond:
      pattern += "jms";
      break;
    case DateTimeFormatterOptions::TimePrecision::kSubsecond_2:
      pattern += "jmsSS";
      break;
    case DateTimeFormatterOptions::TimePrecision::kSubsecond_3:
      pattern += "jmsSSS";
      break;
    case DateTimeFormatterOptions::TimePrecision::kSubsecond_4:
      pattern += "jmsSSSS";
      break;
    case DateTimeFormatterOptions::TimePrecision::kNone:
      break;
  }

  if (pattern.empty()) {
    return std::u16string();
  }

  return DateTimeFormat(CreateSimpleDateFormatter(pattern, locale), time,
                        options.am_pm_clock_type);
}

}  // namespace base::i18n
