// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_DATE_TIME_FORMATTER_H_
#define BASE_I18N_ICUBRIDGE_DATE_TIME_FORMATTER_H_

#include <string>
#include <string_view>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/time_formatting_types.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"

namespace base::i18n {

class IcuBridge;
struct DateTimeFormatterOptions;

// DateTimeFormatter provides a set of helper functions for formatting dates and
// times using ICU. It handles locale-specific formatting and provides
// common patterns used across the codebase.
//
// Usage example:
//
// #include "base/i18n/icubridge/icu_bridge.h"
// #include "base/i18n/icubridge/date_time_formatter.h"
//
// // Simple usage with predefined shorthands:
// std::u16string simple = base::i18n::IcuBridge::GetInstance()
//     .date_time_formatter()
//     .Format(base::Time::Now(), base::i18n::datetime_options::YMDT::Short());
//
// // Advanced usage with fluent builder:
// std::u16string advanced = base::i18n::IcuBridge::GetInstance()
//     .date_time_formatter()
//     .Format(base::Time::Now(),
//             base::i18n::datetime_options::YMDT::Medium()
//                 .with_year_style(DateTimeFormatterOptions::YearStyle::kNoEra)
//                 .with_time_precision(
//                     DateTimeFormatterOptions::TimePrecision::kSecond));
class BASE_I18N_EXPORT IcuBridge::DateTimeFormatter {
 public:
  // Formats date and time according to the provided options.
  // The formatting is locale-aware and uses the default locale set for the
  // process.
  std::u16string Format(const base::Time& time,
                        const DateTimeFormatterOptions& options) const;

  explicit DateTimeFormatter(base::PassKey<IcuBridge>) {}
};

// Options for date and time formatting.
//
// These options define which components (date, time, or both) and in what
// format they should be presented.
//
// NOTE: These options MUST be constructed using the predefined shorthand
// functions in the `base::i18n::datetime_options` namespace (e.g.,
// `datetime_options::YMDT::Short()`). Direct instantiation or usage of the
// Builder class outside that namespace is discouraged.
struct BASE_I18N_EXPORT DateTimeFormatterOptions {
  // Predefined lengths for date and time components.
  enum class ItemLength {
    kNone,
    kFull,    // e.g., "Monday, May 25, 2026" / "10:30:00 AM Pacific Daylight
              // Time"
    kLong,    // e.g., "May 25, 2026" / "10:30:00 AM PDT"
    kMedium,  // e.g., "May 25, 2026" / "10:30:00 AM"
    kShort,   // e.g., "5/25/26" / "10:30 AM"
  };

  // Specific format identifiers for targeted components.
  // The naming convention for these identifiers uses the following characters:
  // - Y: Year
  // - M: Month
  // - D: Day of month
  // - E: Day of week (Weekday)
  // - T: Time
  enum class FormatIdentifier {
    kNone,
    kD,      // Day of month (standalone)
    kDE,     // Day of month and weekday
    kDET,    // Day of month and weekday with time
    kDT,     // Day of month with time
    kE,      // Weekday (standalone)
    kET,     // Weekday with time
    kM,      // Month (standalone)
    kMD,     // Month and day
    kMDE,    // Month, day, and weekday
    kMDET,   // Month, day, and weekday with time
    kMDT,    // Month and day with time
    kT,      // Time (standalone)
    kY,      // Year (standalone)
    kYM,     // Year and month
    kYMD,    // Year, month, and day
    kYMDE,   // Year, month, day, and weekday
    kYMDET,  // Year, month, day, and weekday with time
    kYMDT,   // Year, month, and day with time
  };
  // Year style options.
  enum class YearStyle {
    kAuto,
    kFull,     // e.g., "2026"
    kWithEra,  // e.g., "2026 AD"
    kNoEra     // e.g., "2026" (force no era)
  };
  // Time precision options.
  enum class TimePrecision {
    kNone,
    kHour,            // e.g., "10 AM"
    kMinute,          // e.g., "10:30 AM"
    kSecond,          // e.g., "10:30:00 AM"
    kSubsecond_2,     // e.g., "10:30:00.00 AM"
    kSubsecond_3,     // e.g., "10:30:00.000 AM"
    kSubsecond_4,     // e.g., "10:30:00.0000 AM"
  };

  ItemLength length = ItemLength::kNone;
  FormatIdentifier format_identifier = FormatIdentifier::kNone;
  YearStyle year_style = YearStyle::kAuto;
  TimePrecision time_precision = TimePrecision::kNone;
  std::optional<base::HourClockType> hour_clock_type;
  std::optional<base::AmPmClockType> am_pm_clock_type;

  // Internal builder class for creating DateTimeFormatterOptions.
  //
  // Use the predefined shorthand functions in `base::i18n::datetime_options`
  // instead of using this class directly.
  template <FormatIdentifier component_type_value,
            ItemLength length = ItemLength::kNone>
  class BASE_I18N_EXPORT Builder;
};
template <DateTimeFormatterOptions::FormatIdentifier component_type_value,
          DateTimeFormatterOptions::ItemLength length>
class BASE_I18N_EXPORT DateTimeFormatterOptions::Builder {
 public:
  // Returns the constructed DateTimeFormatterOptions object.
  DateTimeFormatterOptions Get() const {
    static_assert(length != DateTimeFormatterOptions::ItemLength::kNone,
                  "A length must be specified (Short(), Medium(), Long(), or "
                  "Full()) before converting to Options.");
    DateTimeFormatterOptions options;
    options.format_identifier = component_type_value;
    options.year_style = year_style_;
    options.time_precision = time_precision_;
    options.hour_clock_type = hour_clock_type_;
    options.am_pm_clock_type = am_pm_clock_type_;
    options.length = length;
    return options;
  }

  // Implicit conversion to DateTimeFormatterOptions for convenience.
  operator DateTimeFormatterOptions() const { return Get(); }

  // Sets the year style for the formatter.
  auto& with_year_style(YearStyle year_style_arg) {
    year_style_ = year_style_arg;
    return *this;
  }
  // Sets the time precision for the formatter.
  auto& with_time_precision(TimePrecision time_precision_arg) {
    time_precision_ = time_precision_arg;
    return *this;
  }
  // Sets the hour clock type for the formatter.
  auto& with_hour_clock_type(base::HourClockType hour_clock_type_arg) {
    hour_clock_type_ = hour_clock_type_arg;
    return *this;
  }
  // Sets the AM/PM clock type for the formatter.
  auto& with_am_pm_clock_type(base::AmPmClockType am_pm_clock_type_arg) {
    am_pm_clock_type_ = am_pm_clock_type_arg;
    return *this;
  }

  // Predefined builder configurations that set the length.
  // These are the typical entry points for the fluent builder.

  // e.g., "5/25/26, 10:30 AM"
  static auto Short() {
    return Builder<component_type_value, ItemLength::kShort>();
  }
  // e.g., "May 25, 2026, 10:30:00 AM"
  static auto Medium() {
    return Builder<component_type_value, ItemLength::kMedium>();
  }
  // e.g., "May 25, 2026, 10:30:00 AM PDT"
  static auto Long() {
    return Builder<component_type_value, ItemLength::kLong>();
  }
  // e.g., "Monday, May 25, 2026, 10:30:00 AM Pacific Daylight Time"
  static auto Full() {
    return Builder<component_type_value, ItemLength::kFull>();
  }

 private:
  YearStyle year_style_ = YearStyle::kAuto;
  TimePrecision time_precision_ = TimePrecision::kNone;
  std::optional<base::HourClockType> hour_clock_type_;
  std::optional<base::AmPmClockType> am_pm_clock_type_;
};

// Namespace containing the primary entry points for date/time formatting.
//
// These shorthand builders provide a fluent interface to specify the components
// (e.g., YMDT for Year, Month, Day, Time) and their length (Short, Medium,
// Long, Full).
//
// All `DateTimeFormatterOptions` should be initiated from this namespace.
//
// Example: datetime_options::YMDT::Short() -> Year, Month, Day, Time (Short)
namespace datetime_options {
// Day of month (standalone)
using D = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kD>;
// Day of month and weekday
using DE = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kDE>;
// Day of month and weekday with time
using DET = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kDET>;
// Day of month with time
using DT = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kDT>;
// Weekday (standalone)
using E = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kE>;
// Weekday with time
using ET = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kET>;
// Month (standalone)
using M = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kM>;
// Month and day
using MD = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kMD>;
// Month, day, and weekday
using MDE = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kMDE>;
// Month, day, and weekday with time
using MDET = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kMDET>;
// Month and day with time
using MDT = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kMDT>;
// Time (standalone)
using T = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kT>;
// Year (standalone)
using Y = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kY>;
// Year and month
using YM = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kYM>;
// Year, month, and day
using YMD = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kYMD>;
// Year, month, day, and weekday
using YMDE = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kYMDE>;
// Year, month, day, and weekday with time
using YMDET = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kYMDET>;
// Year, month, and day with time
using YMDT = DateTimeFormatterOptions::Builder<
    DateTimeFormatterOptions::FormatIdentifier::kYMDT>;

}  // namespace datetime_options

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_DATE_TIME_FORMATTER_H_
