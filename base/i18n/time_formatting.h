// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic time formatting methods.  Most methods format based on the current
// locale. *TimeFormatWithPattern() are special; see comments there.

#ifndef BASE_I18N_TIME_FORMATTING_H_
#define BASE_I18N_TIME_FORMATTING_H_

#include <string>
#include <string_view>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/time_formatting_types.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/uversion.h"

U_NAMESPACE_BEGIN
class TimeZone;
U_NAMESPACE_END

namespace base {

class Time;
class TimeDelta;

namespace i18n {
class TimeZone;
}  // namespace i18n

// Returns the time of day, e.g., "3:07 PM".
BASE_I18N_EXPORT std::u16string TimeFormatTimeOfDay(Time time);

// Returns the time of day in 24-hour clock format with millisecond accuracy,
// e.g., "15:07:30.568"
BASE_I18N_EXPORT std::u16string TimeFormatTimeOfDayWithMilliseconds(Time time);

// Returns the time of day in the specified hour clock type. e.g.
// "3:07 PM" (type == k12HourClock, ampm == kKeepAmPm).
// "3:07"    (type == k12HourClock, ampm == kDropAmPm).
// "15:07"   (type == k24HourClock).
BASE_I18N_EXPORT std::u16string TimeFormatTimeOfDayWithHourClockType(
    Time time,
    HourClockType type,
    AmPmClockType ampm);

// Returns a shortened date, e.g. "Nov 7, 2007"
BASE_I18N_EXPORT std::u16string TimeFormatShortDate(Time time);

// Returns a numeric date such as 12/13/52.
BASE_I18N_EXPORT std::u16string TimeFormatShortDateNumeric(Time time);

// Returns a numeric date and time such as "12/13/52 2:44:30 PM".
BASE_I18N_EXPORT std::u16string TimeFormatShortDateAndTime(Time time);

#if BUILDFLAG(IS_CHROMEOS)
// Returns a month and year, e.g. "November 2007" for the specified time zone.
BASE_I18N_EXPORT std::u16string TimeFormatMonthAndYearForTimeZone(
    Time time,
    const icu::TimeZone* time_zone);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns a month and year, e.g. "November 2007"
BASE_I18N_EXPORT std::u16string TimeFormatMonthAndYear(Time time);

// Returns a numeric date and time with time zone such as
// "12/13/52 2:44:30 PM PST".
BASE_I18N_EXPORT std::u16string TimeFormatShortDateAndTimeWithTimeZone(
    Time time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
BASE_I18N_EXPORT std::u16string TimeFormatFriendlyDateAndTime(Time time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008".
BASE_I18N_EXPORT std::u16string TimeFormatFriendlyDate(Time time);

// Formats a time compliant to ISO 8601 in UTC, e.g. "2020-12-31T23:59:59.999Z".
BASE_I18N_EXPORT std::string TimeFormatAsIso8601(Time time);

// Formats a time compliant to ISO 8601 in the specified timezone.
// If the timezone is UTC, appends a 'Z' suffix, otherwise appends the offset,
// e.g. "2020-12-31T23:59:59.999-08:00".
// If |include_offset_suffix| is false, no offset or 'Z' suffix is appended.
BASE_I18N_EXPORT std::string TimeFormatAsIso8601WithTimeZone(
    Time time,
    const i18n::TimeZone& time_zone,
    bool include_offset_suffix = true);

// Formats a time compliant to ISO 8601 in the specified timezone.
// If the timezone is UTC, appends a 'Z' suffix, otherwise appends the offset,
// e.g. "2020-12-31T23:59:59.999-08:00".
// If |include_subseconds| is false, no subseconds is appended to the formatted
// date.
// If |include_offset_suffix| is false, no offset or 'Z' suffix is appended.
BASE_I18N_EXPORT std::string TimeFormatAsIso8601(
    Time time,
    const i18n::TimeZone& time_zone,
    const i18n::DateTimeFormatterOptions::TimePrecision& precision,
    bool include_offset_suffix);

// Formats a time in POSIX "unixtime" format with microsecond precision and
// local timezone offset, e.g., "2020-01-01T12:34:56.000000+00:00".
BASE_I18N_EXPORT std::string TimeFormatUnix(Time time);

// Formats a time in the IMF-fixdate format defined by RFC 7231 (satisfying its
// HTTP-date format), e.g. "Sun, 06 Nov 1994 08:49:37 GMT".
BASE_I18N_EXPORT std::string TimeFormatHTTP(Time time);

// Formats a time duration of hours and minutes into various formats, e.g.,
// "3:07" or "3 hours, 7 minutes", and returns true on success. See
// DurationFormatWidth for details.
[[nodiscard]] BASE_I18N_EXPORT bool TimeDurationFormat(
    TimeDelta time,
    DurationFormatWidth width,
    std::u16string* out);

// Formats a time duration of hours, minutes and seconds into various formats,
// e.g., "3:07:30" or "3 hours, 7 minutes, 30 seconds", and returns true on
// success. See DurationFormatWidth for details.
[[nodiscard]] BASE_I18N_EXPORT bool TimeDurationFormatWithSeconds(
    TimeDelta time,
    DurationFormatWidth width,
    std::u16string* out);

// Formats a time duration of hours, minutes and seconds into various formats,
// without the leading 0 time measurement units. e.g., "7m 30s" or
// "30 seconds", and returns true on success.
// Since the numeric format of time duration with the leading 0 omitted
// can produces ambiguous outputs such as "7:30", the "hh:mm:ss" format
// will always be used.
// See DurationFormatWidth for details.
[[nodiscard]] BASE_I18N_EXPORT bool TimeDurationCompactFormatWithSeconds(
    TimeDelta time,
    DurationFormatWidth width,
    std::u16string* out);

// Gets the hour clock type of the current locale. e.g.
// k12HourClock (en-US).
// k24HourClock (en-GB).
BASE_I18N_EXPORT HourClockType GetHourClockType();

}  // namespace base

#endif  // BASE_I18N_TIME_FORMATTING_H_
