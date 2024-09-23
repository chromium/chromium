// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UTILS_H_

#include <optional>
#include <set>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace views {

class TableLayout;

}  // namespace views

namespace ash {

namespace calendar_utils {

// Number of days in one week.
constexpr int kDateInOneWeek = 7;

// Milliseconds per minute.
constexpr int kMillisecondsPerMinute = 60000;

// The padding in each date cell view.
constexpr int kDateVerticalPadding = 13;
constexpr int kDateHorizontalPadding = 16;
constexpr int kColumnSetPadding = 5;

// The insets for the event list item view.
constexpr int kEventListItemViewStartEndMargin = 12;

// The insets within a Date cell.
const auto kDateCellInsets =
    gfx::Insets::VH(kDateVerticalPadding, kDateHorizontalPadding);

// Duration of opacity animation for visibility changes.
constexpr base::TimeDelta kAnimationDurationForVisibility =
    base::Milliseconds(200);

// Fade-out and fade-in duration for resetting to today animation.
constexpr base::TimeDelta kResetToTodayFadeAnimationDuration =
    base::Milliseconds(100);

// Duration of moving animation.
constexpr base::TimeDelta kAnimationDurationForMoving = base::Milliseconds(300);

// Duration of month moving animation.
constexpr base::TimeDelta kAnimationDurationForMonthMoving =
    base::Milliseconds(600);

// This duration is added to a midnight `base::Time` to adjust the DST when
// adding days by `base::Day`.
//
// For example, in PST time zone, when we add 1 week (7 days) to Nov 3rd 00:00,
// the expected result is Nov 10th 00:00, but the actual result is Nov 9th
// 23:00. Because in Nov 6th is the DST end day and there are 25 hours in that
// day. By adding this time delta, the result is Nov 10th 4:00, which is the
// expected date.
constexpr base::TimeDelta kDurationForAdjustingDST = base::Hours(5);

// Duration subtracted from an existing midnight in UTC to get the
// previous day. It is less than 24 hours to consider daylight savings.
constexpr base::TimeDelta kDurationForGettingPreviousDay = base::Hours(20);

// The fetch of a user's calendar list or event list will terminate if a
// response is not received sooner than this.
constexpr base::TimeDelta kCalendarDataFetchTimeout = base::Seconds(10);

// Maximum number of selected calendars for which events should be fetched when
// Multi-Calendar Support is enabled.
constexpr int kMultipleCalendarsLimit = 10;

// Number of months, before and after the month currently on-display, that we
// cache-ahead.
constexpr int kNumSurroundingMonthsCached = 2;

// Maximum number of non-prunable months allowed, which is a function of
// kNumSurroundingMonthsCached.
constexpr int kMaxNumNonPrunableMonths = 2 * kNumSurroundingMonthsCached + 1;

// Maximum number of prunable months to cache. Note that this plus
// kMaxNumNonPrunableMonths is the total maximum number of cached months.
constexpr int kMaxNumPrunableMonths = 20;

// Between child spacing for `CalendarUpNextView`.
constexpr int kUpNextBetweenChildSpacing = 8;

// The `CalendarUpNextView` UI has a rounded 'nub' that sticks up in the middle
// of the view. To ensure that the scroll view animates nicely behind the up
// next view, we need to forcibly overlap the views slightly for the distance
// between the bottom and top of the 'nub'.
constexpr int kUpNextOverlapInPx = 12;

// Returns true if the Multi-Calendar Support feature is enabled.
bool IsMultiCalendarEnabled();

// Checks if the `selected_date` is local time today.
bool IsToday(const base::Time selected_date);

// Checks if the two exploded are in the same day.
bool IsTheSameDay(std::optional<base::Time> date_a,
                  std::optional<base::Time> date_b);

// Returns the set of months that includes |selected_date| and
// |num_months_out| before and after.
std::set<base::Time> GetSurroundingMonthsUTC(const base::Time& selected_date,
                                             int num_months_out);

// Gets the given `date`'s `Exploded` instance, in UTC time.
base::Time::Exploded GetExplodedUTC(const base::Time& date);

// Gets the `date`'s month name, numeric day of month, and year.
// (e.g. March 10, 2022)
ASH_EXPORT std::u16string GetMonthDayYear(const base::Time date);

// Gets the `date`'s month name, numeric day of month, year and day of week.
// (e.g. Wednesday, May 25, 2022)
ASH_EXPORT std::u16string GetMonthDayYearWeek(const base::Time date);

// Gets the `date`'s month name in string in the current language.
// (e.g. March)
ASH_EXPORT std::u16string GetMonthName(const base::Time date);

// Gets the `date`'s day of month in local format. For some languages, the
// formatter adds some words/characters which means `day` in that language to
// the digital day of month (e.g. 10日).
ASH_EXPORT std::u16string GetDayOfMonth(const base::Time date);

// Gets the `date`'s numeric day of month.
// (e.g. 10)
ASH_EXPORT std::u16string GetDayIntOfMonth(const base::Time local_date);

// Gets the `date`'s month name and the numeric day of month.
// (e.g. March 10)
ASH_EXPORT std::u16string GetMonthNameAndDayOfMonth(const base::Time date);

// Gets the `date`'s time in twelve hour clock format.
// (e.g. 10:31 PM)
ASH_EXPORT std::u16string GetTwelveHourClockTime(const base::Time date);

// Gets the `date`'s time in twenty four hour clock format.
// (e.g. 22:31)
ASH_EXPORT std::u16string GetTwentyFourHourClockTime(const base::Time date);

// Gets the `date`'s time zone.
// (e.g. Greenwich Mean Time)
ASH_EXPORT std::u16string GetTimeZone(const base::Time date);

// Gets the index of this day in the week, starts from 1. This number is
// different for different languages.
ASH_EXPORT std::u16string GetDayOfWeek(const base::Time date);

// Gets the `date`'s year.
// (e.g. 2022)
ASH_EXPORT std::u16string GetYear(const base::Time date);

// Gets the `date`'s month name and year.
// (e.g. March 2022)
ASH_EXPORT std::u16string GetMonthNameAndYear(const base::Time date);

// Gets the `date`'s hour in twelve hour clock format.
// (e.g. 9, when given 21:05)
// Some locales may add zero-padding.
ASH_EXPORT std::u16string GetTwelveHourClockHours(const base::Time date);

// Gets the `date`'s hour in twenty four hour clock format.
// (e.g. 21, when given 21:05)
// Some locales may add zero-padding.
ASH_EXPORT std::u16string GetTwentyFourHourClockHours(const base::Time date);

// Gets the `date`'s minutes with zero-padding.
// (e.g. 05, when given 22:05)
ASH_EXPORT std::u16string GetMinutes(const base::Time date);

// Gets the formatted interval between `start_time` and `end_time` in twelve
// hour clock format.
// (e.g. 8:30 – 9:30 PM or 11:30 AM – 2:30 PM)
ASH_EXPORT std::u16string FormatTwelveHourClockTimeInterval(
    const base::Time& start_time,
    const base::Time& end_time);

// Gets the formatted interval between `start_time` and `end_time` in twenty
// four hour clock format.
// (e.g. 20:30 – 21:30)
ASH_EXPORT std::u16string FormatTwentyFourHourClockTimeInterval(
    const base::Time& start_time,
    const base::Time& end_time);

// Sets up the `TableLayout` to have 7 columns, which is one week row (7 days).
void SetUpWeekColumns(views::TableLayout* layout);

// Computes the distance, in months, between `start_date` and `end_date`.
ASH_EXPORT int GetMonthsBetween(const base::Time& start_date,
                                const base::Time& end_date);

// Gets date with greater value between `d1` and `d2`.
ASH_EXPORT base::Time GetMaxTime(const base::Time d1, const base::Time d2);

// Gets date with lesser value between `d1` and `d2`.
ASH_EXPORT base::Time GetMinTime(const base::Time d1, const base::Time d2);

// Colors.
SkColor GetPrimaryTextColor();
SkColor GetSecondaryTextColor();
SkColor GetDisabledTextColor();

// Get the first day of the month that includes |date|.
ASH_EXPORT base::Time GetFirstDayOfMonth(const base::Time& date);

// Get the first day of the month before the one that includes |date|.
base::Time GetStartOfPreviousMonthLocal(base::Time date);

// Get the first day of the month after the one that includes |date|.
base::Time GetStartOfNextMonthLocal(base::Time date);

// Get UTC midnight on the first day of the month that includes |date|.
base::Time GetStartOfMonthUTC(const base::Time& date);

// Get UTC midnight on the first day of the month before the one that includes
// |date|.
base::Time GetStartOfPreviousMonthUTC(base::Time date);

// Get UTC midnight on the first day of the month after the one that includes
// |date|.
base::Time GetStartOfNextMonthUTC(base::Time date);

// Returns UTC midnight of `date`'s next day without adjusting time difference.
ASH_EXPORT base::Time GetNextDayMidnight(base::Time date);

// Returns true if (1) it's a regular user; and (2) the user session is not
// blocked; and (3) the admin has not disabled Google Calendar integration.
bool ShouldFetchCalendarData();

// Returns true if it's a regular user or the user session is not blocked.
bool IsActiveUser();

// Returns true if the admin has disabled Google Calendar integration.
bool IsDisabledByAdmin();

// Get the time difference to UTC time based on the time passed in and the
// system timezone. Daylight saving is considered.
ASH_EXPORT base::TimeDelta GetTimeDifference(base::Time date);

// Gets the first day's local midnight of the week based on the `date`.
base::Time GetFirstDayOfWeekLocalMidnight(base::Time date);

// Calculate the start/end times for a fetch of a single month's events.
// `start_of_month_local_midnight` should be local midnight on the first day of
// a month, and the two `base::Time` objects returned are the UTC times that
// represent the start of the month and start of the next month. See the unit
// test of this method in calendar_utils_unittest.cc for examples.
ASH_EXPORT const std::pair<base::Time, base::Time> GetFetchStartEndTimes(
    base::Time start_of_month_local_midnight);

// Gets the int index of this day in the week, starts from 1. This number is
// different for different languages. If cannot find this local's day in a week,
// returns its time exploded's `day_of_week`;
ASH_EXPORT int GetDayOfWeekInt(const base::Time date);

// Checks if the event spans more than one day.
ASH_EXPORT bool IsMultiDayEvent(
    const google_apis::calendar::CalendarEvent* event);

// Returns the `start_time` of `event` adjusted by time difference, to ensure
// that each event is stored by its local time, e.g. an event that starts at
// 2022-05-31 22:00:00.000 PST (2022-06-01 05:00:00.000 UTC) is stored in the
// map for 05-2022.
base::Time GetStartTimeAdjusted(
    const google_apis::calendar::CalendarEvent* event);

// Returns the `end_time` of `event` adjusted by time difference.
base::Time GetEndTimeAdjusted(
    const google_apis::calendar::CalendarEvent* event);

// Returns midnight on the day of the start time of `event`.
ASH_EXPORT base::Time GetStartTimeMidnightAdjusted(
    const google_apis::calendar::CalendarEvent* event);

// Returns midnight on the day of the end time of `event`.
ASH_EXPORT base::Time GetEndTimeMidnightAdjusted(
    const google_apis::calendar::CalendarEvent* event);

// Gets the event start and end times accounting for timezone.
const std::tuple<base::Time, base::Time> GetStartAndEndTime(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time& selected_date,
    const base::Time& selected_date_midnight,
    const base::Time& selected_date_midnight_utc);

// Calculates the UTC and local midnight times for the given `base::Time`,
// rounding to the correct midnight for the given timezone. This avoids an
// issue with `base::Time::UTCMidnight()`, which will (in certain ahead
// timezones) return the previous days midnight.
// For example, if the current time is 19 Jan 2023 00:10 in GMT+13, then
// `GetUTCMidnight` will return 19 Jan 2023 00:00 UTC.
// `base::Time::UTCMidnight()` will round down to 18 Jan 2023 00:00 UTC.
ASH_EXPORT const std::tuple<base::Time, base::Time> GetMidnight(
    const base::Time);

}  // namespace calendar_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
