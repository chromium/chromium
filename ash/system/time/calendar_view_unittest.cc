// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/icon_button.h"
#include "ash/system/time/calendar_event_list_view.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

namespace {

constexpr char kTestUser[] = "user@test";

}  // namespace

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
    tray_model_ =
        base::MakeRefCounted<UnifiedSystemTrayModel>(/*shelf=*/nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    controller_ = std::make_unique<CalendarViewController>();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    delegate_.reset();
    tray_controller_.reset();
    controller_.reset();
    tray_model_.reset();

    AshTestBase::TearDown();
  }

  void CreateEventListView(base::Time date) {
    event_list_view_.reset();
    controller_->selected_date_ = date;
    event_list_view_ =
        std::make_unique<CalendarEventListView>(controller_.get());
  }

  void CreateCalendarView() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);

    auto calendar_view =
        std::make_unique<CalendarView>(delegate_.get(), tray_controller_.get());

    calendar_view_ = widget_->SetContentsView(std::move(calendar_view));
  }

  void DestroyCalendarViewWidget() { widget_.reset(); }

  void DestroyEventListView() { event_list_view_.reset(); }

  CalendarView* calendar_view() { return calendar_view_; }
  views::ScrollView* scroll_view() { return calendar_view_->scroll_view_; }

  views::View* previous_label() { return calendar_view_->previous_label_; }
  views::View* current_label() { return calendar_view_->current_label_; }
  views::View* next_label() { return calendar_view_->next_label_; }
  views::View* next_next_label() { return calendar_view_->next_next_label_; }

  views::ScrollView::ScrollBarMode GetScrollBarMode() {
    return scroll_view()->GetVerticalScrollBarMode();
  }

  // The position of the `next_month_`.
  int NextMonthPosition() {
    return previous_label()->GetPreferredSize().height() +
           calendar_view_->previous_month_->GetPreferredSize().height() +
           current_label()->GetPreferredSize().height() +
           calendar_view_->current_month_->GetPreferredSize().height() +
           next_label()->GetPreferredSize().height();
  }

  std::u16string GetPreviousLabelText() {
    return static_cast<views::Label*>(previous_label()->children()[0])
        ->GetText();
  }
  std::u16string GetCurrentLabelText() {
    return static_cast<views::Label*>(current_label()->children()[0])
        ->GetText();
  }
  std::u16string GetNextLabelText() {
    return static_cast<views::Label*>(next_label()->children()[0])->GetText();
  }
  std::u16string GetNextNextLabelText() {
    return static_cast<views::Label*>(next_next_label()->children()[0])
        ->GetText();
  }
  CalendarMonthView* previous_month() {
    return calendar_view_->previous_month_;
  }
  CalendarMonthView* current_month() { return calendar_view_->current_month_; }
  CalendarMonthView* next_month() { return calendar_view_->next_month_; }
  CalendarMonthView* next_next_month() {
    return calendar_view_->next_next_month_;
  }

  views::Label* month_header() { return calendar_view_->header_->header_; }
  views::Label* header_year() { return calendar_view_->header_->header_year_; }

  views::Button* reset_to_today_button() {
    return calendar_view_->reset_to_today_button_;
  }
  views::Button* settings_button() { return calendar_view_->settings_button_; }
  IconButton* up_button() { return calendar_view_->up_button_; }
  IconButton* down_button() { return calendar_view_->down_button_; }
  views::ImageButton* close_button() {
    return calendar_view_->event_list_view_->close_button_;
  }
  views::View* event_list_view() { return calendar_view_->event_list_view_; }

  void ScrollUpOneMonth() {
    calendar_view_->ScrollOneMonthAndAutoScroll(/*scroll_up=*/true);
  }
  void ScrollDownOneMonth() {
    calendar_view_->ScrollOneMonthAndAutoScroll(/*scroll_up=*/false);
  }
  void ResetToToday() { calendar_view_->ResetToToday(); }

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  }

  void PressShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  }

  void PressEnter() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EventFlags::EF_NONE);
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
  std::unique_ptr<DetailedViewDelegate> delegate_;
  scoped_refptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  std::unique_ptr<CalendarViewController> controller_;
  std::unique_ptr<CalendarEventListView> event_list_view_;
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
  EXPECT_EQ(u"October", GetNextNextLabelText());
  EXPECT_EQ(u"August", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  EXPECT_EQ(u"27",
            static_cast<views::LabelButton*>(previous_month()->children()[0])
                ->GetText());
  EXPECT_EQ(u"1",
            static_cast<views::LabelButton*>(current_month()->children()[0])
                ->GetText());
  EXPECT_EQ(
      u"29",
      static_cast<views::LabelButton*>(next_month()->children()[0])->GetText());
  EXPECT_EQ(u"26",
            static_cast<views::LabelButton*>(next_next_month()->children()[0])
                ->GetText());
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
  EXPECT_EQ(u"January", GetNextLabelText());
  EXPECT_EQ(u"February", GetNextNextLabelText());
  EXPECT_EQ(u"December", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(previous_month()->children()[0])
                ->GetText());
  EXPECT_EQ(u"28",
            static_cast<views::LabelButton*>(current_month()->children()[0])
                ->GetText());
  EXPECT_EQ(u"30",
            static_cast<views::LabelButton*>(next_next_month()->children()[0])
                ->GetText());
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
  EXPECT_EQ(u"December", GetNextNextLabelText());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Scrolls to the next month.
  scroll_view()->ScrollToPosition(scroll_view()->vertical_scroll_bar(),
                                  NextMonthPosition());

  EXPECT_EQ(u"October", GetPreviousLabelText());
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"December", GetNextLabelText());
  EXPECT_EQ(u"January", GetNextNextLabelText());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  scroll_view()->ScrollToPosition(scroll_view()->vertical_scroll_bar(),
                                  NextMonthPosition());

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January", GetNextLabelText());
  EXPECT_EQ(u"February", GetNextNextLabelText());
  EXPECT_EQ(u"December", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  scroll_view()->ScrollToPosition(scroll_view()->vertical_scroll_bar(),
                                  NextMonthPosition());

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January", GetCurrentLabelText());
  EXPECT_EQ(u"February", GetNextLabelText());
  EXPECT_EQ(u"March", GetNextNextLabelText());
  EXPECT_EQ(u"January", month_header()->GetText());
  EXPECT_EQ(u"2022", header_year()->GetText());
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
  EXPECT_EQ(u"December", GetNextNextLabelText());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"October", GetPreviousLabelText());
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"December", GetNextLabelText());
  EXPECT_EQ(u"January", GetNextNextLabelText());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January", GetNextLabelText());
  EXPECT_EQ(u"February", GetNextNextLabelText());
  EXPECT_EQ(u"December", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January", GetCurrentLabelText());
  EXPECT_EQ(u"February", GetNextLabelText());
  EXPECT_EQ(u"January", month_header()->GetText());
  EXPECT_EQ(u"2022", header_year()->GetText());

  ScrollUpOneMonth();

  EXPECT_EQ(u"November", GetPreviousLabelText());
  EXPECT_EQ(u"December", GetCurrentLabelText());
  EXPECT_EQ(u"January", GetNextLabelText());
  EXPECT_EQ(u"December", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  ScrollDownOneMonth();

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January", GetCurrentLabelText());
  EXPECT_EQ(u"February", GetNextLabelText());
  EXPECT_EQ(u"March", GetNextNextLabelText());
  EXPECT_EQ(u"January", month_header()->GetText());
  EXPECT_EQ(u"2022", header_year()->GetText());

  // Goes back to the landing view.
  ResetToToday();

  EXPECT_EQ(u"September", GetPreviousLabelText());
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"November", GetNextLabelText());
  EXPECT_EQ(u"December", GetNextNextLabelText());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Multiple clicking on the up/down buttons.
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  EXPECT_EQ(u"May", GetPreviousLabelText());
  EXPECT_EQ(u"June", GetCurrentLabelText());
  EXPECT_EQ(u"July", GetNextLabelText());
  EXPECT_EQ(u"August", GetNextNextLabelText());
  EXPECT_EQ(u"June", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  EXPECT_EQ(u"September", GetPreviousLabelText());
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"November", GetNextLabelText());
  EXPECT_EQ(u"December", GetNextNextLabelText());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
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

  // Generates a tab key press. Should focus on today's button.
  PressTab();

  // Moves to the back button.
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

  auto* focus_manager = calendar_view()->GetFocusManager();

  // Generates a tab key press. Should focus on today's cell.
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

// Used to determine whether focus goes directly to the proper CalendarDateCell
// prior to moving on to the EventListView.
class DateCellFocusChangeListener : public views::FocusChangeListener {
 public:
  DateCellFocusChangeListener(views::FocusManager* focus_manager,
                              std::u16string looking_for,
                              int steps_to_find)
      : focus_manager_(focus_manager),
        looking_for_(looking_for),
        steps_to_find_(steps_to_find) {
    focus_manager_->AddFocusChangeListener(this);
  }
  DateCellFocusChangeListener(const DateCellFocusChangeListener& other) =
      delete;
  DateCellFocusChangeListener& operator=(
      const DateCellFocusChangeListener& other) = delete;
  ~DateCellFocusChangeListener() override {
    focus_manager_->RemoveFocusChangeListener(this);
    EXPECT_EQ(steps_taken_, steps_to_find_);
  }

  bool found() const { return found_; }

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (found_)
      return;

    steps_taken_++;
    found_ = static_cast<const views::LabelButton*>(focused_now)->GetText() ==
             looking_for_;
    DCHECK_LE(steps_taken_, steps_to_find_);
  }

 private:
  // Whether a `views::Labelbutton` matching `looking_for_` was focused.
  bool found_ = false;
  // How many focus changes have occurred so far.
  int steps_taken_ = 0;

  // Unowned.
  views::FocusManager* const focus_manager_;
  // The string being looked for.
  const std::u16string looking_for_;
  // The number of steps it is acceptable to have made before finding the
  // appropriate view.
  const int steps_to_find_;
};

// Tests that keyboard focus movement mixed with non-keyboard date cell
// activation results in proper focus directly to the date cell.
TEST_F(CalendarViewTest, MixedInput) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  auto* focus_manager = calendar_view()->GetFocusManager();

  // Generates a tab key press. Should focus on today's cell.
  PressTab();
  ASSERT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  const views::LabelButton* non_focused_date_cell_view = nullptr;
  for (const auto* child_view : current_month()->children()) {
    auto* date_cell_view = static_cast<const views::LabelButton*>(child_view);
    if (u"9" != date_cell_view->GetText())
      continue;

    non_focused_date_cell_view = date_cell_view;
    break;
  }

  {
    auto focus_change_listener = DateCellFocusChangeListener(
        focus_manager, /*looking_for=*/u"9", /*steps_to_find=*/1);
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        non_focused_date_cell_view->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
    EXPECT_TRUE(focus_change_listener.found());
  }
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

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Focus on the the 7th date cell, which is the date of "today".
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

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Focus on the the 7th date cell, which is the date of "today".
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

TEST_F(CalendarViewTest, ExpandableViewFocusing) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  EXPECT_EQ(views::ScrollView::ScrollBarMode::kHiddenButEnabled,
            GetScrollBarMode());

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Moves to the the 7th date cell, which is the date of "today".
  PressTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"June", GetCurrentLabelText());

  // Opens the event list.
  PressEnter();
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kDisabled, GetScrollBarMode());

  // Focus moves to the event list close button.
  EXPECT_EQ(close_button(), focus_manager->GetFocusedView());

  // Focus moves back to the date cell.
  PressShiftTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"June", GetCurrentLabelText());

  // Tapping on up arrow keys should go to the previous month, which mens the
  // scroll bar is enabled during the key pressed.
  PressUp();
  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  EXPECT_EQ(u"May", GetCurrentLabelText());
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kDisabled, GetScrollBarMode());

  // Moves to the event list.
  PressTab();
  EXPECT_EQ(close_button(), focus_manager->GetFocusedView());

  // Goes to empty list view.
  PressTab();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  // Goes back to back button.
  PressTab();

  // Moves to the next focusable view. Today's button.
  PressTab();
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());
}

// Tests that the metric is recorded when calling `UpdateMonth`.
TEST_F(CalendarViewTest, RecordDwellTimeMetric) {
  base::HistogramTester histogram_tester;
  CreateCalendarView();

  // Does not record metric if `UpdateMonth` is called with same month as the
  // one in `current_date_`.
  calendar_view()->calendar_view_controller()->UpdateMonth(base::Time::Now());
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/0);

  // Records metric when `UpdateMonth` is called with a month different than the
  // one in `current_date_`.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));
  calendar_view()->calendar_view_controller()->UpdateMonth(date);
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/1);
}

TEST_F(CalendarViewTest, OnSessionBlocked) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  // The third child cell in the month, which is the non-grayed out 1st date
  // cell.
  EXPECT_EQ(u"1",
            static_cast<views::LabelButton*>(current_month()->children()[2])
                ->GetText());

  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[2]));
  bool is_showing_event_list_view = event_list_view();
  EXPECT_FALSE(is_showing_event_list_view);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_EQ(u"1",
            static_cast<views::LabelButton*>(current_month()->children()[2])
                ->GetText());

  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[2]));

  is_showing_event_list_view = event_list_view();
  EXPECT_TRUE(is_showing_event_list_view);
  GestureTapOn(close_button());
}

// Tests multiple scenarios that should record the metric when scrolling.
TEST_F(CalendarViewTest, RecordDwellTimeMetricWhenScrolling) {
  base::HistogramTester histogram_tester;
  CreateCalendarView();

  // Scroll up two times.
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/2);

  // Scroll down three times.
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/5);

  // Reset to today.
  ResetToToday();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/6);

  // Opening and closing event list view does not record the metric.
  CreateEventListView(base::Time::Now());
  DestroyEventListView();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/6);

  // Closing the calendar view should record the metric.
  DestroyCalendarViewWidget();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/7);
}

// A test class for testing animation. This class cannot set fake now since it's
// using `MOCK_TIME` to test the animations.
class CalendarViewAnimationTest : public AshTestBase {
 public:
  CalendarViewAnimationTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  CalendarViewAnimationTest(const CalendarViewAnimationTest&) = delete;
  CalendarViewAnimationTest& operator=(const CalendarViewAnimationTest&) =
      delete;
  ~CalendarViewAnimationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);
    tray_model_ =
        base::MakeRefCounted<UnifiedSystemTrayModel>(/*shelf=*/nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    delegate_.reset();
    tray_controller_.reset();
    tray_model_.reset();
    widget_.reset();

    AshTestBase::TearDown();
  }

  void CreateCalendarView() {
    calendar_view_ = widget_->SetContentsView(std::make_unique<CalendarView>(
        delegate_.get(), tray_controller_.get()));
  }

  void UpdateMonth(base::Time date) {
    calendar_view_->calendar_view_controller()->UpdateMonth(date);
    calendar_view_->content_view_->RemoveAllChildViews();
    calendar_view_->SetMonthViews();
  }

  CalendarView* calendar_view() { return calendar_view_; }

  views::Label* month_header() { return calendar_view_->header_->header_; }
  views::Label* header_year() { return calendar_view_->header_->header_year_; }
  CalendarHeaderView* header() { return calendar_view_->header_; }
  CalendarMonthView* current_month() { return calendar_view_->current_month_; }
  CalendarMonthView* previous_month() {
    return calendar_view_->previous_month_;
  }
  CalendarMonthView* next_month() { return calendar_view_->next_month_; }
  views::View* previous_label() { return calendar_view_->previous_label_; }
  views::View* current_label() { return calendar_view_->current_label_; }
  views::View* next_label() { return calendar_view_->next_label_; }
  views::ScrollView* scroll_view() { return calendar_view_->scroll_view_; }
  views::ScrollView::ScrollBarMode GetScrollBarMode() {
    return scroll_view()->GetVerticalScrollBarMode();
  }

  // The position of the `next_month_`.
  int NextMonthPosition() {
    return previous_label()->GetPreferredSize().height() +
           calendar_view_->previous_month_->GetPreferredSize().height() +
           current_label()->GetPreferredSize().height() +
           calendar_view_->current_month_->GetPreferredSize().height() +
           next_label()->GetPreferredSize().height();
  }

  bool is_scrolling_up() { return calendar_view_->is_scrolling_up_; }

  void ScrollUpOneMonth() {
    calendar_view_->ScrollOneMonthWithAnimation(/*scroll_up=*/true);
  }
  void ScrollDownOneMonth() {
    calendar_view_->ScrollOneMonthWithAnimation(/*scroll_up=*/false);
  }

  void ResetToTodayWithAnimation() {
    calendar_view_->ResetToTodayWithAnimation();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  // Owned by `widget_`.
  CalendarView* calendar_view_ = nullptr;
  std::unique_ptr<DetailedViewDelegate> delegate_;
  scoped_refptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
};

// The header should show the new header with animation when there's an update.
TEST_F(CalendarViewAnimationTest, HeaderAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  CreateCalendarView();
  // Gives it a duration to let the animation finish and pass the cool down
  // duration. The same for the other 3s duration.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  calendar_view()->calendar_view_controller()->UpdateMonth(date);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Update the header to next month.
  calendar_view()->calendar_view_controller()->UpdateMonth(date +
                                                           base::Days(33));

  // To prevent flakiness, fast forward until the header changes (see
  // crbug/1270161). The second animation starts after the header is updated to
  // the new month.
  while (u"November" != month_header()->GetText()) {
    task_environment()->FastForwardBy(base::Milliseconds(30));
  }
  // The opacity is updated to 0 after the first animation ends.
  EXPECT_EQ(0.0f, header()->layer()->opacity());

  // Now the header is updated to the new month and year before it starts
  // showing up.
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // The Opacity is back from 0.0f to 1.0 after 200ms delay duration.
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForVisibility);
  EXPECT_EQ(0.0f, header()->layer()->opacity());

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  // The header is still with the updated new month after all animation
  // finished.
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
}

// The month views and header should animate when scrolling up or down.
TEST_F(CalendarViewAnimationTest, MonthAndHeaderAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  CreateCalendarView();
  // Gives it a duration to let the animation finish and pass the cool down
  // duration.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  UpdateMonth(date);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Scrolls to the next month.
  ScrollDownOneMonth();

  EXPECT_FALSE(is_scrolling_up());

  // If scrolls down, the month views and labels will be animating.
  EXPECT_EQ(1.0f, header()->layer()->opacity());
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForVisibility);
  EXPECT_TRUE(current_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(current_label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // The header animation starts from 300ms. Its Opacity is updated from 1.0f to
  // 0.0f after 300+200ms delay duration.
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForMoving);
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  // To prevent flakiness, fast forward until the header changes (see
  // crbug/1270161). The second animation starts after the header is updated to
  // the new month.
  while (u"November" != month_header()->GetText()) {
    task_environment()->FastForwardBy(base::Milliseconds(30));
  }

  // The opacity is updated to 0 after the first animation ends.
  EXPECT_EQ(0.0f, header()->layer()->opacity());

  // Now the header is updated to the new month and year before it starts
  // showing up.
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // The Opacity is back from 0.0f to 1.0 after 200ms delay duration.
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForVisibility);
  EXPECT_EQ(0.0f, header()->layer()->opacity());

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  // The header is still with the updated new month after all animation
  // finished.
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Scrolls to the previous month.
  ScrollUpOneMonth();

  EXPECT_TRUE(is_scrolling_up());

  // If scrolls up, the month views and labels will be animating.
  EXPECT_EQ(1.0f, header()->layer()->opacity());
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForVisibility);
  EXPECT_TRUE(current_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(current_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // The header animation starts from 300ms. Its Opacity is updated from 1.0f to
  // 0.0f after 300+200ms delay duration.
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForMoving);
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Multiple clicking on the up/down buttons. Here only checks the label since
  // if it scrolls too fast some scroll actions might be skipped due to
  // `is_resetting_scroll_` == true.
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_EQ(u"2021", header_year()->GetText());

  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_EQ(u"2021", header_year()->GetText());
}

// The content view should not be scrollable when the month view is animating.
TEST_F(CalendarViewAnimationTest, NotScrollableWhenAnimating) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  CreateCalendarView();
  // Gives it a duration to let the animation finish and pass the cool down
  // duration.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  UpdateMonth(date);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // The scrll bar is enaled before tapping on the up button.
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kHiddenButEnabled,
            GetScrollBarMode());

  // Scrolls to the previous month.
  ScrollUpOneMonth();

  // If scrolls down, the month views and labels will be animating.
  EXPECT_EQ(1.0f, header()->layer()->opacity());
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForVisibility);
  EXPECT_TRUE(current_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(current_label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Try to scrol to the next month.
  scroll_view()->ScrollToPosition(scroll_view()->vertical_scroll_bar(),
                                  NextMonthPosition());

  // Should not scroll and keep showing the animation.
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kDisabled, GetScrollBarMode());
  EXPECT_TRUE(current_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(current_label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Animation finished. On the previous month.
  EXPECT_EQ(u"September", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kHiddenButEnabled,
            GetScrollBarMode());

  // Try to scroll to the next month. Should get to the next month.
  scroll_view()->ScrollToPosition(scroll_view()->vertical_scroll_bar(),
                                  NextMonthPosition());
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kHiddenButEnabled,
            GetScrollBarMode());
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
}

// TODO(crbug.com/1298314) flaky test
TEST_F(CalendarViewAnimationTest, DISABLED_ResetToTodayWithAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Creates calendar view and waits for the creation animation to finish.
  CreateCalendarView();
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Expect header visible before starting animation.
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  // Fades out on-screen date, and fades in today's date.
  ResetToTodayWithAnimation();

  // Expect header opacity less than 1 when fading out on-screen date.
  task_environment()->FastForwardBy(
      calendar_utils::kResetToTodayFadeAnimationDuration);
  EXPECT_GT(1.0f, header()->layer()->opacity());

  // Expect header visible after today's date fades in.
  task_environment()->FastForwardBy(
      calendar_utils::kResetToTodayFadeAnimationDuration);
  EXPECT_LT(0.0f, header()->layer()->opacity());
}

}  // namespace ash
