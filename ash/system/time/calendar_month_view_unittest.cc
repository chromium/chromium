// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

using CalendarMonthViewTest = AshTestBase;

// Test the basics of the `CalendarMonthView`.
TEST_F(CalendarMonthViewTest, Basics) {
  // Create a monthview based on Aug,1st 2021.
  // 1 , 2 , 3 , 4 , 5 , 6 , 7
  // 8 , 9 , 10, 11, 12, 13, 14
  // 15, 16, 17, 18, 19, 20, 21
  // 22, 23, 24, 25, 26, 27, 28
  // 29, 30, 31, 1 , 2 , 3 , 4
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  std::unique_ptr<CalendarMonthView> month_view_ =
      std::make_unique<CalendarMonthView>(date);

  // Randomly checks some dates in this month view.
  EXPECT_EQ(
      static_cast<views::LabelButton*>(month_view_->children()[0])->GetText(),
      u"1");
  EXPECT_EQ(
      static_cast<views::LabelButton*>(month_view_->children()[30])->GetText(),
      u"31");
  EXPECT_EQ(
      static_cast<views::LabelButton*>(month_view_->children()[34])->GetText(),
      u"4");

  views::GridLayout* layout =
      static_cast<views::GridLayout*>(month_view_->GetLayoutManager());
  views::ColumnSet* column_set0 = layout->GetColumnSet(0);
  // 7 date columns and 7 padding columns.
  EXPECT_EQ(column_set0->num_columns(), 14);

  // Create a monthview based on Jun,1st 2021, which has the previous month's
  // dates in the first row.
  // 30, 31, 1 , 2 , 3 , 4 , 5
  // 6 , 7 , 8 , 9 , 10, 11, 12
  // 13, 14, 15, 16, 17, 18, 19
  // 20, 21, 22, 23, 24, 25, 26
  // 27, 28, 29, 30, 1 , 2 , 3
  base::Time jun_date;
  ASSERT_TRUE(base::Time::FromString("1 Jun 2021 10:00 GMT", &jun_date));

  std::unique_ptr<CalendarMonthView> jun_month_view_ =
      std::make_unique<CalendarMonthView>(jun_date);

  // Randomly checks some dates in this month view.
  EXPECT_EQ(static_cast<views::LabelButton*>(jun_month_view_->children()[0])
                ->GetText(),
            u"30");
  EXPECT_EQ(static_cast<views::LabelButton*>(jun_month_view_->children()[30])
                ->GetText(),
            u"29");
  EXPECT_EQ(static_cast<views::LabelButton*>(jun_month_view_->children()[34])
                ->GetText(),
            u"3");
}

}  // namespace ash
