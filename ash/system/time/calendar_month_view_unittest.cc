// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include <memory>

#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

class CalendarMonthViewTest : public AshTestBase {
 public:
  CalendarMonthViewTest() = default;
  CalendarMonthViewTest(const CalendarMonthViewTest&) = delete;
  CalendarMonthViewTest& operator=(const CalendarMonthViewTest&) = delete;
  ~CalendarMonthViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    calendar_month_view_.reset();
    controller_.reset();

    AshTestBase::TearDown();
  }

  void CreateMonthView(base::Time date) {
    calendar_month_view_.reset();
    controller_->UpdateMonth(date);
    calendar_month_view_ =
        std::make_unique<CalendarMonthView>(date, controller_.get());
    calendar_month_view_->Layout();
  }

  CalendarMonthView* month_view() { return calendar_month_view_.get(); }
  CalendarViewController* controller() { return controller_.get(); }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  std::unique_ptr<CalendarMonthView> calendar_month_view_;
  std::unique_ptr<CalendarViewController> controller_;
  static base::Time fake_time_;
};

base::Time CalendarMonthViewTest::fake_time_;

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

  CreateMonthView(date);

  // Randomly checks some dates in this month view.
  EXPECT_EQ(
      u"1",
      static_cast<views::LabelButton*>(month_view()->children()[0])->GetText());
  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(month_view()->children()[30])
                ->GetText());
  EXPECT_EQ(u"4", static_cast<views::LabelButton*>(month_view()->children()[34])
                      ->GetText());

  // Create a monthview based on Jun,1st 2021, which has the previous month's
  // dates in the first row.
  // 30, 31, 1 , 2 , 3 , 4 , 5
  // 6 , 7 , 8 , 9 , 10, 11, 12
  // 13, 14, 15, 16, 17, 18, 19
  // 20, 21, 22, 23, 24, 25, 26
  // 27, 28, 29, 30, 1 , 2 , 3
  base::Time jun_date;
  ASSERT_TRUE(base::Time::FromString("1 Jun 2021 10:00 GMT", &jun_date));

  CreateMonthView(jun_date);

  // Randomly checks some dates in this month view.
  EXPECT_EQ(
      u"30",
      static_cast<views::LabelButton*>(month_view()->children()[0])->GetText());
  EXPECT_EQ(u"29",
            static_cast<views::LabelButton*>(month_view()->children()[30])
                ->GetText());
  EXPECT_EQ(u"3", static_cast<views::LabelButton*>(month_view()->children()[34])
                      ->GetText());
}

// Tests that todays row position is not updated when today is not in the month.
TEST_F(CalendarMonthViewTest, TodayNotInMonth) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is not in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("17 Sep 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarMonthViewTest::FakeTimeNow, nullptr, nullptr);

  CreateMonthView(date);

  // Today's row position is not updated.
  EXPECT_EQ(0, controller()->GetTodayRowTopHeight());
  EXPECT_EQ(0, controller()->GetTodayRowBottomHeight());
}

// The position of today should be updated when today is in the month.
TEST_F(CalendarMonthViewTest, TodayInMonth) {
  // Create a monthview based on Aug,1st 2021. Today is set to 17th.
  // 1 , 2 , 3 ,   4 , 5 , 6 , 7
  // 8 , 9 , 10,   11, 12, 13, 14
  // 15, 16, [17], 18, 19, 20, 21
  // 22, 23, 24,   25, 26, 27, 28
  // 29, 30, 31,   1 , 2 , 3 , 4

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("17 Aug 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides in_month_time_override(
      &CalendarMonthViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateMonthView(date);

  // Today's row position is updated because today is in this month.
  int top = controller()->GetTodayRowTopHeight();
  int bottom = controller()->GetTodayRowBottomHeight();
  EXPECT_NE(0, top);
  EXPECT_NE(0, bottom);

  // The date 17th is on the 3rd row.
  EXPECT_EQ(3, bottom / (bottom - top));
}

}  // namespace ash
