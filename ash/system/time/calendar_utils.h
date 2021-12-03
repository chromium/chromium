// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UTILS_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace views {

class TableLayout;

}  // namespace views

namespace ash {

namespace calendar_utils {

// Number of days in one week.
constexpr int kDateInOneWeek = 7;

// The padding in each date cell view.
constexpr int kDateVerticalPadding = 13;
constexpr int kDateHorizontalPadding = 12;
constexpr int kColumnSetPadding = 3;

// The insets within a Date cell.
constexpr gfx::Insets kDateCellInsets{kDateVerticalPadding,
                                      kDateHorizontalPadding};

// Duration of opacity animation for visibility changes.
constexpr base::TimeDelta kAnimationDurationForVisibility =
    base::Milliseconds(100);

// Duration of moving animation.
constexpr base::TimeDelta kAnimationDurationForMoving = base::Milliseconds(300);

// Checks if the `selected_date` is local time today.
bool IsToday(const base::Time::Exploded& selected_date);

// Checks if the two exploded are in the same day.
bool IsTheSameDay(absl::optional<base::Time::Exploded> date_a,
                  absl::optional<base::Time::Exploded> date_b);

// Gets the given `date`'s `Exploded` instance, in local time.
base::Time::Exploded GetExplodedLocal(const base::Time& date);

// Gets the given `date`'s `Exploded` instance, in UTC time.
base::Time::Exploded GetExplodedUTC(const base::Time& date);

// Gets the given `date`'s month name in string in the current language.
std::u16string GetMonthName(const base::Time date);

// Sets up the `TableLayout` to have 7 columns, which is one week row (7 days).
void SetUpWeekColumns(views::TableLayout* layout);

// Colors.
SkColor GetPrimaryTextColor();
SkColor GetSecondaryTextColor();

// Get local midnight on the first day of the month that includes |date|.
base::Time GetStartOfMonthLocal(const base::Time& date);

// Get local midnight on the first day of the month before the one that includes
// |date|.
base::Time GetStartOfPreviousMonthLocal(base::Time date);

// Get local midnight on the first day of the month after the one that includes
// |date|.
base::Time GetStartOfNextMonthLocal(base::Time date);

// Get UTC midnight on the first day of the month that includes |date|.
base::Time GetStartOfMonthUTC(const base::Time& date);

// Get UTC midnight on the first day of the month before the one that includes
// |date|.
base::Time GetStartOfPreviousMonthUTC(base::Time date);

// Get UTC midnight on the first day of the month after the one that includes
// |date|.
base::Time GetStartOfNextMonthUTC(base::Time date);

}  // namespace calendar_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
