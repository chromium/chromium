// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include "ash/style/ash_color_provider.h"
#include "base/i18n/unicodestring.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

namespace calendar_utils {

bool IsToday(const base::Time::Exploded& selected_date) {
  base::Time::Exploded today_exploded = GetExplodedLocal(base::Time::Now());
  return IsTheSameDay(selected_date, today_exploded);
}
bool IsTheSameDay(absl::optional<base::Time::Exploded> date_a,
                  absl::optional<base::Time::Exploded> date_b) {
  if (!date_a.has_value() || !date_b.has_value())
    return false;
  return date_a->year == date_b->year && date_a->month == date_b->month &&
         date_a->day_of_month == date_b->day_of_month;
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
  // Inits status with no error, no warning.
  UErrorCode status = U_ZERO_ERROR;

  // Generates the "MMMM" pattern.
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(UDAT_MONTH), status);
  DCHECK(U_SUCCESS(status));

  // Then, creates the formatter and formats `date` to string with the pattern.
  auto dfmt =
      std::make_unique<icu::SimpleDateFormat>(generated_pattern, status);
  UDate unicode_date = static_cast<UDate>(date.ToDoubleT() * 1000);
  icu::UnicodeString unicode_string;
  unicode_string = dfmt->format(unicode_date, unicode_string);

  return base::i18n::UnicodeStringToString16(unicode_string);
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

base::Time GetStartOfMonthLocal(const base::Time& date) {
  return (date -
          base::Days(calendar_utils::GetExplodedLocal(date).day_of_month - 1))
      .LocalMidnight();
}

base::Time GetStartOfPreviousMonthLocal(base::Time date) {
  return GetStartOfMonthLocal(GetStartOfMonthLocal(date) - base::Days(1));
}

base::Time GetStartOfNextMonthLocal(base::Time date) {
  // Adds over 31 days to make sure it goes to the next month.
  return GetStartOfMonthLocal(GetStartOfMonthLocal(date) + base::Days(33));
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

}  // namespace calendar_utils

}  // namespace ash
