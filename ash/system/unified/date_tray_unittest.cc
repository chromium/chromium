// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/date_tray.h"

#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace {
constexpr char kUserEmail[] = "test_user@gmail.com";
}

namespace ash {

class DateTrayTest
    : public AshTestBase,
      public wm::ActivationChangeObserver,
      public testing::WithParamInterface</*glanceables_v2_enabled=*/bool> {
 public:
  DateTrayTest() {
    scoped_feature_list_.InitWithFeatureState(features::kGlanceablesV2,
                                              GetParam());
  }

  DateTrayTest(const DateTrayTest&) = delete;
  DateTrayTest& operator=(const DateTrayTest&) = delete;
  ~DateTrayTest() override = default;

  void SetUp() override {
    // Set time override.
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time date;
          bool result = base::Time::FromString("24 Aug 2021 10:00 GMT", &date);
          DCHECK(result);
          return date;
        },
        /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);

    AshTestBase::SetUp();

    SimulateUserLogin(GetAccountId(kUserEmail));

    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(std::make_unique<views::View>());
    widget_->SetFullscreen(true);
    date_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
    unified_system_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                               ->unified_system_tray();
    widget_->GetContentsView()->AddChildView(date_tray_.get());
    widget_->GetContentsView()->AddChildView(unified_system_tray_.get());
    date_tray_->SetVisiblePreferred(true);
    date_tray_->unified_system_tray_->SetVisiblePreferred(true);

    if (AreGlanceablesV2Enabled()) {
      fake_glanceables_tasks_client_ =
          std::make_unique<FakeGlanceablesTasksClient>();
      Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
          GetAccountId(kUserEmail),
          GlanceablesV2Controller::ClientsRegistration{
              .tasks_client = fake_glanceables_tasks_client_.get()});
    }
  }

  void TearDown() override {
    if (AreGlanceablesV2Enabled()) {
      fake_glanceables_tasks_client_.reset();
    }

    widget_.reset();
    date_tray_ = nullptr;
    if (observering_activation_changes_) {
      Shell::Get()->activation_client()->RemoveObserver(this);
    }
    AshTestBase::TearDown();
  }

  bool AreGlanceablesV2Enabled() { return GetParam(); }

  DateTray* GetDateTray() { return date_tray_; }

  UnifiedSystemTray* GetUnifiedSystemTray() {
    return date_tray_->unified_system_tray_;
  }

  GlanceableTrayBubble* GetGlanceableTrayBubble() {
    return date_tray_->bubble_.get();
  }

  bool IsBubbleShown() {
    if (AreGlanceablesV2Enabled()) {
      return !!GetGlanceableTrayBubble();
    }
    return GetUnifiedSystemTray()->IsBubbleShown();
  }

  bool AreContentsViewShown() {
    if (AreGlanceablesV2Enabled()) {
      return !!GetGlanceableTrayBubble();
    }
    return GetUnifiedSystemTray()->IsShowingCalendarView();
  }

  void LeftClickOnOpenBubble() {
    if (AreGlanceablesV2Enabled()) {
      LeftClickOn(GetGlanceableTrayBubble()->GetBubbleView());

    } else {
      LeftClickOn(GetUnifiedSystemTray()->bubble()->GetBubbleView());
    }
  }

  std::u16string GetTimeViewText() {
    return date_tray_->time_view_->time_view()
        ->horizontal_label_date_for_test()
        ->GetText();
  }

  void ImmediatelyCloseBubbleOnActivation() {
    Shell::Get()->activation_client()->AddObserver(this);
    observering_activation_changes_ = true;
  }

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    GetUnifiedSystemTray()->CloseBubble();
  }

  AccountId GetAccountId(base::StringPiece email) {
    return AccountId::FromUserEmail(std::string(email));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FakeGlanceablesTasksClient> fake_glanceables_tasks_client_;
  bool observering_activation_changes_ = false;

  // Owned by `widget_`.
  raw_ptr<DateTray, ExperimentalAsh> date_tray_ = nullptr;

  raw_ptr<UnifiedSystemTray, ExperimentalAsh> unified_system_tray_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(GlanceablesV2, DateTrayTest, testing::Bool());

// Tests that toggling the `CalendarView` via the date tray accelerator does not
// result in a crash when the unified system tray bubble is set to immediately
// close upon activation. See crrev/c/1419499 for details.
TEST_P(DateTrayTest, AcceleratorOpenAndImmediateCloseDoesNotCrash) {
  ImmediatelyCloseBubbleOnActivation();
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_FALSE(IsBubbleShown());
}

// Test the initial state.
TEST_P(DateTrayTest, InitialState) {
  // Show the mock time now Month and day.
  EXPECT_EQ(u"Aug 24", GetTimeViewText());

  // Initial state: not showing the calendar bubble.
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(AreContentsViewShown());
}

TEST_P(DateTrayTest, DISABLED_ShowTasksComboModel) {
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_EQ(GetGlanceableTrayBubble(), nullptr);
  } else {
    EXPECT_TRUE(GetGlanceableTrayBubble()->GetTasksView()->GetVisible());
    EXPECT_FALSE(GetGlanceableTrayBubble()->GetTasksView()->IsMenuRunning());
    EXPECT_TRUE(GetGlanceableTrayBubble()
                    ->GetTasksView()
                    ->task_list_combo_box_view()
                    ->GetVisible());
    GestureTapOn(
        GetGlanceableTrayBubble()->GetTasksView()->task_list_combo_box_view());
    PressAndReleaseKey(ui::VKEY_DOWN);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(GetGlanceableTrayBubble()->GetTasksView()->IsMenuRunning());
  }
}

// Tests clicking/tapping the DateTray shows/closes the calendar bubble.
TEST_P(DateTrayTest, ShowCalendarBubble) {
  base::HistogramTester histogram_tester;
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  histogram_tester.ExpectTotalCount("Ash.Calendar.ShowSource.TimeView",
                                    AreGlanceablesV2Enabled() ? 0 : 1);

  // Clicking on the `DateTray` again -> close the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());

  // Tapping on the `DateTray` again -> open the calendar bubble.
  GestureTapOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  histogram_tester.ExpectTotalCount("Ash.Calendar.ShowSource.TimeView",
                                    AreGlanceablesV2Enabled() ? 0 : 2);

  // Tapping on the `DateTray` again -> close the calendar bubble.
  GestureTapOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
}

// Tests the behavior when clicking on different areas.
TEST_P(DateTrayTest, ClickingArea) {
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Clicking on the bubble area -> not close the calendar bubble.
  LeftClickOnOpenBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Clicking on the `UnifiedSystemTray` -> switch to QS bubble.
  LeftClickOn(GetUnifiedSystemTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetUnifiedSystemTray()->IsBubbleShown());
  EXPECT_TRUE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());

  // Clicking on the `DateTray` -> switch to the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Clicking on `DateTray` closes the bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
}

TEST_P(DateTrayTest, EscapeKeyForClose) {
  base::HistogramTester histogram_tester;
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  histogram_tester.ExpectTotalCount("Ash.Calendar.ShowSource.TimeView",
                                    AreGlanceablesV2Enabled() ? 0 : 1);

  // Hitting escape key -> close and deactivate the calendar bubble.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
}

// Tests that calling `DateTray::CloseBubble()` actually closes the bubble.
TEST_P(DateTrayTest, CloseBubble) {
  ASSERT_FALSE(IsBubbleShown());
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Calling `DateTray::CloseBubble()` should close the bubble.
  GetDateTray()->CloseBubble();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());

  // Calling `DateTray::CloseBubble()` on an already-closed bubble should do
  // nothing.
  GetDateTray()->CloseBubble();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
}

}  // namespace ash
