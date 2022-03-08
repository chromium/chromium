// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include <string>

#include "ash/components/settings/timezone_settings.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
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

  return base::TimeFormatWithPattern(date_a.value(), "dd MM YYYY") ==
         base::TimeFormatWithPattern(date_b.value(), "dd MM YYYY");
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

std::u16string GetMonthName(const base::Time date) {
  return base::TimeFormatWithPattern(date, "MMMM");
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
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  const int raw_time_diff = time_zone.getRawOffset() / kMillisecondsPerMinute;

  // Calculates the time difference adjust by the possible daylight savings
  // offset. If the status of any step fails, returns the default time
  // difference without considering daylight savings.

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar =
      std::make_unique<icu::GregorianCalendar>(time_zone, status);
  if (U_FAILURE(status))
    return raw_time_diff;

  UDate current_date =
      static_cast<UDate>(date.ToDoubleT() * base::Time::kMillisecondsPerSecond);
  status = U_ZERO_ERROR;
  gregorian_calendar->setTime(current_date, status);
  if (U_FAILURE(status))
    return raw_time_diff;

  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status))
    return raw_time_diff;

  int gmt_offset = time_zone.getRawOffset();
  if (day_light)
    gmt_offset += time_zone.getDSTSavings();

  return gmt_offset / kMillisecondsPerMinute;
}

}  // namespace calendar_utils

}  // namespace ash
