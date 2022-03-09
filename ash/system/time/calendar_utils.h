// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UTILS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

#include <set>

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
constexpr int kDateHorizontalPadding = 14;
constexpr int kColumnSetPadding = 5;

// The insets within a Date cell.
constexpr gfx::Insets kDateCellInsets{kDateVerticalPadding,
                                      kDateHorizontalPadding};

// Duration of opacity animation for visibility changes.
constexpr base::TimeDelta kAnimationDurationForVisibility =
    base::Milliseconds(100);

// Fade-out and fade-in duration for resetting to today animation.
constexpr base::TimeDelta kResetToTodayFadeAnimationDuration =
    base::Milliseconds(100);

// Duration of moving animation.
constexpr base::TimeDelta kAnimationDurationForMoving = base::Milliseconds(300);

// Event fetch will terminate if we don't receive a response sooner than this.
constexpr base::TimeDelta kEventFetchTimeout = base::Milliseconds(1000);

// Checks if the `selected_date` is local time today.
bool IsToday(const base::Time selected_date);

// Checks if the two exploded are in the same day.
bool IsTheSameDay(absl::optional<base::Time> date_a,
                  absl::optional<base::Time> date_b);

// Returns the set of months that includes |selected_date| and
// |num_months_out| before and after.
std::set<base::Time> GetSurroundingMonthsUTC(const base::Time& selected_date,
                                             int num_months_out);

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

// Get the first day of the month that includes |date|.
base::Time GetFirstDayOfMonth(const base::Time& date);

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

// Returns true if it's a regular user or the user session is not blocked.
bool IsActiveUser();

// Get the time difference to UTC time based on the time passed in and the
// system timezone. Daylight saving is considered.
ASH_EXPORT int GetTimeDifferenceInMinutes(base::Time date);

}  // namespace calendar_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
