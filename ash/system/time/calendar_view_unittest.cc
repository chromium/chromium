// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

namespace ash {

class CalendarViewTest : public AshTestBase {
 public:
  CalendarViewTest() = default;
  CalendarViewTest(const CalendarViewTest&) = delete;
  CalendarViewTest& operator=(const CalendarViewTest&) = delete;
  ~CalendarViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<CalendarViewController>();
    delegate_ = std::make_unique<DetailedViewDelegate>(nullptr);
  }

  void TearDown() override {
    calendar_view_.reset();
    controller_.reset();
    delegate_.reset();

    AshTestBase::TearDown();
  }

  void CreateCalendarView(base::Time date) {
    calendar_view_.reset();
    controller_.reset();
    controller_ = std::make_unique<CalendarViewController>();
    controller_->UpdateMonth(date);
    calendar_view_ =
        std::make_unique<CalendarView>(delegate_.get(), controller_.get());
    calendar_view_->Init();
  }

  CalendarView* calendar_view() { return calendar_view_.get(); }

  views::Label* previous_label() { return calendar_view_->previous_label_; }
  views::Label* current_label() { return calendar_view_->current_label_; }
  views::Label* next_label() { return calendar_view_->next_label_; }
  CalendarMonthView* previous_month() {
    return calendar_view_->previous_month_;
  }
  CalendarMonthView* current_month() { return calendar_view_->current_month_; }
  CalendarMonthView* next_month() { return calendar_view_->next_month_; }

 private:
  std::unique_ptr<CalendarView> calendar_view_;
  std::unique_ptr<CalendarViewController> controller_;
  std::unique_ptr<DetailedViewDelegate> delegate_;
};

// Test the init view of the `CalendarView`.
TEST_F(CalendarViewTest, Init) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));
  CreateCalendarView(date);

  EXPECT_EQ(u"July", previous_label()->GetText());
  EXPECT_EQ(u"August", current_label()->GetText());
  EXPECT_EQ(u"September", next_label()->GetText());

  EXPECT_EQ(u"27",
            static_cast<views::LabelButton*>(previous_month()->children()[0])
                ->GetText());
  EXPECT_EQ(u"1",
            static_cast<views::LabelButton*>(current_month()->children()[0])
                ->GetText());
  EXPECT_EQ(
      u"29",
      static_cast<views::LabelButton*>(next_month()->children()[0])->GetText());

  // Test corner cases of the `CalendarView`.
  base::Time dec_date;
  ASSERT_TRUE(base::Time::FromString("24 Dec 2021 10:00 GMT", &dec_date));
  CreateCalendarView(dec_date);

  EXPECT_EQ(u"November", previous_label()->GetText());
  EXPECT_EQ(u"December", current_label()->GetText());
  EXPECT_EQ(u"January", next_label()->GetText());

  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(previous_month()->children()[0])
                ->GetText());
  EXPECT_EQ(u"28",
            static_cast<views::LabelButton*>(current_month()->children()[0])
                ->GetText());
  EXPECT_EQ(
      u"26",
      static_cast<views::LabelButton*>(next_month()->children()[0])->GetText());
}

}  // namespace ash
