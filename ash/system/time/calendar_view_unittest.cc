// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/shell.h"
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

    delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);
    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(/*shelf=*/nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    delegate_.reset();
    tray_controller_.reset();
    tray_model_.reset();

    AshTestBase::TearDown();
  }

  void CreateCalendarView() {
    auto calendar_view =
        std::make_unique<CalendarView>(delegate_.get(), tray_controller_.get());

    calendar_view_ = widget_->SetContentsView(std::move(calendar_view));
  }

  CalendarView* calendar_view() { return calendar_view_; }
  views::ScrollView* scroll_view_() { return calendar_view_->scroll_view_; }

  views::View* previous_label() { return calendar_view_->previous_label_; }
  views::View* current_label() { return calendar_view_->current_label_; }
  views::View* next_label() { return calendar_view_->next_label_; }

  // The position of the `next_month_`.
  int NextMonthPosition() {
    return previous_label()->GetPreferredSize().height() +
           calendar_view_->previous_month_->GetPreferredSize().height() +
           current_label()->GetPreferredSize().height() +
           calendar_view_->current_month_->GetPreferredSize().height();
  }

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

  views::Button* reset_to_today_button() {
    return calendar_view_->reset_to_today_button_;
  }
  views::Button* settings_button() { return calendar_view_->settings_button_; }
  TopShortcutButton* up_button() { return calendar_view_->up_button_; }
  TopShortcutButton* down_button() { return calendar_view_->down_button_; }

  void ScrollUpOneMonth() { calendar_view_->ScrollUpOneMonthAndAutoScroll(); }
  void ScrollDownOneMonth() {
    calendar_view_->ScrollDownOneMonthAndAutoScroll();
  }
  void ResetToToday() { calendar_view_->ResetToToday(); }

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  }

  void PressUp() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_UP, ui::EventFlags::EF_NONE);
  }

  void PressDown() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_DOWN, ui::EventFlags::EF_NONE);
  }

  void PressLeft() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_LEFT, ui::EventFlags::EF_NONE);
  }

  void PressRight() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_RIGHT, ui::EventFlags::EF_NONE);
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  std::unique_ptr<views::Widget> widget_;
  // Owned by `widget_`.
  CalendarView* calendar_view_ = nullptr;
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

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

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
}

// Test the init view of the `CalendarView` starting with December.
TEST_F(CalendarViewTest, InitDec) {
  base::Time dec_date;
  ASSERT_TRUE(base::Time::FromString("24 Dec 2021 10:00 GMT", &dec_date));

  // Set time override.
  SetFakeNow(dec_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

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

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  EXPECT_EQ(u"September", GetPreviousLabelText());
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"November", GetNextLabelText());
  EXPECT_EQ(u"October", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  // Scrolls to the next month.
  scroll_view_()->ScrollToPosition(scroll_view_()->vertical_scroll_bar(),
                                   NextMonthPosition());

  EXPECT_EQ(u"October", GetPreviousLabelText());
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"December", GetNextLabelText());
  EXPECT_EQ(u"November", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  scroll_view_()->ScrollToPosition(scroll_view_()->vertical_scroll_bar(),
                                   NextMonthPosition());

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January2022", GetNextLabelText());
  EXPECT_EQ(u"December", header_()->GetText());
  EXPECT_EQ(u"2021", header_year_()->GetText());

  scroll_view_()->ScrollToPosition(scroll_view_()->vertical_scroll_bar(),
                                   NextMonthPosition());

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

  CreateCalendarView();

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

// For all the Focusing tests below, Jun, 2021 is used.
// 30, 31, 1 , 2 , 3 , 4 , 5
// 6 , 7 , 8 , 9 , 10, 11, 12
// 13, 14, 15, 16, 17, 18, 19
// 20, 21, 22, 23, 24, 25, 26
// 27, 28, 29, 30, 1 , 2 , 3
TEST_F(CalendarViewTest, HeaderFocusing) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  // Generates a tab key press. Should focus on back button.
  PressTab();

  // Moves to the next focusable view. Today's button.
  PressTab();
  auto* focus_manager = calendar_view()->GetFocusManager();
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());

  // Moves to settings button.
  PressTab();
  EXPECT_EQ(settings_button(), focus_manager->GetFocusedView());

  // Moves to down button.
  PressTab();
  EXPECT_EQ(down_button(), focus_manager->GetFocusedView());

  // Moves to up button.
  PressTab();
  EXPECT_EQ(up_button(), focus_manager->GetFocusedView());
}

// Tests the focus loop between the back button, today's button, settings
// button, and the todays date in the month.
TEST_F(CalendarViewTest, FocusingToDateCell) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  // Generates a tab key press. Should focus on back button.
  PressTab();

  // Moves to the next focusable view. Today's button.
  PressTab();
  auto* focus_manager = calendar_view()->GetFocusManager();
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());

  PressTab();  // Settings button.
  PressTab();  // Moves to down button.
  PressTab();  // Moves to up button.

  // Moves to the the 7th date cell, which is the date of "today".
  PressTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressTab();  // Moves to back button again.
  PressTab();  // Moves to Today's button.
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());

  PressTab();  // Moves to settings button.
  PressTab();  // Moves to down button.
  PressTab();  // Moves to up button.

  // Moves to the the 7th date cell, which is the date of "today".
  PressTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
}

TEST_F(CalendarViewTest, MonthViewFocusing) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  // Generates a tab key press.
  PressTab();  // Focusing on the back button.
  PressTab();  // Today's button.
  PressTab();  // Settings button.
  PressTab();  // Moves to down button.
  PressTab();  // Moves to up button.

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Moves to the the 7th date cell, which is the date of "today".
  PressTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  // Tapping on arrow keys should start navigating inside the month view.
  PressUp();
  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressDown();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressLeft();
  EXPECT_EQ(u"6",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressLeft();
  EXPECT_EQ(u"5",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressDown();
  EXPECT_EQ(u"12",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressRight();
  EXPECT_EQ(u"13",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressRight();
  EXPECT_EQ(u"14",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressRight();
  EXPECT_EQ(u"15",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
}

// Should be able to use the arrow keys to navigate to the previous months or
// next months.
TEST_F(CalendarViewTest, FocusingToNavigate) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  // Generates a tab key press.
  PressTab();  // Focusing on the back button.
  PressTab();  // Today's button.
  PressTab();  // Settings button.
  PressTab();  // Moves to down button.
  PressTab();  // Moves to up button.

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Moves to the the 7th date cell, which is the date of "today".
  PressTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"June", GetCurrentLabelText());

  // Tapping on arrow keys should start navigating inside the month view.
  PressUp();
  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"May", GetCurrentLabelText());

  PressUp();
  EXPECT_EQ(u"24",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressUp();
  EXPECT_EQ(u"17",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressUp();
  EXPECT_EQ(u"10",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  PressUp();
  EXPECT_EQ(u"3",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"May", GetCurrentLabelText());

  PressUp();
  EXPECT_EQ(u"26",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"April", GetCurrentLabelText());
}

}  // namespace ash
