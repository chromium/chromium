// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
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
    delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);
    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(/*shelf=*/nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
  }

  void TearDown() override {
    calendar_view_.reset();
    controller_.reset();
    delegate_.reset();
    tray_controller_.reset();
    tray_model_.reset();

    AshTestBase::TearDown();
  }

  void CreateCalendarView(base::Time date) {
    calendar_view_.reset();
    controller_->UpdateMonth(date);
    calendar_view_ = std::make_unique<CalendarView>(
        delegate_.get(), tray_controller_.get(), controller_.get());
    // TODO(https://crbug.com/1236276): remove calling `Layout()` once we can
    // pop up the view from the tray. (https://crbug.com/1254491) And add tests
    // for focusing behaviors.
    calendar_view_->Layout();
  }

  CalendarView* calendar_view() { return calendar_view_.get(); }
  views::ScrollView* scroll_view_() { return calendar_view_->scroll_view_; }

  views::View* previous_label() { return calendar_view_->previous_label_; }
  views::View* current_label() { return calendar_view_->current_label_; }
  views::View* next_label() { return calendar_view_->next_label_; }

  std::u16string GetPreviousLabelText() {
    std::u16string month_text =
        static_cast<views::Label*>(previous_label()->children()[0])->GetText();
    if (previous_label()->children().size() > 1) {
      month_text += static_cast<views::Label*>(previous_label()->children()[1])
                        ->GetText();
    }
    return month_text;
  }
  std::u16string GetCurrentLabelText() {
    std::u16string month_text =
        static_cast<views::Label*>(current_label()->children()[0])->GetText();
    if (current_label()->children().size() > 1) {
      month_text +=
          static_cast<views::Label*>(current_label()->children()[1])->GetText();
    }
    return month_text;
  }
  std::u16string GetNextLabelText() {
    std::u16string month_text =
        static_cast<views::Label*>(next_label()->children()[0])->GetText();
    if (next_label()->children().size() > 1) {
      month_text +=
          static_cast<views::Label*>(next_label()->children()[1])->GetText();
    }
    return month_text;
  }
  CalendarMonthView* previous_month() {
    return calendar_view_->previous_month_;
  }
  CalendarMonthView* current_month() { return calendar_view_->current_month_; }
  CalendarMonthView* next_month() { return calendar_view_->next_month_; }

  views::Label* header_() { return calendar_view_->header_; }
  views::Label* header_year_() { return calendar_view_->header_year_; }

  void ScrollUpOneMonth() { calendar_view_->ScrollUpOneMonthAndAutoScroll(); }
  void ScrollDownOneMonth() {
    calendar_view_->ScrollDownOneMonthAndAutoScroll();
  }
  void ResetToToday() { calendar_view_->ResetToToday(); }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  std::unique_ptr<CalendarView> calendar_view_;
  std::unique_ptr<CalendarViewController> controller_;
  std::unique_ptr<DetailedViewDelegate> delegate_;
  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  static base::Time fake_time_;
};

base::Time CalendarViewTest::fake_time_;

// Test the init view of the `CalendarView`.
TEST_F(CalendarViewTest, Init) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));
  CreateCalendarView(date);

  EXPECT_EQ(u"July", GetPreviousLabelText());
  EXPECT_EQ(u"August", GetCurrentLabelText());
  EXPECT_EQ(u"September", GetNextLabelText());
  EXPECT_EQ(u"August", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

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

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January2022", GetNextLabelText());
  EXPECT_EQ(u"December", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

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

TEST_F(CalendarViewTest, Scroll) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  CreateCalendarView(date);

  EXPECT_EQ(u"September", GetPreviousLabelText());
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"November", GetNextLabelText());
  EXPECT_EQ(u"October", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  // Give it a number which is larger than the height of a month view, so it
  // can scroll to the next month. Same for the other places where use 400 as
  // the target position.
  scroll_view_()->ScrollToPosition(scroll_view_()->vertical_scroll_bar(), 400);

  EXPECT_EQ(u"October", GetPreviousLabelText());
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"December", GetNextLabelText());
  EXPECT_EQ(u"November", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  scroll_view_()->ScrollToPosition(scroll_view_()->vertical_scroll_bar(), 400);

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January2022", GetNextLabelText());
  EXPECT_EQ(u"December", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  scroll_view_()->ScrollToPosition(scroll_view_()->vertical_scroll_bar(), 400);

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January2022", GetCurrentLabelText());
  EXPECT_EQ(u"February2022", GetNextLabelText());
  EXPECT_EQ(u"January", header_()->GetText());
  EXPECT_EQ(u"2022", header_year_()->GetText());
}

// Tests the up, down, and reset_to_today button callback functions.
TEST_F(CalendarViewTest, ButtonFunctions) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView(date);

  EXPECT_EQ(u"September", GetPreviousLabelText());
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"November", GetNextLabelText());
  EXPECT_EQ(u"October", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"October", GetPreviousLabelText());
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"December", GetNextLabelText());
  EXPECT_EQ(u"November", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January2022", GetNextLabelText());
  EXPECT_EQ(u"December", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January2022", GetCurrentLabelText());
  EXPECT_EQ(u"February2022", GetNextLabelText());
  EXPECT_EQ(u"January", header_()->GetText());
  EXPECT_EQ(u"2022", header_year_()->GetText());

  ScrollUpOneMonth();

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January2022", GetNextLabelText());
  EXPECT_EQ(u"December", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January2022", GetCurrentLabelText());
  EXPECT_EQ(u"February2022", GetNextLabelText());
  EXPECT_EQ(u"January", header_()->GetText());
  EXPECT_EQ(u"2022", header_year_()->GetText());

  // Goes back to the landing view.
  ResetToToday();

  EXPECT_EQ(u"September", GetPreviousLabelText());
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"November", GetNextLabelText());
  EXPECT_EQ(u"October", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());
}

}  // namespace ash
