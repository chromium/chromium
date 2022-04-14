// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include <string>

#include "ash/components/settings/timezone_settings.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/time/date_helper.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

namespace calendar_utils {

bool IsToday(const base::Time selected_date) {
  return IsTheSameDay(selected_date, base::Time::Now());
}

bool IsTheSameDay(absl::optional<base::Time> date_a,
                  absl::optional<base::Time> date_b) {
  if (!date_a.has_value() || !date_b.has_value())
    return false;

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

base::Time::Exploded GetExplodedLocal(const base::Time& date) {
  base::Time::Exploded exploded;
  date.LocalExplode(&exploded);
  return exploded;
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

std::u16string GetMonthDayYear(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_day_year_formatter(), date);
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

std::u16string GetYear(const base::Time date) {
  return calendar_utils::FormatDate(DateHelper::GetInstance()->year_formatter(),
                                    date);
}

std::u16string GetMonthNameAndYear(const base::Time date) {
  return calendar_utils::FormatDate(
      DateHelper::GetInstance()->month_name_year_formatter(), date);
}

void SetUpWeekColumns(views::TableLayout* layout) {
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, kColumnSetPadding);
  for (int i = 0; i < calendar_utils::kDateInOneWeek; ++i) {
    layout
        ->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kStretch, 1.0f,
                    views::TableLayout::ColumnSize::kFixed, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize, kColumnSetPadding);
  }
}

int GetMonthsBetween(const base::Time& start_date, const base::Time& end_date) {
  base::Time::Exploded start_exp = calendar_utils::GetExplodedUTC(start_date);
  base::Time::Exploded end_exp = calendar_utils::GetExplodedUTC(end_date);
  return (end_exp.year - start_exp.year) * 12 +
         (end_exp.month - start_exp.month) % 12;
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

ASH_EXPORT base::Time GetStartOfPreviousMonthUTC(base::Time date) {
  return GetStartOfMonthUTC(GetStartOfMonthUTC(date) - base::Days(1));
}

ASH_EXPORT base::Time GetStartOfNextMonthUTC(base::Time date) {
  // Adds over 31 days to make sure it goes to the next month.
  return GetStartOfMonthUTC(GetStartOfMonthUTC(date) + base::Days(33));
}

bool IsActiveUser() {
  absl::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return (user_type && *user_type == user_manager::USER_TYPE_REGULAR) &&
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

int GetTimeDifferenceInMinutes(base::Time date) {
  return DateHelper::GetInstance()->GetTimeDifferenceInMinutes(date);
}

}  // namespace calendar_utils

}  // namespace ash
