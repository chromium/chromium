// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include <optional>
#include <string>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/time/date_helper.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/user_manager/user_type.h"
#include "ui/views/layout/table_layout.h"

namespace ash::calendar_utils {

bool IsMultiCalendarEnabled() {
  return features::IsMultiCalendarSupportEnabled();
}

bool IsToday(const base::Time selected_date) {
  return IsTheSameDay(selected_date, base::Time::Now());
}

bool IsTheSameDay(std::optional<base::Time> date_a,
                  std::optional<base::Time> date_b) {
  if (!date_a.has_value() || !date_b.has_value()) {
    return false;
  }

  return calendar_utils::GetMonthDayYear(date_a.value()) ==
         calendar_utils::GetMonthDayYear(date_b.value());
}

ASH_EXPORT std::set<base::Time> GetSurroundingMonthsUTC(
    const base::Time& selected_date,
    int num_months_out) {
  std::set<base::Time> months;

  // First month is the one that contains |selected_date|.
  base::Time selected_date_start =
      calendar_utils::GetStartOfMonthUTC(selected_date);
  months.emplace(selected_date_start);

  // Add |num_months_out| before and after.
  base::Time current_forward = selected_date_start;
  base::Time current_backward = selected_date_start;
  for (int i = 0; i < num_months_out; ++i) {
    current_forward = calendar_utils::GetStartOfNextMonthUTC(current_forward);
    months.emplace(current_forward);
    current_backward =
        calendar_utils::GetStartOfPreviousMonthUTC(current_backward);
    months.emplace(current_backward);
  }

  return months;
}

base::Time::Exploded GetExplodedUTC(const base::Time& date) {
  base::Time::Exploded exploded;
  date.UTCExplode(&exploded);
  return exploded;
}

std::u16string FormatDate(const icu::SimpleDateFormat& formatter,
                          const base::Time date) {
  return DateHelper::GetInstance()->GetFormattedTime(&formatter, date);
}

std::u16string FormatInterval(const icu::DateIntervalFormat* formatter,
                              const base::Time& start_time,
                              const base::Time& end_time) {
  return DateHelper::GetInstance()->GetFormattedInterval(formatter, start_time,
                                                         end_time);
}

std::u16string GetMonthDayYear(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_day_year_formatter(), date);
}

std::u16string GetMonthDayYearWeek(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_day_year_week_formatter(), date);
}

std::u16string GetMonthName(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_name_formatter(), date);
}

std::u16string GetDayOfMonth(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->day_of_month_formatter(), date);
}

std::u16string GetDayIntOfMonth(const base::Time local_date) {
  return base::UTF8ToUTF16(base::NumberToString(
      calendar_utils::GetExplodedUTC(local_date).day_of_month));
}

std::u16string GetMonthNameAndDayOfMonth(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_day_formatter(), date);
}

std::u16string GetTwelveHourClockTime(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->twelve_hour_clock_formatter(), date);
}

std::u16string GetTwentyFourHourClockTime(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->twenty_four_hour_clock_formatter(), date);
}

std::u16string GetTimeZone(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->time_zone_formatter(), date);
}

std::u16string GetDayOfWeek(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->day_of_week_formatter(), date);
}

std::u16string GetYear(const base::Time date) {
  return calendar_utils::FormatDate(DateHelper::GetInstance()->year_formatter(),
                                    date);
}

std::u16string GetMonthNameAndYear(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_name_year_formatter(), date);
}

std::u16string GetTwelveHourClockHours(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->twelve_hour_clock_hours_formatter(), date);
}

std::u16string GetTwentyFourHourClockHours(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->twenty_four_hour_clock_hours_formatter(),
      date);
}

std::u16string GetMinutes(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->minutes_formatter(), date);
}

std::u16string FormatTwelveHourClockTimeInterval(const base::Time& start_time,
                                                 const base::Time& end_time) {
  return calendar_utils::FormatInterval(
      DateHelper::GetInstance()->twelve_hour_clock_interval_formatter(),
      start_time, end_time);
}

std::u16string FormatTwentyFourHourClockTimeInterval(
    const base::Time& start_time,
    const base::Time& end_time) {
  return calendar_utils::FormatInterval(
      DateHelper::GetInstance()->twenty_four_hour_clock_interval_formatter(),
      start_time, end_time);
}

void SetUpWeekColumns(views::TableLayout* layout) {
  for (int i = 0; i < calendar_utils::kDateInOneWeek; ++i) {
    layout->AddColumn(views::LayoutAlignment::kStretch,
                      views::LayoutAlignment::kStretch, 1.0f,
                      views::TableLayout::ColumnSize::kFixed, 0, 0);
  }
}

int GetMonthsBetween(const base::Time& start_date, const base::Time& end_date) {
  base::Time::Exploded start_exp = calendar_utils::GetExplodedUTC(start_date);
  base::Time::Exploded end_exp = calendar_utils::GetExplodedUTC(end_date);
  return (end_exp.year - start_exp.year) * 12 +
         (end_exp.month - start_exp.month) % 12;
}

base::Time GetMaxTime(const base::Time d1, const base::Time d2) {
  return (d1 > d2) ? d1 : d2;
}

base::Time GetMinTime(const base::Time d1, const base::Time d2) {
  return (d1 < d2) ? d1 : d2;
}

SkColor GetPrimaryTextColor() {
  const ash::AshColorProvider* color_provider = ash::AshColorProvider::Get();
  return color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

SkColor GetSecondaryTextColor() {
  const ash::AshColorProvider* color_provider = ash::AshColorProvider::Get();
  return color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
}

SkColor GetDisabledTextColor() {
  const ash::AshColorProvider* color_provider = ash::AshColorProvider::Get();
  const SkColor primary_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  return ColorUtil::GetDisabledColor(primary_color);
}

base::Time GetFirstDayOfMonth(const base::Time& date) {
  return date -
         base::Days(calendar_utils::GetExplodedUTC(date).day_of_month - 1);
}

base::Time GetStartOfPreviousMonthLocal(base::Time date) {
  return GetFirstDayOfMonth(GetFirstDayOfMonth(date) - base::Days(1));
}

base::Time GetStartOfNextMonthLocal(base::Time date) {
  // Adds over 31 days to make sure it goes to the next month.
  return GetFirstDayOfMonth(GetFirstDayOfMonth(date) + base::Days(33));
}

ASH_EXPORT base::Time GetStartOfMonthUTC(const base::Time& date) {
  return (date -
          base::Days(calendar_utils::GetExplodedUTC(date).day_of_month - 1))
      .UTCMidnight();
}

base::Time GetNextDayMidnight(base::Time date) {
  return (date.UTCMidnight() + base::Days(1) + kDurationForAdjustingDST)
      .UTCMidnight();
}

ASH_EXPORT base::Time GetStartOfPreviousMonthUTC(base::Time date) {
  return GetStartOfMonthUTC(GetStartOfMonthUTC(date) - base::Days(1));
}

ASH_EXPORT base::Time GetStartOfNextMonthUTC(base::Time date) {
  // Adds over 31 days to make sure it goes to the next month.
  return GetStartOfMonthUTC(GetStartOfMonthUTC(date) + base::Days(33));
}

ASH_EXPORT bool ShouldFetchCalendarData() {
  return IsActiveUser() && !IsDisabledByAdmin();
}

ASH_EXPORT bool IsActiveUser() {
  std::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return (user_type && (*user_type == user_manager::UserType::kRegular ||
                        *user_type == user_manager::UserType::kChild)) &&
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

ASH_EXPORT bool IsDisabledByAdmin() {
  const auto* const client = Shell::Get()->calendar_controller()->GetClient();
  return !client || client->IsDisabledByAdmin();
}

base::TimeDelta GetTimeDifference(base::Time date) {
  return DateHelper::GetInstance()->GetTimeDifference(date);
}

base::Time GetFirstDayOfWeekLocalMidnight(base::Time date) {
  int day_of_week = calendar_utils::GetDayOfWeekInt(date);
  base::Time first_day_of_week =
      DateHelper::GetInstance()->GetLocalMidnight(date) -
      base::Days(day_of_week - 1) + kDurationForAdjustingDST;
  return DateHelper::GetInstance()->GetLocalMidnight(first_day_of_week);
}

ASH_EXPORT const std::pair<base::Time, base::Time> GetFetchStartEndTimes(
    base::Time start_of_month_local_midnight) {
  base::Time start = start_of_month_local_midnight -
                     DateHelper::GetInstance()->GetTimeDifference(
                         start_of_month_local_midnight);
  base::Time end =
      GetStartOfMonthUTC(start_of_month_local_midnight + base::Days(33));
  end -= DateHelper::GetInstance()->GetTimeDifference(end);
  return std::make_pair(start, end);
}

int GetDayOfWeekInt(const base::Time date) {
  int day_int;
  if (base::StringToInt(GetDayOfWeek(date), &day_int)) {
    return day_int;
  }

  // For a few special locales the day of week is not in a number. In these
  // cases, use the default day of week from time exploded. For example:
  // 'pa-PK', it returns '۰۳' for the fourth day of week.
  base::Time date_local = date + GetTimeDifference(date);
  base::Time::Exploded local_date_exploded = GetExplodedUTC(date_local);
  // Time exploded uses 0-based day of week (0 = Sunday, etc.)
  return local_date_exploded.day_of_week + 1;
}

bool IsMultiDayEvent(const google_apis::calendar::CalendarEvent* event) {
  DCHECK(event);
  return (GetStartTimeMidnightAdjusted(event) <
          GetEndTimeMidnightAdjusted(event));
}

base::Time GetStartTimeAdjusted(
    const google_apis::calendar::CalendarEvent* event) {
  base::Time start_time = event->start_time().date_time();
  return start_time + GetTimeDifference(start_time);
}

base::Time GetEndTimeAdjusted(
    const google_apis::calendar::CalendarEvent* event) {
  base::Time end_time = event->end_time().date_time();
  return end_time + GetTimeDifference(end_time);
}

ASH_EXPORT base::Time GetStartTimeMidnightAdjusted(
    const google_apis::calendar::CalendarEvent* event) {
  return GetStartTimeAdjusted(event).UTCMidnight();
}

ASH_EXPORT base::Time GetEndTimeMidnightAdjusted(
    const google_apis::calendar::CalendarEvent* event) {
  return GetEndTimeAdjusted(event).UTCMidnight();
}

ASH_EXPORT const std::tuple<base::Time, base::Time> GetStartAndEndTime(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time& selected_date,
    const base::Time& selected_date_midnight,
    const base::Time& selected_date_midnight_utc) {
  const base::Time selected_last_minute =
      calendar_utils::GetNextDayMidnight(selected_date_midnight) -
      base::Minutes(1);
  const base::TimeDelta time_difference =
      calendar_utils::GetTimeDifference(selected_date);
  const base::Time selected_last_minute_utc =
      selected_last_minute - time_difference;

  // If it's an "all day" event, then we want to display 00:00 - 23:59 for the
  // event. The formatter we use will apply timezone changes to the given
  // `base::Time` which are set to UTC midnight in the response, so we need to
  // negate the timezone, so when the formatter formats, it will make the dates
  // midnight in the local timezone.
  if (event->all_day_event()) {
    return std::make_tuple(selected_date_midnight_utc,
                           selected_last_minute_utc);
  }

  base::Time start_time = calendar_utils::GetMaxTime(
      event->start_time().date_time(), selected_date_midnight_utc);
  base::Time end_time = calendar_utils::GetMinTime(
      event->end_time().date_time(), selected_last_minute_utc);

  return std::make_tuple(start_time, end_time);
}

const std::tuple<base::Time, base::Time> GetMidnight(const base::Time time) {
  const auto time_difference = GetTimeDifference(time);
  const auto utc_midnight = (time + time_difference).UTCMidnight();
  const auto local_midnight = utc_midnight - time_difference;

  return std::make_tuple(utc_midnight, local_midnight);
}

}  // namespace ash::calendar_utils
