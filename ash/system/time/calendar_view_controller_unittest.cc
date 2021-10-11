// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using CalendarViewControllerUnittest = AshTestBase;

TEST_F(CalendarViewControllerUnittest, UtilFunctions) {
  auto controller = std::make_unique<CalendarViewController>();

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));

  controller->UpdateMonth(date);

  base::Time::Exploded first_day_exploded;
  base::Time first_day = controller->GetOnScreenMonthFirstDay();
  first_day.LocalExplode(&first_day_exploded);
  std::u16string month_name = controller->GetOnScreenMonthName();

  EXPECT_EQ(8, first_day_exploded.month);
  EXPECT_EQ(1, first_day_exploded.day_of_month);
  EXPECT_EQ(2021, first_day_exploded.year);
  EXPECT_EQ(u"August", month_name);

  base::Time::Exploded previous_first_day_exploded;
  base::Time previous_first_day = controller->GetPreviousMonthFirstDay();
  previous_first_day.LocalExplode(&previous_first_day_exploded);
  std::u16string previous_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(7, previous_first_day_exploded.month);
  EXPECT_EQ(1, previous_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, previous_first_day_exploded.year);
  EXPECT_EQ(u"July", previous_month_name);

  base::Time::Exploded next_first_day_exploded;
  base::Time next_first_day = controller->GetNextMonthFirstDay();
  next_first_day.LocalExplode(&next_first_day_exploded);
  std::u16string next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(9, next_first_day_exploded.month);
  EXPECT_EQ(1, next_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, next_first_day_exploded.year);
  EXPECT_EQ(u"September", next_month_name);
}

TEST_F(CalendarViewControllerUnittest, CornerCases) {
  auto controller = std::make_unique<CalendarViewController>();

  // Next month of Dec should be Jan of next year.
  base::Time last_month_date;
  ASSERT_TRUE(
      base::Time::FromString("24 Dec 2021 10:00 GMT", &last_month_date));

  controller->UpdateMonth(last_month_date);

  base::Time::Exploded january_first_day_exploded;
  base::Time january_first_day = controller->GetNextMonthFirstDay();
  january_first_day.LocalExplode(&january_first_day_exploded);
  std::u16string january_month_name = controller->GetNextMonthName();

  EXPECT_EQ(1, january_first_day_exploded.month);
  EXPECT_EQ(1, january_first_day_exploded.day_of_month);
  EXPECT_EQ(2022, january_first_day_exploded.year);
  EXPECT_EQ(u"January", january_month_name);

  // Previous month of Jan should be Dec of last year
  base::Time first_month_date;
  ASSERT_TRUE(
      base::Time::FromString("24 Jan 2021 10:00 GMT", &first_month_date));

  controller->UpdateMonth(first_month_date);

  base::Time::Exploded dec_first_day_exploded;
  base::Time dec_first_day = controller->GetPreviousMonthFirstDay();
  dec_first_day.LocalExplode(&dec_first_day_exploded);
  std::u16string dec_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(12, dec_first_day_exploded.month);
  EXPECT_EQ(1, dec_first_day_exploded.day_of_month);
  EXPECT_EQ(2020, dec_first_day_exploded.year);
  EXPECT_EQ(u"December", dec_month_name);
}

}  // namespace ash
