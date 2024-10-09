// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include <climits>
#include <memory>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/time/calendar_event_list_view.h"
#include "ash/system/time/calendar_list_model.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "components/account_id/account_id.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view.h"

namespace ash {

namespace {

using ::google_apis::calendar::CalendarEvent;
using ::google_apis::calendar::EventList;
using ::google_apis::calendar::SingleCalendar;

constexpr char kTestUser[] = "user@test";

const char* kCalendarId1 = "user1@email.com";
const char* kCalendarSummary1 = "user1@email.com";
const char* kCalendarColorId1 = "12";
bool kCalendarSelected1 = true;
bool kCalendarPrimary1 = true;

}  // namespace

class CalendarViewControllerTestObserver
    : public CalendarViewController::Observer {
 public:
  explicit CalendarViewControllerTestObserver(
      base::OnceCallback<void(void)> callback)
      : callback_(std::move(callback)) {}

  CalendarViewControllerTestObserver(
      const CalendarViewControllerTestObserver&) = delete;
  CalendarViewControllerTestObserver& operator=(
      const CalendarViewControllerTestObserver&) = delete;

  ~CalendarViewControllerTestObserver() override = default;

  void OnCalendarLoaded() override { std::move(callback_).Run(); }

 private:
  base::OnceCallback<void(void)> callback_;
};

class CalendarViewTest : public AshTestBase {
 public:
  CalendarViewTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kGlanceablesTimeManagementClassroomStudentView,
            features::kGlanceablesTimeManagementTasksView});
  }
  CalendarViewTest(const CalendarViewTest&) = delete;
  CalendarViewTest& operator=(const CalendarViewTest&) = delete;
  ~CalendarViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id_);
    Shell::Get()->calendar_controller()->RegisterClientForUser(account_id_,
                                                               &client_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    DestroyCalendarViewWidget();
    Shell::Get()->calendar_controller()->RegisterClientForUser(account_id_,
                                                               nullptr);
    AshTestBase::TearDown();
  }

  calendar_test_utils::CalendarClientTestImpl* client() { return &client_; }

  // Gets date cell of a given CalendarMonthView and numerical `day`.
  const views::LabelButton* GetDateCell(CalendarMonthView* month,
                                        std::u16string day) {
    const views::LabelButton* date_cell = nullptr;
    for (const views::View* child_view : month->children()) {
      auto* current_date_cell =
          static_cast<const views::LabelButton*>(child_view);
      if (day != current_date_cell->GetText()) {
        continue;
      }

      date_cell = current_date_cell;
      break;
    }
    return date_cell;
  }

  // Clicks on a given `date_cell`.
  void ClickDateCell(const views::LabelButton* date_cell) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(date_cell->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  void CreateCalendarView() {
    if (!widget_) {
      widget_ = CreateFramelessTestWidget();
      widget_->SetFullscreen(true);
    }
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);

    auto calendar_view = std::make_unique<CalendarView>(
        /*use_glanceables_container_style=*/false);

    // This awkward-looking thing is to ensure that calendar_view_ (a raw_ptr)
    // doesn't outlive the calendar view it points to, which is otherwise
    // destroyed as part of Widget::SetContentsView().
    calendar_view_ = nullptr;
    calendar_view_ = widget_->SetContentsView(std::move(calendar_view));
  }

  void CloseEventList() { calendar_view_->CloseEventList(); }

  void DestroyCalendarViewWidget() {
    calendar_view_ = nullptr;
    widget_.reset();
  }

  // Calendar has some arbitrary delays to allow itself to load, otherwise the
  // test assertions run too early and fail. We hook into the `OnCalendarLoaded`
  // callback here to pause the test until the Calendar has finished loading.
  void WaitForCalendarToCompleteLoading() {
    base::RunLoop run_loop;
    auto callback = [](base::RunLoop* run_loop) {
      run_loop->QuitClosure().Run();
    };

    auto observer = std::make_unique<CalendarViewControllerTestObserver>(
        base::BindOnce(callback, &run_loop));
    AddCalendarViewControllerObserver(observer.get());

    run_loop.Run();

    RemoveCalendarViewControllerObserver(observer.get());
  }

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

  views::Button* managed_button() { return calendar_view_->managed_button_; }
  IconButton* up_button() { return calendar_view_->up_button_; }
  IconButton* down_button() { return calendar_view_->down_button_; }
  views::View* close_button() {
    return calendar_view_->event_list_view_->close_button_container_
        ->children()[0];
  }
  views::View* event_list_view() { return calendar_view_->event_list_view_; }

  std::optional<base::Time> selected_date() {
    return calendar_view_->event_list_view_->calendar_view_controller_
        ->selected_date_;
  }

  CalendarUpNextView* up_next_view() { return calendar_view_->up_next_view_; }

  views::View* up_next_scroll_contents() {
    return calendar_view_->up_next_view_->content_view_;
  }

  // Executes the given task immediately rather than waiting for the timer.
  void run_upcoming_events_timer_task() {
    calendar_view_->check_upcoming_events_timer_.user_task().Run();
  }

  views::View* calendar_sliding_surface_view() {
    return calendar_view_->calendar_sliding_surface_;
  }

  views::View* up_next_todays_events_button() {
    return calendar_view_->up_next_view_->todays_events_button_container_
        ->children()[0];
  }

  void ScrollUpOneMonth() {
    calendar_view_->ScrollOneMonthAndAutoScroll(/*scroll_up=*/true);
  }
  void ScrollDownOneMonth() {
    calendar_view_->ScrollOneMonthAndAutoScroll(/*scroll_up=*/false);
  }
  void ScrollUpOneMonthWithAnimation() {
    calendar_view_->ScrollOneMonthWithAnimation(/*scroll_up=*/true);
  }
  void ScrollDownOneMonthWithAnimation() {
    calendar_view_->ScrollOneMonthWithAnimation(/*scroll_up=*/false);
  }
  void ResetToToday() { calendar_view_->ResetToToday(); }

  void RequestFocusForEventListCloseButton() {
    calendar_view_->RequestFocusForEventListCloseButton();
  }

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  }

  void PressShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  }

  void PressEnter() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  }

  void PressUp() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_UP, ui::EF_NONE);
  }

  void PressDown() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  }

  void PressLeft() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_LEFT, ui::EF_NONE);
  }

  void PressRight() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);
  }

  void AddCalendarViewControllerObserver(
      CalendarViewController::Observer* observer) {
    calendar_view_->calendar_view_controller_->AddObserver(observer);
  }

  void RemoveCalendarViewControllerObserver(
      CalendarViewController::Observer* observer) {
    calendar_view_->calendar_view_controller_->RemoveObserver(observer);
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  base::test::ScopedFeatureList features_;
  const AccountId account_id_ = AccountId::FromUserEmail("user1@email.com");
  calendar_test_utils::CalendarClientTestImpl client_;
  std::unique_ptr<views::Widget> widget_;
  // Owned by `widget_`.
  raw_ptr<CalendarView> calendar_view_ = nullptr;
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

// TODO(b/285280977): Remove when CalendarView is out of TrayDetailedView.
TEST_F(CalendarViewTest, NoBackButton) {
  CreateCalendarView();

  // No back button should be shown.
  EXPECT_FALSE(
      calendar_view()->GetViewByID(VIEW_ID_QS_DETAILED_VIEW_BACK_BUTTON));
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

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Todays DateCellView should be focused on open.
  EXPECT_STREQ(focus_manager->GetFocusedView()->GetClassName(),
               "CalendarDateCellView");
  EXPECT_EQ(
      static_cast<const views::LabelButton*>(focus_manager->GetFocusedView())
          ->GetText(),
      u"7");

  // Moves to the next focusable view. Today's button.
  PressTab();
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());

  // Moves to settings button.
  PressTab();
  EXPECT_EQ(settings_button(), focus_manager->GetFocusedView());

  // Moves to up button.
  PressTab();
  EXPECT_EQ(up_button(), focus_manager->GetFocusedView());

  // Moves to down button.
  PressTab();
  EXPECT_EQ(down_button(), focus_manager->GetFocusedView());

  // Moves to today's cell.
  PressTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  // Moves to down button.
  PressShiftTab();
  EXPECT_EQ(down_button(), focus_manager->GetFocusedView());

  // Moves to up button.
  PressShiftTab();
  EXPECT_EQ(up_button(), focus_manager->GetFocusedView());

  // Moves to settings button.
  PressShiftTab();
  EXPECT_EQ(settings_button(), focus_manager->GetFocusedView());

  // Moves to "Go back to today" button.
  PressShiftTab();
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());
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

  // Focus should start on todays CalendarDateCellView.
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

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

// Tests the Ash.Calendar.MaxDistanceBrowsed metric only records once in
// CalendarViews lifetime.
TEST_F(CalendarViewTest, MaxDistanceBrowsedRecordsOncePerLifetime) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  auto histogram_tester = std::make_unique<base::HistogramTester>();

  CreateCalendarView();
  DestroyCalendarViewWidget();

  // The metric should log once.
  histogram_tester->ExpectTotalCount("Ash.Calendar.MaxDistanceBrowsed", 1);

  // Create the CalendarView again, and scroll once. The metric should record
  // once, but only once the widget has been destroyed.
  histogram_tester = std::make_unique<base::HistogramTester>();
  CreateCalendarView();

  ScrollDownOneMonth();
  histogram_tester->ExpectTotalCount("Ash.Calendar.MaxDistanceBrowsed", 0);
  DestroyCalendarViewWidget();

  histogram_tester->ExpectTotalCount("Ash.Calendar.MaxDistanceBrowsed", 1);

  // Create the CalendarView again, scroll a few more times. Still the metric
  // should only record once.
  histogram_tester = std::make_unique<base::HistogramTester>();
  CreateCalendarView();

  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollDownOneMonth();
  ScrollUpOneMonth();

  DestroyCalendarViewWidget();
  histogram_tester->ExpectTotalCount("Ash.Calendar.MaxDistanceBrowsed", 1);
}

// Tests the Ash.Calendar.MaxDistanceBrowsed metric records max distance
// traveled from today.
TEST_F(CalendarViewTest,
       MaxDistanceBrowsedRecordsAbsoluteValueOfDistanceTraveled) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  auto histogram_tester = base::HistogramTester();

  CreateCalendarView();
  const int scroll_up_count = 10;
  const int scroll_down_count = scroll_up_count - 1;
  // Scroll up.
  for (int i = 0; i < scroll_up_count; ++i) {
    ScrollUpOneMonth();
  }
  // Return to today.
  for (int i = 0; i < scroll_up_count; ++i) {
    ScrollDownOneMonth();
  }
  // Scroll down from today.
  for (int i = 0; i < scroll_down_count; ++i) {
    ScrollDownOneMonth();
  }

  DestroyCalendarViewWidget();

  // `scroll_up_count` is the furthest traveled.
  histogram_tester.ExpectBucketCount("Ash.Calendar.MaxDistanceBrowsed",
                                     scroll_up_count, 1);
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
    if (found_) {
      return;
    }

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
  const raw_ptr<views::FocusManager> focus_manager_;
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

  // Focus starts on todays CalendarDateCellView.
  ASSERT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  const auto* non_focused_date_cell_view =
      GetDateCell(/*month=*/current_month(), /*day=*/u"9");

  {
    auto focus_change_listener = DateCellFocusChangeListener(
        focus_manager, /*looking_for=*/u"9", /*steps_to_find=*/1);
    ClickDateCell(non_focused_date_cell_view);
    EXPECT_TRUE(focus_change_listener.found());
  }
}

// Tests that focus returns to a DateCellView after closing the EventListView if
// the EventListView view tree had a focused view.
TEST_F(CalendarViewTest, FocusAfterClosingEventListView) {
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

  // Should start with focus on today's cell.
  ASSERT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  ASSERT_FALSE(event_list_view());

  PressEnter();
  EXPECT_TRUE(event_list_view());

  EXPECT_EQ(calendar_view()->GetFocusManager()->GetFocusedView(),
            close_button());

  PressEnter();
  EXPECT_STREQ(
      calendar_view()->GetFocusManager()->GetFocusedView()->GetClassName(),
      "CalendarDateCellView");
}

TEST_F(CalendarViewTest, FocusReturnsToTodaysDate) {
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
  // Todays DateCellView should be focused on open.
  ASSERT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  const auto* todays_date_cell_view = focus_manager->GetFocusedView();
  ClickDateCell(static_cast<const views::LabelButton*>(todays_date_cell_view));

  ASSERT_TRUE(event_list_view());

  PressEnter();

  // After EventListView is closed, todays DateCellView should be focused.
  EXPECT_EQ(todays_date_cell_view, focus_manager->GetFocusedView());
}

TEST_F(CalendarViewTest, OpenListAndCloseListFireAccessibilityEvents) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  CreateCalendarView();
  auto* focus_manager = calendar_view()->GetFocusManager();
  const auto* todays_date_cell_view = focus_manager->GetFocusedView();
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, scroll_view()));

  // Clicking on the date cell will open the event list. There should be one
  // text-changed accessibility event fired on the scroll view.
  ClickDateCell(static_cast<const views::LabelButton*>(todays_date_cell_view));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, scroll_view()));

  counter.ResetAllCounts();

  // Pressing enter will close the event list. There should be one text-changed
  // accessibility event fired on the scroll view.
  PressEnter();
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, scroll_view()));
}

// Tests `RequestFocusForEventListCloseButton()`.
TEST_F(CalendarViewTest, CloseButtonFocusing) {
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
  // Todays DateCellView should be focused on open.
  ASSERT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  ASSERT_FALSE(event_list_view());

  PressEnter();
  EXPECT_TRUE(event_list_view());

  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());

  // Focus moves back to the date cell.
  PressShiftTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  // Manually moves the focus to the close button.
  RequestFocusForEventListCloseButton();
  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());
}

// Tests when the focus changes to another date cell with the event list opened,
// the focusing ring will go to the close button automatically.
TEST_F(CalendarViewTest, FocusingToCloseButtonWithEventListOpened) {
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
  // Todays DateCellView should be focused on open.
  ASSERT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  ASSERT_FALSE(event_list_view());

  PressEnter();
  EXPECT_TRUE(event_list_view());

  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());

  // Focus moves back to today's date cell.
  PressShiftTab();
  EXPECT_EQ(u"7",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());

  // Navigates to another date cell and focuses on it. The focusing ring should
  // go to the close button automatically.
  PressUp();
  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  PressEnter();
  EXPECT_TRUE(event_list_view());
  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());

  // Tests different date cells and expects the same focusing behavior.
  PressShiftTab();
  PressLeft();
  EXPECT_EQ(u"29",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  PressEnter();
  EXPECT_TRUE(event_list_view());
  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());

  PressShiftTab();
  PressRight();
  PressRight();
  EXPECT_EQ(u"25",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  PressEnter();
  EXPECT_TRUE(event_list_view());
  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());

  PressShiftTab();
  PressDown();
  EXPECT_EQ(u"30",
            static_cast<views::LabelButton*>(focus_manager->GetFocusedView())
                ->GetText());
  PressEnter();
  EXPECT_TRUE(event_list_view());
  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());
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
  // Todays DateCellView should be focused on open.
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
  // Focus starts on todays CalendarDateCellView.
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
  // Focus starts on todays CalendarDateCellView.
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

  // The 17th child cell in the month, which is the non-grayed out 15th date
  // cell.
  EXPECT_EQ(u"15",
            static_cast<views::LabelButton*>(current_month()->children()[16])
                ->GetText());

  // The calendar view will show today's row as the first row, so only dates
  // that are below today's row are in the visible rect.
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[16]));
  bool is_showing_event_list_view = event_list_view();
  EXPECT_FALSE(is_showing_event_list_view);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_EQ(u"15",
            static_cast<views::LabelButton*>(current_month()->children()[16])
                ->GetText());

  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[16]));

  is_showing_event_list_view = event_list_view();
  EXPECT_TRUE(is_showing_event_list_view);
  GestureTapOn(close_button());
}

// Tests multiple scenarios that should record the metric when scrolling.
// TODO(crbug.com/333283676): Re-enable once the test failure is fixed.
TEST_F(CalendarViewTest, DISABLED_RecordDwellTimeMetricWhenScrolling) {
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

  // Opening and closing the CalendarEventListView through a date cell within
  // the current month does not record the metric.
  auto* first_of_month_date_cell =
      GetDateCell(/*month=*/current_month(), /*day=*/u"1");
  ClickDateCell(first_of_month_date_cell);
  CloseEventList();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/6);

  // Closing the calendar view should record the metric.
  DestroyCalendarViewWidget();
  histogram_tester.ExpectTotalCount("Ash.Calendar.MonthDwellTime",
                                    /*expected_count=*/7);
}

// Tests that EventListView has proper bounds when shown.
TEST_F(CalendarViewTest, EventListBoundsTest) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));

  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  ASSERT_EQ(u"15",
            static_cast<views::LabelButton*>(current_month()->children()[16])
                ->GetText());
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[16]));
  ASSERT_TRUE(event_list_view());
  ASSERT_FALSE(current_month()->layer()->GetAnimator()->is_animating());

  // EventListView should be flush with the bottom of the scroll views visible
  // area. EventListView is ignored by the CalendarViews LayoutManager, but it
  // should be flush with the bottom of `scroll_view_`'s clipped height.
  // `scroll_view_`'s bounds extend further to the end of the view, but the
  // contents of `scroll_view_` are clipped.
  const int bottom_of_scroll_view_visible_area =
      scroll_view()->bounds().y() + scroll_view()->GetMaxHeight();
  const int top_of_event_list_view = calendar_sliding_surface_view()->y();
  EXPECT_EQ(bottom_of_scroll_view_visible_area, top_of_event_list_view);
}

TEST_F(CalendarViewTest, AdminDisabledTest) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  client()->set_is_disabled_by_admin(true);

  CreateCalendarView();

  auto* focus_manager = calendar_view()->GetFocusManager();
  // Todays `DateCellView` should be focused on open.
  ASSERT_TRUE(focus_manager->GetFocusedView()->GetClassName());
  ASSERT_TRUE(focus_manager->GetFocusedView());

  // Moves to the next focusable view - managed icon button.
  PressTab();
  EXPECT_EQ(managed_button(), focus_manager->GetFocusedView());

  // Moves to the next focusable view. Today's button.
  PressTab();
  EXPECT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());

  // Moves to settings button.
  PressTab();
  EXPECT_EQ(settings_button(), focus_manager->GetFocusedView());

  // Moves back to managed icon button.
  PressShiftTab();
  PressShiftTab();

  EXPECT_EQ(managed_button(), focus_manager->GetFocusedView());
}

TEST_F(CalendarViewTest, ManagedButtonTest) {
  base::Time date;
  // Create a monthview based on Jun,7th 2021.
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  client()->set_is_disabled_by_admin(true);

  CreateCalendarView();

  // Click on managed button to open chrome://management.
  GestureTapOn(managed_button());
}

// A test class for testing animation. This class cannot set fake now since it's
// using `MOCK_TIME` to test the animations, and it can't inherit from
// CalendarAnimationTest due to the same reason.
class CalendarViewAnimationTest
    : public AshTestBase,
      public testing::WithParamInterface</*multi_calendar_enabled=*/bool> {
 public:
  CalendarViewAnimationTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kMultiCalendarSupport, IsMultiCalendarEnabled()},
         {features::kGlanceablesTimeManagementClassroomStudentView, false},
         {features::kGlanceablesTimeManagementTasksView, false}});
  }
  CalendarViewAnimationTest(const CalendarViewAnimationTest&) = delete;
  CalendarViewAnimationTest& operator=(const CalendarViewAnimationTest&) =
      delete;
  ~CalendarViewAnimationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    // Register a mock `CalendarClient` to the `CalendarController`.
    const std::string email = "user1@email.com";
    AccountId account_id = AccountId::FromUserEmail(email);
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    calendar_list_model_ =
        Shell::Get()->system_tray_model()->calendar_list_model();
    calendar_model_ = Shell::Get()->system_tray_model()->calendar_model();
    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());

    if (IsMultiCalendarEnabled()) {
      // Set a mock calendar list so the calendar list fetch returns
      // successfully.
      SetCalendarList();
    }
  }

  void TearDown() override {
    calendar_list_model_ = nullptr;
    calendar_model_ = nullptr;
    calendar_view_ = nullptr;

    widget_.reset();
    time_overrides_.reset();
    scoped_feature_list_.Reset();

    AshTestBase::TearDown();
  }

  bool IsMultiCalendarEnabled() { return GetParam(); }

  void CreateCalendarView() {
    // Don't allow calendar_view_ to temporarily dangle while we're replacing
    // the Widget's contents view. Otherwise, inside SetContentsView() the old
    // calendar_view_ will be destroyed and will temporarily dangle.
    calendar_view_ = nullptr;
    calendar_view_ = widget_->SetContentsView(std::make_unique<CalendarView>(
        /*use_glanceables_container_style=*/false));
  }

  void SetCalendarList() {
    // Set a mock calendar list.
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
    calendars.push_back(calendar_test_utils::CreateCalendar(
        kCalendarId1, kCalendarSummary1, kCalendarColorId1, kCalendarSelected1,
        kCalendarPrimary1));
    calendar_client_->SetCalendarList(
        calendar_test_utils::CreateMockCalendarList(std::move(calendars)));
  }

  // Gets date cell of a given CalendarMonthView and numerical `day`.
  const views::LabelButton* GetDateCell(CalendarMonthView* month,
                                        std::u16string day) {
    const views::LabelButton* date_cell = nullptr;
    for (const views::View* child_view : month->children()) {
      auto* current_date_cell =
          static_cast<const views::LabelButton*>(child_view);
      if (day != current_date_cell->GetText()) {
        continue;
      }

      date_cell = current_date_cell;
      break;
    }
    return date_cell;
  }

  int GetPositionOfToday() { return calendar_view()->GetPositionOfToday(); }

  // Clicks on a given `date_cell`.
  void ClickDateCell(const views::LabelButton* date_cell) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(date_cell->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  std::optional<base::Time> GetSelectedDate() {
    return calendar_view_->calendar_view_controller()->selected_date_;
  }

  void CloseEventList() { calendar_view_->CloseEventList(); }

  void UpdateMonth(base::Time date) {
    calendar_view_->calendar_view_controller()->UpdateMonth(date);
    // Advances the time to allow `on_screen_month_` to update.
    task_environment()->FastForwardBy(
        calendar_test_utils::kAnimationSettleDownDuration);
    calendar_view_->content_view_->RemoveAllChildViews();
    calendar_view_->SetMonthViews();
    scroll_view()->ScrollToPosition(
        scroll_view()->vertical_scroll_bar(),
        calendar_view_->GetPositionOfCurrentMonth());
  }

  // The position of the `next_month_`.
  int NextMonthPosition() {
    return previous_label()->GetPreferredSize().height() +
           calendar_view_->previous_month_->GetPreferredSize().height() +
           current_label()->GetPreferredSize().height() +
           calendar_view_->current_month_->GetPreferredSize().height() +
           next_label()->GetPreferredSize().height();
  }

  // The position of `current_month_`.
  int CurrentMonthPosition() {
    return calendar_view_->GetPositionOfCurrentMonth();
  }

  void ScrollUpOneMonth() {
    calendar_view_->ScrollOneMonthWithAnimation(/*scroll_up=*/true);
  }

  void ScrollDownOneMonth() {
    calendar_view_->ScrollOneMonthWithAnimation(/*scroll_up=*/false);
  }

  void ResetToTodayWithAnimation() {
    calendar_view_->ResetToTodayWithAnimation();
  }

  bool IsAnimating() { return calendar_view_->IsAnimating(); }

  bool is_scrolling_up() { return calendar_view_->is_scrolling_up_; }

  views::ScrollView::ScrollBarMode GetScrollBarMode() {
    return scroll_view()->GetVerticalScrollBarMode();
  }

  views::Widget* widget() { return widget_.get(); }
  CalendarView* calendar_view() { return calendar_view_; }

  views::Label* month_header() { return calendar_view_->header_->header_; }
  views::Label* header_year() { return calendar_view_->header_->header_year_; }
  CalendarHeaderView* header() { return calendar_view_->header_; }
  CalendarHeaderView* temp_header() { return calendar_view_->temp_header_; }
  CalendarMonthView* current_month() { return calendar_view_->current_month_; }
  CalendarMonthView* previous_month() {
    return calendar_view_->previous_month_;
  }
  CalendarMonthView* next_month() { return calendar_view_->next_month_; }
  views::View* previous_label() { return calendar_view_->previous_label_; }
  views::View* current_label() { return calendar_view_->current_label_; }
  views::View* next_label() { return calendar_view_->next_label_; }
  views::ScrollView* scroll_view() { return calendar_view_->scroll_view_; }
  views::View* event_list_view() { return calendar_view_->event_list_view_; }
  CalendarListModel* calendar_list_model() { return calendar_list_model_; }
  CalendarModel* calendar_model() { return calendar_model_; }
  views::View* calendar_sliding_surface_view() {
    return calendar_view_->calendar_sliding_surface_;
  }
  calendar_test_utils::CalendarClientTestImpl* calendar_client() {
    return calendar_client_.get();
  }

  bool should_months_animate() {
    return calendar_view_->should_months_animate_;
  }

  std::map<base::Time, CalendarModel::FetchingStatus> on_screen_month() {
    return calendar_view_->on_screen_month_;
  }

  // Wait until the response is back. Since we used `PostDelayedTask` with 1
  // second to mimic the behavior of fetching, duration of 1 minute should be
  // enough.
  void WaitUntilFetched() {
    task_environment()->FastForwardBy(base::Minutes(1));
    base::RunLoop().RunUntilIdle();
  }

  void SetTodayFromTime(base::Time date) {
    std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
        date, calendar_utils::kNumSurroundingMonthsCached);

    calendar_model_->non_prunable_months_.clear();
    // Non-prunable months are today's date and the two surrounding months.
    calendar_model_->AddNonPrunableMonths(months);
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  // Owned by `widget_`.
  raw_ptr<CalendarView> calendar_view_ = nullptr;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_;
  raw_ptr<CalendarListModel> calendar_list_model_ = nullptr;
  raw_ptr<CalendarModel> calendar_model_ = nullptr;
  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(MultiCalendar,
                         CalendarViewAnimationTest,
                         testing::Bool());

// The header should show the new header with animation once there's an update
// when the event list view is shown.
TEST_P(CalendarViewAnimationTest, HeaderAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Oct 2021 10:00 GMT", &date));
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  CreateCalendarView();
  // Gives it a duration to let the animation finish and pass the cool down
  // duration.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  UpdateMonth(date);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Update the header to next month.
  calendar_view()->calendar_view_controller()->UpdateMonth(date +
                                                           base::Days(10));

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationStartBufferDuration);

  // Should not show animation if the event list is not open.
  EXPECT_FALSE(temp_header()->GetVisible());
  EXPECT_FALSE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(temp_header()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Also update the month views to click on the correct date cell.
  UpdateMonth(date + base::Days(10));
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Opens the event list view by click on a radom non-grayed out cell.
  const auto* date_cell = GetDateCell(/*month=*/current_month(), /*day=*/u"10");
  ClickDateCell(date_cell);

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_TRUE(event_list_view());

  // Update the header to next month.
  calendar_view()->calendar_view_controller()->UpdateMonth(date +
                                                           base::Days(45));
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationStartBufferDuration);

  // Showing the temp header for animation. All header should be animating.
  EXPECT_TRUE(temp_header()->GetVisible());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(temp_header()->layer()->GetAnimator()->is_animating());
  EXPECT_GE(1.0f, temp_header()->layer()->opacity());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Now the header is updated to the new month and year.
  EXPECT_EQ(u"December", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
}

TEST_P(CalendarViewAnimationTest, HeaderAnimationDirection) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2023 10:00 GMT", &date));
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  CreateCalendarView();

  // Gives it a duration to let the animation finish and pass the cool down
  // duration.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  UpdateMonth(date);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Scrolls to the next month.
  ScrollDownOneMonth();
  EXPECT_FALSE(is_scrolling_up());

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Scrolls to the previous month.
  ScrollUpOneMonth();
  EXPECT_TRUE(is_scrolling_up());

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Opens the event list view by clicking on a non-grayed out cell on the next
  // month, so that the header will animate to the next month's header.
  const auto* date_cell = GetDateCell(/*month=*/next_month(), /*day=*/u"10");
  ClickDateCell(date_cell);

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_FALSE(is_scrolling_up());
  EXPECT_TRUE(event_list_view());

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
}

// The month views and header should animate when scrolling up or down.
TEST_P(CalendarViewAnimationTest, MonthAndHeaderAnimation) {
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
  EXPECT_FALSE(temp_header()->GetVisible());

  // Scrolls to the next month.
  ScrollDownOneMonth();

  EXPECT_FALSE(is_scrolling_up());

  // If scrolls down, the month views and labels will be animating.
  EXPECT_EQ(1.0f, header()->layer()->opacity());
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForMoving);
  EXPECT_TRUE(current_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(current_label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Showing the temp header for animation. All header should be animating.
  EXPECT_TRUE(temp_header()->GetVisible());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(temp_header()->layer()->GetAnimator()->is_animating());
  EXPECT_GE(1.0f, temp_header()->layer()->opacity());

  // Gives it a duration to let the animation finish.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Now the header is updated to the new month and year before it starts
  // showing up.
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
  EXPECT_EQ(1.0f, header()->layer()->opacity());
  EXPECT_FALSE(temp_header()->GetVisible());

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

  // Showing the temp header for animation. All header should be animating.
  EXPECT_TRUE(temp_header()->GetVisible());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(temp_header()->layer()->GetAnimator()->is_animating());
  EXPECT_GE(1.0f, temp_header()->layer()->opacity());

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
TEST_P(CalendarViewAnimationTest, NotScrollableWhenAnimating) {
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

  // The scroll bar is enaled before tapping on the up button.
  EXPECT_EQ(views::ScrollView::ScrollBarMode::kHiddenButEnabled,
            GetScrollBarMode());

  // Scrolls to the previous month.
  ScrollUpOneMonth();

  // If scrolls down, the month views and labels will be animating.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationStartBufferDuration);
  EXPECT_TRUE(current_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(next_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_month()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(previous_label()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(current_label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Showing the temp header for animation. All header should be animating.
  EXPECT_TRUE(temp_header()->GetVisible());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(temp_header()->layer()->GetAnimator()->is_animating());

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

TEST_P(CalendarViewAnimationTest, ResetToTodayWithAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create calendar view and wait for the animation to finish.
  CreateCalendarView();
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(header()->layer());

  // Expect header visible before starting ResetToToday animation.
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  ResetToTodayWithAnimation();
  // The header starts to animate.
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());

  // The header's opacity should be between 0~1 after the first animator
  // finished and in the middle of the second animation.
  animation_waiter.Wait(header()->layer());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_GT(1.0f, header()->layer()->opacity());

  // The header's opacity should be 1 after the second animator finished.
  animation_waiter.Wait(header()->layer());
  EXPECT_FALSE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, header()->layer()->opacity());

  // Open event list by selecting the next month's first cell.
  const auto* date_cell = GetDateCell(/*month=*/next_month(), /*day=*/u"1");
  ClickDateCell(date_cell);

  // Event list view starts to animate.
  EXPECT_TRUE(
      calendar_sliding_surface_view()->layer()->GetAnimator()->is_animating());
  animation_waiter.Wait(current_label()->layer());
  animation_waiter.Wait(calendar_sliding_surface_view()->layer());

  // Event list view just finished animating.
  EXPECT_FALSE(
      calendar_sliding_surface_view()->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(should_months_animate());
  // The cool-down time for enabling animation. Otherwise the next reset to
  // today animation will not be enabled.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_TRUE(should_months_animate());
  EXPECT_TRUE(event_list_view());
  EXPECT_EQ(1.0f, event_list_view()->layer()->opacity());

  ResetToTodayWithAnimation();
  // The header starts to animate.
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());

  // The header's opacity should be between 0~1 after the first animator
  // finished and in the middle of the second animation.
  animation_waiter.Wait(header()->layer());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_GT(1.0f, header()->layer()->opacity());

  // The header's opacity should be 1 after the second animator finished.
  animation_waiter.Wait(header()->layer());
  EXPECT_FALSE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, header()->layer()->opacity());
  // Expect today's date in `selected_date_` after resetting to today.
  EXPECT_EQ(calendar_utils::GetMonthDayYear(base::Time::Now()),
            calendar_utils::GetMonthDayYear(GetSelectedDate().value()));

  // Expect header visible after closing event list and resetting to today.
  CloseEventList();
  EXPECT_TRUE(
      calendar_sliding_surface_view()->layer()->GetAnimator()->is_animating());
  // Wait `event_list_view()`'s layer first since it can be deleted after the
  // animation finished.
  animation_waiter.Wait(event_list_view()->layer());
  animation_waiter.Wait(calendar_sliding_surface_view()->layer());
  EXPECT_FALSE(
      calendar_sliding_surface_view()->layer()->GetAnimator()->is_animating());

  ResetToTodayWithAnimation();
  // The header starts to animate.
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());

  // The header's opacity should be between 0~1 after the first animator
  // finished and in the middle of the second animation.
  animation_waiter.Wait(header()->layer());
  EXPECT_TRUE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_GT(1.0f, header()->layer()->opacity());

  // The header's opacity should be 1 after the second animator finished.
  animation_waiter.Wait(header()->layer());
  EXPECT_FALSE(header()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, header()->layer()->opacity());
}

// Tests that the loading bar becomes visible when any of the on screen months
// has not finished fetching and becomes invisible once all months on screen
// have finished fetching events.
TEST_P(CalendarViewAnimationTest, LoadingBarVisibilityForOneMonthOnScreen) {
  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  // Tests when the `CalendarView` size is small to hold only one month on
  // screen.
  UpdateDisplay("800x200");
  CreateCalendarView();

  // Advances the time to allow `on_screen_month_` to initialize.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  EXPECT_EQ(1U, on_screen_month().size());
  const auto* progress_bar = calendar_view()->GetViewByID(
      base::to_underlying(GlanceablesViewId::kProgressBar));
  EXPECT_TRUE(progress_bar->GetVisible());

  // Waits until the events are fetched, and tests the loading bar is invisible.
  WaitUntilFetched();
  EXPECT_FALSE(progress_bar->GetVisible());
}

TEST_P(CalendarViewAnimationTest, LoadingBarVisibility) {
  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  // Tests when the `CalendarView` has two months on screen.
  UpdateDisplay("800x600");
  CreateCalendarView();

  // Advances the time to allow `on_screen_month_` to initialize.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  const auto* progress_bar = calendar_view()->GetViewByID(
      base::to_underlying(GlanceablesViewId::kProgressBar));
  EXPECT_TRUE(progress_bar->GetVisible());

  // Waits until the events are fetched, and tests the loading bar is invisible.
  WaitUntilFetched();
  EXPECT_FALSE(progress_bar->GetVisible());

  // Resets to today so that a new fetching request will be sent for on-screen
  // months who have cached events(eg. a refetching status). Tests the loading
  // bar is visible.
  SetTodayFromTime(base::Time::Now());
  ResetToTodayWithAnimation();

  // Advances the time to allow `on_screen_month_` to update.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_TRUE(progress_bar->GetVisible());
}

// Tests the loading bar visibility for different user sessions.
TEST_P(CalendarViewAnimationTest,
       LoadingBarVisibilityForDifferentUserSessions) {
  // Make sure that the `CalendarView` can have enough space to hold at least 1
  // month. 600 should be enough since the calendar bubble's height is set to
  // 464 in the `UnifiedSystemTrayBubble`.
  UpdateDisplay("800x600");

  // Tests when the user is logged in, the loading bar is visible.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  CreateCalendarView();
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_TRUE(
      calendar_view()
          ->GetViewByID(base::to_underlying(GlanceablesViewId::kProgressBar))
          ->GetVisible());

  // Tests when the screen is locked, the loading bar is invisible.
  calendar_model()->ClearAllCachedEvents();
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  CreateCalendarView();

  // Advances the time to allow `on_screen_month_` to initialize.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_FALSE(
      calendar_view()
          ->GetViewByID(base::to_underlying(GlanceablesViewId::kProgressBar))
          ->GetVisible());

  // Tests when the user starts the login process, the loading bar is invisible.
  calendar_model()->ClearAllCachedEvents();
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  CreateCalendarView();
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  EXPECT_FALSE(
      calendar_view()
          ->GetViewByID(base::to_underlying(GlanceablesViewId::kProgressBar))
          ->GetVisible());
}

// Tests the loading bar visibility for when fetching events errors.
TEST_P(CalendarViewAnimationTest, LoadingBarVisibilityForErrorFetchingEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("04 May 2022 15:00 GMT", &date));

  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  // Tests when the `CalendarView` size is small to hold only one month on
  // screen.
  UpdateDisplay("800x200");
  CreateCalendarView();
  // Advances the time to allow `on_screen_month_` to initialize.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  UpdateMonth(date);

  EXPECT_EQ(1U, on_screen_month().size());

  // Sets the fetching status of current month to be kFetching, and tests the
  // loading bar is visible.
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(date);
  base::Time current_date =
      calendar_view()->calendar_view_controller()->currently_shown_date();
  base::Time start_of_current_month = calendar_utils::GetStartOfMonthUTC(
      current_date + calendar_utils::GetTimeDifference(current_date));
  EXPECT_EQ(start_of_month, start_of_current_month);

  calendar_client()->SetError(google_apis::NO_CONNECTION);
  calendar_model()->FetchEvents(start_of_current_month);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  const auto* progress_bar = calendar_view()->GetViewByID(
      base::to_underlying(GlanceablesViewId::kProgressBar));
  EXPECT_TRUE(progress_bar->GetVisible());

  // Waits until the events are fetched, and tests the loading bar is invisible.
  WaitUntilFetched();
  EXPECT_FALSE(progress_bar->GetVisible());
}

// Tests the loading bar visibility for when fetching events times out.
TEST_P(CalendarViewAnimationTest,
       LoadingBarVisibilityForTimeoutFetchingEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("04 May 2022 15:00 GMT", &date));

  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  // Tests when the `CalendarView` size is small to hold only one month on
  // screen.
  UpdateDisplay("800x200");
  CreateCalendarView();
  // Advances the time to allow `on_screen_month_` to initialize.
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  UpdateMonth(date);

  EXPECT_EQ(1U, on_screen_month().size());

  // Sets the fetching status of current month to be kFetching, and tests the
  // loading bar is visible.
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(date);
  base::Time current_date =
      calendar_view()->calendar_view_controller()->currently_shown_date();
  base::Time start_of_current_month = calendar_utils::GetStartOfMonthUTC(
      current_date + calendar_utils::GetTimeDifference(current_date));
  EXPECT_EQ(start_of_month, start_of_current_month);

  calendar_client()->ForceTimeout();
  calendar_model()->FetchEvents(start_of_current_month);
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);
  const auto* progress_bar = calendar_view()->GetViewByID(
      base::to_underlying(GlanceablesViewId::kProgressBar));
  EXPECT_TRUE(progress_bar->GetVisible());

  // Waits until the events are fetched, and tests the loading bar is invisible.
  WaitUntilFetched();
  EXPECT_FALSE(progress_bar->GetVisible());
}

// Tests that the EventListView does not crash if shown during the initial open.
TEST_P(CalendarViewAnimationTest, QuickShowEventListInitialOpen) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Creates calendar view, which will trigger an animation.
  CreateCalendarView();

  // Try to show the EventListView during the initial animation by selecting a
  // today's date cell.
  ClickDateCell(
      calendar_view()->calendar_view_controller()->todays_date_cell_view());

  EXPECT_TRUE(IsAnimating());
  EXPECT_TRUE(event_list_view());

  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  EXPECT_TRUE(event_list_view());
}

// Tests that the EventListView does not show during the month change animation.
TEST_P(CalendarViewAnimationTest, DontShowEventListDuringMonthAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  CreateCalendarView();
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Start the scroll down animation.
  ScrollUpOneMonth();
  task_environment()->FastForwardBy(
      calendar_utils::kAnimationDurationForVisibility);
  ASSERT_TRUE(current_month()->layer()->GetAnimator()->is_animating());

  // Try to open the EventListView.
  const auto* tomorrow_date_cell =
      GetDateCell(/*month=*/current_month(), /*day=*/u"25");
  ClickDateCell(tomorrow_date_cell);

  EXPECT_FALSE(event_list_view());
}

// Regression test for b/265203105
// Tests open/close the `CalendarEventListView`. Also tests one corner case:
// when closing the event list right after opening it, do nothing since the
// animation is not finished.
TEST_P(CalendarViewAnimationTest, OpenAndCloseEventList) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  CreateCalendarView();
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(header()->layer());

  // Opens the `CalendarEventListView`.
  ClickDateCell(
      calendar_view()->calendar_view_controller()->todays_date_cell_view());

  EXPECT_TRUE(IsAnimating());
  EXPECT_TRUE(event_list_view());
  EXPECT_TRUE(GetSelectedDate().has_value());

  // Should not close the event list before showing up animation is finished.
  CloseEventList();
  EXPECT_TRUE(IsAnimating());
  EXPECT_TRUE(event_list_view());
  EXPECT_TRUE(GetSelectedDate().has_value());

  // After the showing up animation is finished, the event list view should be
  // up.
  animation_waiter.Wait(event_list_view()->layer());
  animation_waiter.Wait(calendar_sliding_surface_view()->layer());
  animation_waiter.Wait(current_label()->layer());
  EXPECT_FALSE(IsAnimating());
  EXPECT_TRUE(event_list_view());
  EXPECT_TRUE(GetSelectedDate().has_value());

  // Should close the event list now.
  CloseEventList();

  // The event list the view is still showing and `selected_date_` value is
  // still set during the animation.
  EXPECT_TRUE(IsAnimating());
  EXPECT_TRUE(event_list_view());
  EXPECT_TRUE(GetSelectedDate().has_value());

  // Resets the `selected_date_` after the fading out animation is done.
  animation_waiter.Wait(event_list_view()->layer());
  animation_waiter.Wait(calendar_sliding_surface_view()->layer());
  animation_waiter.Wait(current_label()->layer());
  EXPECT_FALSE(IsAnimating());
  EXPECT_FALSE(event_list_view());
  EXPECT_FALSE(GetSelectedDate().has_value());
}

class CalendarViewWithUpNextViewTest : public CalendarViewTest {
 public:
  CalendarViewWithUpNextViewTest() = default;
  CalendarViewWithUpNextViewTest(const CalendarViewWithUpNextViewTest&) =
      delete;
  CalendarViewWithUpNextViewTest& operator=(
      const CalendarViewWithUpNextViewTest&) = delete;
  ~CalendarViewWithUpNextViewTest() override = default;

  // Assumes current time is "18 Nov 2021 10:00 GMT".
  std::unique_ptr<google_apis::calendar::EventList>
  CreateMockEventListWithEventStartTimeMoreThanTwoHoursAway() {
    auto event_list = std::make_unique<google_apis::calendar::EventList>();
    event_list->set_time_zone("Greenwich Mean Time");
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_0", "summary_0", "18 Nov 2021 12:30 GMT", "18 Nov 2021 13:30 GMT"));

    return event_list;
  }

  // Assumes current time is "18 Nov 2021 10:00 GMT".
  std::unique_ptr<google_apis::calendar::EventList>
  CreateMockEventListWithEventStartTimeTenMinsAway() {
    auto event_list = std::make_unique<google_apis::calendar::EventList>();
    event_list->set_time_zone("Greenwich Mean Time");
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_0", "summary_0", "18 Nov 2021 10:10 GMT", "18 Nov 2021 13:30 GMT"));

    return event_list;
  }

  // Assumes current time is "18 Nov 2021 10:00 GMT".
  std::unique_ptr<google_apis::calendar::EventList>
  CreateMockEventListWithTwoEventsOneEndingInOneMin() {
    auto event_list = std::make_unique<google_apis::calendar::EventList>();
    event_list->set_time_zone("Greenwich Mean Time");
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_0", "summary_0", "18 Nov 2021 10:00 GMT", "18 Nov 2021 13:30 GMT"));
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_1", "summary_1", "18 Nov 2021 09:00 GMT", "18 Nov 2021 10:01 GMT"));

    return event_list;
  }

  // Assumes current time is "18 Nov 2021 23:55 GMT".
  std::unique_ptr<google_apis::calendar::EventList>
  CreateMockEventListStartingFivePastMidnight() {
    auto event_list = std::make_unique<google_apis::calendar::EventList>();
    event_list->set_time_zone("Greenwich Mean Time");
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_0", "summary_0", "19 Nov 2021 00:05 GMT", "19 Nov 2021 01:30 GMT"));

    return event_list;
  }

  void MockEventsFetched(
      base::Time date,
      std::unique_ptr<google_apis::calendar::EventList> event_list) {
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(date),
        google_apis::calendar::kPrimaryCalendarId,
        google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());
  }
};

TEST_F(CalendarViewWithUpNextViewTest,
       GivenNoEvents_WhenCalendarViewOpens_ThenUpNextViewShouldNotBeShown) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("7 Jun 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  client()->set_is_disabled_by_admin(true);

  CreateCalendarView();

  // When we've just created the calendar view and not fetched any events, then
  // up next shouldn't have been created.
  bool is_showing_up_next_view = up_next_view();
  EXPECT_FALSE(is_showing_up_next_view);
}

TEST_F(
    CalendarViewWithUpNextViewTest,
    GivenEventsStartingMoreThanTwoHoursAway_WhenCalendarViewOpens_ThenUpNextViewShouldNotBeShown) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(
      calendar_utils::GetStartOfMonthUTC(date),
      CreateMockEventListWithEventStartTimeMoreThanTwoHoursAway());

  // Fetch an event starting more than two hours away, up next should show.
  bool is_showing_up_next_view = up_next_view();
  EXPECT_TRUE(is_showing_up_next_view);
}

TEST_F(CalendarViewWithUpNextViewTest, ShouldShowUpNextView) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE(up_next_view());
}

TEST_F(CalendarViewWithUpNextViewTest,
       ShouldNotShowUpNextView_WhenEventListViewIsOpen) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  // Open the event list view for any day.
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[25]));
  ASSERT_TRUE(event_list_view());
  // Mock events that start in ten mins coming in.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  bool is_showing_up_next_view = up_next_view();
  EXPECT_FALSE(is_showing_up_next_view);
  const int bottom_of_scroll_view_visible_area =
      scroll_view()->bounds().y() + scroll_view()->GetMaxHeight();
  const int top_of_event_list_view = calendar_sliding_surface_view()->y();
  EXPECT_EQ(bottom_of_scroll_view_visible_area, top_of_event_list_view);
}

// If there are upcoming events and the up next view should have been shown but
// the event list was open, then when it closes we should show the up next view.
TEST_F(
    CalendarViewWithUpNextViewTest,
    ShouldShowUpNextView_WhenEventListViewHasFinishedClosing_SelectedDateIsToday) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  // Open the event list view for today.
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[18]));
  ASSERT_TRUE(event_list_view());
  // Mock events that start in ten mins coming in.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());
  CloseEventList();

  // After closing the event list view, we expect the up next view to now be
  // shown.
  bool is_showing_up_next_view = up_next_view();
  EXPECT_TRUE(is_showing_up_next_view);
}

// If there are upcoming events and the event list was open for another day
// that's not on the same row with today, then when it closes we should not show
// the up next view.
TEST_F(
    CalendarViewWithUpNextViewTest,
    ShouldNotShowUpNextView_WhenEventListViewHasFinishedClosing_SelectedDateNotOnTheSameRowWithToday) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  // Open the event list view for a day that's not on the same row with today.
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[25]));
  ASSERT_TRUE(event_list_view());
  // Mock events that start in ten mins coming in.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());
  CloseEventList();

  // After closing the event list view, we expect there's no up next view
  // showing.
  EXPECT_FALSE(up_next_view());
}

TEST_F(CalendarViewWithUpNextViewTest,
       ShouldOpenEventListView_WhenUpNextShowTodaysEventsButtonPressed) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  bool is_showing_up_next_view = up_next_view();
  EXPECT_TRUE(is_showing_up_next_view);
  bool is_showing_event_list_view = event_list_view();
  EXPECT_FALSE(is_showing_event_list_view);

  LeftClickOn(up_next_todays_events_button());

  is_showing_event_list_view = event_list_view();
  EXPECT_TRUE(is_showing_event_list_view);
}

TEST_F(
    CalendarViewWithUpNextViewTest,
    GivenUpNextIsShown_WhenNewEventsMoreThanTwoHoursAwayAreFetched_UpNextViewShouldStillBeShown) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE((up_next_view() && up_next_view()->GetVisible()));

  MockEventsFetched(
      calendar_utils::GetStartOfMonthUTC(date),
      CreateMockEventListWithEventStartTimeMoreThanTwoHoursAway());

  // When fetched events are now more than two hours away, the up next should
  // still show.
  EXPECT_TRUE((up_next_view() && up_next_view()->GetVisible()));
}

// Tests the following:
// - 2 upcoming events are displayed in the up next view
// - Time passes so that one event has ended
// - Up next refreshes to show the 1 upcoming event (as the other has now ended)
TEST_F(CalendarViewWithUpNextViewTest,
       ShouldHideCompletedEventsInTheUpNextView) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithTwoEventsOneEndingInOneMin());

  // Up next should be shown with the 2 events.
  EXPECT_TRUE(up_next_view());
  EXPECT_EQ(size_t(2), up_next_scroll_contents()->children().size());

  // Move base::Time::Now to 1 minute past the event end time and force the
  // upcoming events timer to fire.
  base::Time time_one_min_past_event_end;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:02 GMT",
                                     &time_one_min_past_event_end));
  SetFakeNow(time_one_min_past_event_end);
  run_upcoming_events_timer_task();

  // Up next should still be showing but with a single event as the other has
  // ended.
  EXPECT_TRUE(up_next_view());
  EXPECT_EQ(size_t(1), up_next_scroll_contents()->children().size());
}

TEST_F(CalendarViewWithUpNextViewTest,
       ShouldClipHeightOfScrollView_WhenUpNextIsShown) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();

  // Expect the scrollview to have max int height when neither up next nor the
  // event list view are showing.
  EXPECT_EQ(INT_MAX, scroll_view()->GetMaxHeight());

  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE(up_next_view());
  // When up next is showing, the scrollview should be clipped to sit above but
  // slightly overlapping the up next view bounds.
  const int expected_max_height = scroll_view()->height() -
                                  up_next_view()->GetPreferredSize().height() +
                                  calendar_utils::kUpNextOverlapInPx;
  EXPECT_EQ(expected_max_height, scroll_view()->GetMaxHeight());
}

TEST_F(
    CalendarViewWithUpNextViewTest,
    ShouldClipHeightOfScrollView_WhenEventListHasClosed_AndUpNextIsStillShown) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE(up_next_view());

  // Open the event list view for today.
  ASSERT_EQ(u"18",
            static_cast<views::LabelButton*>(current_month()->children()[18])
                ->GetText());
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[18]));
  ASSERT_TRUE(event_list_view());

  // Close the event list view.
  GestureTapOn(close_button());
  ASSERT_FALSE(event_list_view());

  // When the event list view closes, the scrollview max height should have been
  // clipped back to the right height for the up next view.
  const int expected_max_height = scroll_view()->height() -
                                  up_next_view()->GetPreferredSize().height() +
                                  calendar_utils::kUpNextOverlapInPx;
  EXPECT_EQ(expected_max_height, scroll_view()->GetMaxHeight());
}

TEST_F(CalendarViewWithUpNextViewTest,
       ShouldShowUpNext_WhenEventListHasClosed_AndAnUpcomingEventJustCameIn) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(
      calendar_utils::GetStartOfMonthUTC(date),
      CreateMockEventListWithEventStartTimeMoreThanTwoHoursAway());

  EXPECT_TRUE(up_next_view());

  // Open the event list view for today.
  ASSERT_EQ(u"18",
            static_cast<views::LabelButton*>(current_month()->children()[18])
                ->GetText());
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[18]));
  ASSERT_TRUE(event_list_view());

  // Mock upcoming events coming in.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // Close the event list view.
  GestureTapOn(close_button());
  ASSERT_FALSE(event_list_view());

  // Up next should be showing.
  EXPECT_TRUE(up_next_view());

  // When the event list view closes, the scrollview max height should have been
  // clipped back to the right height for the up next view.
  const int expected_max_height = scroll_view()->height() -
                                  up_next_view()->GetPreferredSize().height() +
                                  calendar_utils::kUpNextOverlapInPx;
  EXPECT_EQ(expected_max_height, scroll_view()->GetMaxHeight());
}

// Tests the following:
// - 1 upcoming events are displayed in the up next view
// - Scroll down a couple of months
// - Up next view should be invisible after scrolling
// - Scroll back to today's month, and the up next view should remain invisible.
TEST_F(CalendarViewWithUpNextViewTest, ShouldNotShowUpNextView_AfterScrolling) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  // Mock upcoming events coming in.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // Up next view should be shown.
  EXPECT_TRUE(up_next_view());

  // Scroll down a couple of months so we're not on today's date or month any
  // more.
  ScrollDownOneMonthWithAnimation();
  ScrollDownOneMonthWithAnimation();

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January", GetCurrentLabelText());
  EXPECT_EQ(u"February", GetNextLabelText());
  EXPECT_EQ(u"March", GetNextNextLabelText());
  EXPECT_EQ(u"January", month_header()->GetText());
  EXPECT_EQ(u"2022", header_year()->GetText());

  // `up_next_view()` still exists, just invisible.
  EXPECT_FALSE(up_next_view()->GetVisible());

  // Scroll up back to today's month, and `up_next_view()` should not be visible
  // after scrolling.
  ScrollUpOneMonthWithAnimation();
  ScrollUpOneMonthWithAnimation();
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
  EXPECT_FALSE(up_next_view()->GetVisible());
}

// Tests the following:
// - 1 upcoming events are displayed in the up next view
// - Scroll down a couple of months
// - Up next view should be invisible after scrolling
// - Clicking the reset to today button
// - Up next view should be visible
TEST_F(CalendarViewWithUpNextViewTest,
       ShouldShowUpNextView_AfterResettingToToday) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE(up_next_view());

  // Scroll down a couple of months so we're not on today's date or month any
  // more.
  ScrollDownOneMonthWithAnimation();
  ScrollDownOneMonthWithAnimation();

  EXPECT_EQ(u"December", GetPreviousLabelText());
  EXPECT_EQ(u"January", GetCurrentLabelText());
  EXPECT_EQ(u"February", GetNextLabelText());
  EXPECT_EQ(u"March", GetNextNextLabelText());
  EXPECT_EQ(u"January", month_header()->GetText());
  EXPECT_EQ(u"2022", header_year()->GetText());

  // `up_next_view()` still exists, just invisible.
  EXPECT_FALSE(up_next_view()->GetVisible());

  // Clicks the reset to today button and `up_next_view()` should be visible.
  GestureTapOn(reset_to_today_button());
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());
  EXPECT_TRUE(up_next_view()->GetVisible());
}

// Tests the following:
// - 1 upcoming events are displayed in the up next view
// - Open the event list view and scroll down
// - Close the event list view
// - Up next view should be invisible
TEST_F(CalendarViewWithUpNextViewTest,
       ShouldNotShowUpNextView_WhenEventListHasClosedAfterScrolling) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE(up_next_view());

  // Open the event list view.
  ASSERT_EQ(u"25",
            static_cast<views::LabelButton*>(current_month()->children()[25])
                ->GetText());
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[25]));
  ASSERT_TRUE(event_list_view());

  // Scroll down one row with the event list open.
  ScrollDownOneMonthWithAnimation();

  EXPECT_EQ(u"October", GetPreviousLabelText());
  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"December", GetNextLabelText());
  EXPECT_EQ(u"January", GetNextNextLabelText());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());

  // Close the event list view.
  GestureTapOn(close_button());
  ASSERT_FALSE(event_list_view());

  // `up_next_view()` still exists, just invisible.
  EXPECT_FALSE(up_next_view()->GetVisible());
}

// Tests the following:
// - 1 upcoming events are displayed in the up next view
// - Open the event list view
// - Close the event list view
// - Up next view should be visible
TEST_F(CalendarViewWithUpNextViewTest,
       ShouldShowUpNextView_WhenEventListHasClosedWithoutScrolling) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  EXPECT_TRUE(up_next_view());

  // Open the event list view for today.
  ASSERT_EQ(u"18",
            static_cast<views::LabelButton*>(current_month()->children()[18])
                ->GetText());
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[18]));
  ASSERT_TRUE(event_list_view());

  // Close the event list view.
  GestureTapOn(close_button());
  ASSERT_FALSE(event_list_view());

  // `up_next_view()` should be visible.
  EXPECT_TRUE(up_next_view()->GetVisible());
}

// Tests an upcoming event that starts at 00:05 but "now" is 23:55. In this case
// the up next view shouldn't be shown.
TEST_F(CalendarViewWithUpNextViewTest,
       ShouldNotShowUpNextView_WhenNextEventStartsTomorrow) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 23:55 GMT", &date));

  // Sets the timezone to GMT.
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Sets time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  // Mock upcoming events coming in.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListStartingFivePastMidnight());

  // Up next view should not be shown.
  ASSERT_FALSE(up_next_view());
}

TEST_F(
    CalendarViewWithUpNextViewTest,
    ShouldFocusEventListCloseButton_WhenEventListViewLaunchedFromUpNextView) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  ASSERT_TRUE(up_next_view());

  auto* focus_manager = calendar_view()->GetFocusManager();
  up_next_todays_events_button()->RequestFocus();
  ASSERT_EQ(up_next_todays_events_button(), focus_manager->GetFocusedView());

  PressEnter();
  ASSERT_TRUE(event_list_view());

  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());
}

TEST_F(CalendarViewWithUpNextViewTest,
       ShouldFocusEventListCloseButton_WhenFocusMovedFromTodayButton) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // When fetched events are in the next 10 mins, then up next should have been
  // created.
  ASSERT_TRUE(up_next_view());

  auto* focus_manager = calendar_view()->GetFocusManager();
  reset_to_today_button()->RequestFocus();
  ASSERT_EQ(reset_to_today_button(), focus_manager->GetFocusedView());
  GestureTapOn(up_next_todays_events_button());

  ASSERT_TRUE(event_list_view());
  EXPECT_EQ(focus_manager->GetFocusedView(), close_button());
}

TEST_F(CalendarViewWithUpNextViewTest, RecordEventsDisplayedToUserOnce) {
  base::HistogramTester histogram_tester;
  base::Time now;
  ASSERT_TRUE(base::Time::FromString("1 Oct 2021 10:00 GMT", &now));
  base::Time next_month;
  ASSERT_TRUE(base::Time::FromString("1 Nov 2021 10:00 GMT", &next_month));
  // Set time override.
  SetFakeNow(now);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateCalendarView();
  // Mock events will be in the `next_month` so when the calendar opens we don't
  // record the metric.
  MockEventsFetched(
      calendar_utils::GetStartOfMonthUTC(next_month),
      CreateMockEventListWithEventStartTimeMoreThanTwoHoursAway());

  // Make sure we're on the current month and no events have been displayed to
  // the user.
  EXPECT_EQ(u"October", GetCurrentLabelText());
  EXPECT_EQ(u"October", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventsDisplayedToUser", 0);

  // Scroll down a month, this month should contain events.
  ScrollDownOneMonth();

  EXPECT_EQ(u"November", GetCurrentLabelText());
  EXPECT_EQ(u"November", month_header()->GetText());
  EXPECT_EQ(u"2021", header_year()->GetText());
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventsDisplayedToUser", 1);

  // Scroll back and forward to land on the month containing events again.
  ScrollDownOneMonth();
  ScrollUpOneMonth();

  // We should still have only logged the metric once.
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventsDisplayedToUser", 1);
}

TEST_F(CalendarViewWithUpNextViewTest, ShouldShowUpNextWithCachedData) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  // Set time override.
  SetFakeNow(date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  // Build the Calendar view.
  CreateCalendarView();

  // Populate model with events.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithEventStartTimeTenMinsAway());

  // Up next should be displayed (with cached events).
  EXPECT_TRUE(up_next_view());
  EXPECT_EQ(size_t(1), up_next_scroll_contents()->children().size());

  // Replace the cached data with new data.
  Shell::Get()->system_tray_model()->calendar_model()->ClearAllCachedEvents();
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateMockEventListWithTwoEventsOneEndingInOneMin());
  EXPECT_TRUE(up_next_view());
  EXPECT_EQ(size_t(2), up_next_scroll_contents()->children().size());
}

class CalendarViewWithUpNextViewAnimationTest
    : public CalendarViewAnimationTest {
 public:
  CalendarViewWithUpNextViewAnimationTest() = default;
  CalendarViewWithUpNextViewAnimationTest(
      const CalendarViewWithUpNextViewAnimationTest&) = delete;
  CalendarViewWithUpNextViewAnimationTest& operator=(
      const CalendarViewWithUpNextViewAnimationTest&) = delete;
  ~CalendarViewWithUpNextViewAnimationTest() override = default;

  std::unique_ptr<google_apis::calendar::EventList> CreateUpcomingEvents(
      base::Time date) {
    const auto start_time = date + base::Minutes(5);
    const auto end_time = start_time + base::Hours(1);
    auto event_list = std::make_unique<google_apis::calendar::EventList>();
    event_list->set_time_zone("Greenwich Mean Time");
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_0", "summary_0", start_time, end_time));

    return event_list;
  }

  void MockEventsFetched(
      base::Time date,
      std::unique_ptr<google_apis::calendar::EventList> event_list) {
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(date),
        google_apis::calendar::kPrimaryCalendarId,
        google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());
  }

  void MockOnCalendarListFetched() {
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
    calendars.push_back(calendar_test_utils::CreateCalendar(
        kCalendarId1, kCalendarSummary1, kCalendarColorId1, kCalendarSelected1,
        kCalendarPrimary1));
    std::unique_ptr<google_apis::calendar::CalendarList> calendar_list =
        calendar_test_utils::CreateMockCalendarList(std::move(calendars));
    Shell::Get()
        ->system_tray_model()
        ->calendar_list_model()
        ->OnCalendarListFetched(google_apis::ApiErrorCode::HTTP_SUCCESS,
                                std::move(calendar_list));
  }
};

INSTANTIATE_TEST_SUITE_P(MultiCalendar,
                         CalendarViewWithUpNextViewAnimationTest,
                         testing::Bool());

TEST_P(CalendarViewWithUpNextViewAnimationTest,
       UpNextViewShouldNotCoversToday) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  // Pick a date towards the end of the month so it's initial position is not on
  // the top when it's created.
  ASSERT_TRUE(base::Time::FromString("25 Apr 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());

  CreateCalendarView();
  // Force the size of the calendar to be small enough that the bottom row of
  // date cells will be covered by the up next view.
  widget()->SetFullscreen(false);
  widget()->SetSize(gfx::Size(kTrayMenuWidth, 350));
  const int initial_scroll_position = scroll_view()->GetVisibleRect().y();

  EXPECT_EQ(initial_scroll_position, GetPositionOfToday());

  // Today should be visible.
  bool todays_date_cell_is_visible =
      scroll_view()->GetBoundsInScreen().Intersects(
          calendar_view()
              ->calendar_view_controller()
              ->todays_date_cell_view()
              ->GetBoundsInScreen());
  EXPECT_TRUE(todays_date_cell_is_visible);

  // Fetch an upcoming event so up next is displayed.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date));

  // Wait for the show up next animation and smooth scrolling to complete.
  EXPECT_TRUE(calendar_view()->up_next_view());
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(calendar_view()->up_next_view()->layer());
  animation_waiter.Wait(current_month()->layer());

  // After the up next view is shown, the scroll view should not have moved.
  EXPECT_EQ(initial_scroll_position, scroll_view()->GetVisibleRect().y());
  todays_date_cell_is_visible = scroll_view()->GetBoundsInScreen().Intersects(
      calendar_view()
          ->calendar_view_controller()
          ->todays_date_cell_view()
          ->GetBoundsInScreen());
  EXPECT_TRUE(todays_date_cell_is_visible);
}

TEST_P(CalendarViewWithUpNextViewAnimationTest,
       ShouldNotScrollToShowTodaysCell_WhenUpNextViewDoesNotCoverIt) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  // Pick a date at the start of the month so up next doesn't cover it.
  ASSERT_TRUE(base::Time::FromString("1 Apr 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  CreateCalendarView();
  // Force the size of the calendar to be small enough that the bottom row of
  // date cells will be covered by the up next view.
  widget()->SetFullscreen(false);
  widget()->SetSize(gfx::Size(kTrayMenuWidth, 350));

  const int initial_scroll_position = scroll_view()->GetVisibleRect().y();

  // Fetch an upcoming event so up next is displayed.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date));

  // Wait for the show up next animation to complete.
  EXPECT_TRUE(calendar_view()->up_next_view());
  ui::LayerAnimationStoppedWaiter().Wait(
      calendar_view()->up_next_view()->layer());

  // After the up next view is shown, the scroll view should not have moved as
  // the today date cell should remain visible.
  EXPECT_EQ(initial_scroll_position, scroll_view()->GetVisibleRect().y());
  const bool todays_date_cell_is_visible =
      scroll_view()->GetBoundsInScreen().Intersects(
          calendar_view()
              ->calendar_view_controller()
              ->todays_date_cell_view()
              ->GetBoundsInScreen());
  EXPECT_TRUE(todays_date_cell_is_visible);
}

TEST_P(
    CalendarViewWithUpNextViewAnimationTest,
    ShouldNotScrollToShowTodaysCell_WhenUserHasScrolled_AndAnUpcomingEventAppears) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  // Pick a date towards the end of the month so up next covers the bottom row.
  ASSERT_TRUE(base::Time::FromString("25 Apr 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  CreateCalendarView();
  // Force the size of the calendar to be small enough that the bottom row of
  // date cells will be covered by the up next view.
  widget()->SetFullscreen(false);
  widget()->SetSize(gfx::Size(kTrayMenuWidth, 350));

  if (IsMultiCalendarEnabled()) {
    MockOnCalendarListFetched();
    WaitUntilFetched();
  }

  // Fetch an event that starts in 11 mins and up next will show.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date + base::Minutes(6)));
  WaitUntilFetched();
  EXPECT_TRUE(calendar_view()->up_next_view()->GetVisible());

  // Scroll up a bit so that today is off the screen.
  ScrollUpOneMonth();
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(current_month()->layer());

  const int initial_scroll_position = scroll_view()->GetVisibleRect().y();

  // Now advance time so that our upcoming meeting is about to start and up next
  // should be invisible since the user has scrolled.
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(calendar_view()->up_next_view()->GetVisible());

  // The scroll view should not have moved as the user has previously interacted
  // with the scroll view.
  EXPECT_EQ(initial_scroll_position, scroll_view()->GetVisibleRect().y());
}

TEST_P(CalendarViewWithUpNextViewAnimationTest,
       ShouldNotScroll_WhenAnUpcomingEventAppears) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  // Pick a date towards the end of the month so up next covers the bottom row.
  ASSERT_TRUE(base::Time::FromString("25 Apr 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  CreateCalendarView();
  // Force the size of the calendar to be small enough that the bottom row of
  // date cells will be covered by the up next view.
  widget()->SetFullscreen(false);
  widget()->SetSize(gfx::Size(kTrayMenuWidth, 350));

  // Fetch an event that starts in 11 mins and up next will show.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date + base::Minutes(6)));
  EXPECT_TRUE(calendar_view()->up_next_view());

  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(current_month()->layer());
  const int initial_scroll_position = scroll_view()->GetVisibleRect().y();

  // The initial position show be the position of today's row.
  EXPECT_EQ(initial_scroll_position, GetPositionOfToday());

  // Now advance time so that our upcoming meeting is about to start and up next
  // should not appear since the user has scrolled.
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(calendar_view()->up_next_view());
  animation_waiter.Wait(calendar_view()->up_next_view()->layer());

  // The scroll view should have not moved to show the `up_next_view_`.
  EXPECT_EQ(initial_scroll_position, scroll_view()->GetVisibleRect().y());
}

TEST_P(CalendarViewWithUpNextViewAnimationTest,
       ShouldNotScrollToShowTodaysCell_WhenTodaysDateCellIsNull) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  // Pick a date towards the end of the month.
  ASSERT_TRUE(base::Time::FromString("25 Apr 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  CreateCalendarView();
  // Force the size of the calendar to be small enough that the bottom row of
  // date cells will be covered by the up next view.
  widget()->SetFullscreen(false);
  widget()->SetSize(gfx::Size(kTrayMenuWidth, 350));

  if (IsMultiCalendarEnabled()) {
    MockOnCalendarListFetched();
    WaitUntilFetched();
  }

  // Fetch an event that starts in 11 mins and up next view will be shown.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date + base::Minutes(6)));
  WaitUntilFetched();
  EXPECT_TRUE(calendar_view()->up_next_view()->GetVisible());

  // Scrolls 4 months so that todays date cell is null.
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  ScrollUpOneMonth();
  EXPECT_EQ(
      calendar_view()->calendar_view_controller()->todays_date_cell_view(),
      nullptr);

  const int initial_scroll_position = scroll_view()->GetVisibleRect().y();

  // Now advance time so that our upcoming meeting is about to start and up next
  // shouldn't show since the user has scrolled, and the scroll view should not
  // have moved as well.
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(calendar_view()->up_next_view()->GetVisible());
  EXPECT_EQ(initial_scroll_position, scroll_view()->GetVisibleRect().y());
}

// Tests that the scroll view scrolls up to today's row without the up-next
// view.
TEST_P(CalendarViewWithUpNextViewAnimationTest,
       ShouldScrollToToday_WithoutUpNextView) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;

  // Pick a date towards the end of the month so that it need to scroll up to
  // today's row.
  ASSERT_TRUE(base::Time::FromString("30 Nov 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  SetTodayFromTime(date);
  CreateCalendarView();

  // After the view is settled, it should scroll to today's row.
  EXPECT_EQ(GetPositionOfToday(), scroll_view()->GetVisibleRect().y());
}

// Tests that the scroll view scrolls up to today's row with the up-next view.
TEST_P(CalendarViewWithUpNextViewAnimationTest,
       ShouldScrollToToday_WithUpNextView) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  // Pick a date towards the end of the month so that it need to scroll up to
  // today's row.
  ASSERT_TRUE(base::Time::FromString("30 Nov 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  SetTodayFromTime(date);
  CreateCalendarView();

  // Fetch an event that starts in 6 mins and up next will show.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date + base::Minutes(6)));
  EXPECT_TRUE(calendar_view()->up_next_view());

  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(current_month()->layer());
  animation_waiter.Wait(calendar_view()->up_next_view()->layer());

  // After the view is settled, it should scroll to today's row.
  EXPECT_EQ(GetPositionOfToday(), scroll_view()->GetVisibleRect().y());
}

// Tests that the up-next view can show up after showing the event listview
// first. Regression test for b/336722659.
TEST_P(CalendarViewWithUpNextViewAnimationTest,
       ShowUpNextViewAfterEventListViewCorrectly) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("30 Nov 2023 10:00 GMT", &date));
  task_environment()->AdvanceClock(date - base::Time::Now());
  SetTodayFromTime(date);
  CreateCalendarView();
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(current_month()->layer());
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // There's no `up_next_view()` before the events are fetched.
  EXPECT_FALSE(calendar_view()->up_next_view());

  // Open the event list view for today.
  ASSERT_EQ(u"30",
            static_cast<views::LabelButton*>(current_month()->children()[32])
                ->GetText());
  GestureTapOn(
      static_cast<views::LabelButton*>(current_month()->children()[32]));

  animation_waiter.Wait(calendar_sliding_surface_view()->layer());
  animation_waiter.Wait(current_label()->layer());
  ASSERT_TRUE(event_list_view());
  EXPECT_TRUE(event_list_view()->GetVisible());
  task_environment()->FastForwardBy(
      calendar_test_utils::kAnimationSettleDownDuration);

  // Fetch an event that starts in 6 mins.
  MockEventsFetched(calendar_utils::GetStartOfMonthUTC(date),
                    CreateUpcomingEvents(date + base::Minutes(6)));
  EXPECT_FALSE(calendar_view()->up_next_view());

  // Close the event list view.
  CloseEventList();
  animation_waiter.Wait(calendar_sliding_surface_view()->layer());
  animation_waiter.Wait(current_label()->layer());
  EXPECT_FALSE(event_list_view());

  // `up_next_view()` should be visible.
  EXPECT_TRUE(calendar_view()->up_next_view()->GetVisible());
}

}  // namespace ash
