// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include <string>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

ASH_EXPORT void GetSurroundingMonthsUTC(const base::Time& selected_date,
                                        unsigned int num_months_out,
                                        std::set<base::Time>& months_) {
  // Make the output is empty before we start.
  months_.clear();

  // First month is the one that contains |selected_date|.
  base::Time selected_date_start =
      calendar_utils::GetStartOfMonthUTC(selected_date);
  months_.emplace(selected_date_start);

  // Add |num_months_out| before.
  base::Time current = selected_date_start;
  for (unsigned int i = 0; i < num_months_out; ++i) {
    current = calendar_utils::GetStartOfPreviousMonthUTC(current);
    months_.emplace(current);
  }

  // Add |num_months_out| after.
  current = selected_date_start;
  for (unsigned int i = 0; i < num_months_out; ++i) {
    current = calendar_utils::GetStartOfNextMonthUTC(current);
    months_.emplace(current);
  }
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

}  // namespace calendar_utils

}  // namespace ash
