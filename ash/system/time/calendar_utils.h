// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UTILS_H_

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace views {

class ColumnSet;

}  // namespace views

namespace ash {

namespace calendar_utils {

// Number of days in one week.
constexpr int kDateInOneWeek = 7;

// The padding in each date cell view.
constexpr int kDateVerticalPadding = 13;
constexpr int kDateHorizontalPadding = 2;
constexpr int kColumnSetPadding = 2;

// The insets within a Date cell.
constexpr gfx::Insets kDateCellInsets{kDateVerticalPadding,
                                      kDateHorizontalPadding};

// Checks if the `selected_date` is local time today.
bool IsToday(const base::Time::Exploded& selected_date);

// Gets the given `date`'s `Exploded` instance.
base::Time::Exploded GetExploded(const base::Time& date);

// Gets the given `date`'s month name in string in the current language.
std::u16string GetMonthName(const base::Time date);

// Set up the `GridLayout` to have 7 columns, which is one week row (7 days).
void SetUpWeekColumnSets(views::ColumnSet* column_set);

// Colors.
SkColor GetPrimaryTextColor();
SkColor GetSecondaryTextColor();

}  // namespace calendar_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UTILS_H_