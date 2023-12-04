// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/system_textfield.h"
#include "ash/system/focus_mode/focus_mode_chip_carousel.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_countdown_view.h"
#include "ash/system/focus_mode/focus_mode_feature_pod_controller.h"
#include "ash/system/focus_mode/focus_mode_task_view.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/time_formatting.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);

}  // namespace

class FocusModeDetailedViewTest : public AshTestBase {
 public:
  FocusModeDetailedViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_feature_(features::kFocusMode) {}
  ~FocusModeDetailedViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    // Focus Mode considers it to be a first time user flow if
    // `kFocusModeDoNotDisturb` has never been set by the user before. For
    // normal feature testing purposes, we will intentionally set it so that the
    // pref will not be marked as using the default value.
    prefs()->SetBoolean(prefs::kFocusModeDoNotDisturb, true);

    CreateFakeFocusModeDetailedView();
  }

  void TearDown() override {
    focus_mode_detailed_view_ = nullptr;
    widget_.reset();

    AshTestBase::TearDown();
  }

  void CreateFakeFocusModeDetailedView() {
    auto focus_mode_detailed_view =
        std::make_unique<FocusModeDetailedView>(&detailed_view_delegate_);
    focus_mode_detailed_view_ = focus_mode_detailed_view.get();

    widget_->SetContentsView(std::move(focus_mode_detailed_view));
  }

  void SetInactiveSessionDuration(SystemTextfield* timer_textfield) {
    DCHECK(!FocusModeController::Get()->in_focus_session());
    focus_mode_detailed_view_->SetInactiveSessionDuration(base::Minutes(
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield)));
  }

  views::Label* GetToggleRowLabel() {
    return focus_mode_detailed_view_->toggle_view_->text_label();
  }

  views::Label* GetToggleRowSubLabel() {
    return focus_mode_detailed_view_->toggle_view_->sub_text_label();
  }

  bool IsToggleRowSubLabelVisible() {
    return GetToggleRowSubLabel() && GetToggleRowSubLabel()->GetVisible();
  }

  PillButton* GetToggleRowButton() {
    return views::AsViewClass<PillButton>(
        focus_mode_detailed_view_->toggle_view_->right_view());
  }

  views::BoxLayoutView* GetTimerSettingView() {
    return focus_mode_detailed_view_->timer_setting_view_;
  }

  SystemTextfield* GetTimerSettingTextfield() {
    CHECK(!FocusModeController::Get()->in_focus_session());
    return focus_mode_detailed_view_->timer_textfield_;
  }

  IconButton* GetTimerSettingIncrementButton() {
    return focus_mode_detailed_view_->timer_increment_button_;
  }

  IconButton* GetTimerSettingDecrementButton() {
    return focus_mode_detailed_view_->timer_decrement_button_;
  }

  RoundedContainer* GetDoNotDisturbContainer() {
    return focus_mode_detailed_view_->do_not_disturb_view_;
  }

  Switch* GetDoNotDisturbToggleButton() {
    CHECK(!FocusModeController::Get()->in_focus_session());
    return focus_mode_detailed_view_->do_not_disturb_toggle_button_;
  }

  FocusModeCountdownView* GetTimerCountdownView() {
    return focus_mode_detailed_view_->timer_countdown_view_;
  }

  views::Label* GetEndTimeLabel() {
    return focus_mode_detailed_view_->end_time_label_;
  }

  PrefService* prefs() {
    return Shell::Get()->session_controller()->GetActivePrefService();
  }

  FocusModeTaskView* GetTaskView() {
    return focus_mode_detailed_view_->focus_mode_task_view_;
  }

  RoundedContainer* GetTaskContainerView() {
    return focus_mode_detailed_view_->task_view_container_;
  }

  FakeDetailedViewDelegate detailed_view_delegate_;

 private:
  base::test::ScopedFeatureList scoped_feature_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<FocusModeDetailedView> focus_mode_detailed_view_ = nullptr;
};

// Tests that the DND in Quick Settings is off and the toggle button is on/off
// respectively.
TEST_F(FocusModeDetailedViewTest, DndOffBeforeStart) {
  auto* message_center = message_center::MessageCenter::Get();
  auto* focus_mode_controller = FocusModeController::Get();
  Switch* toggle_button = GetDoNotDisturbToggleButton();

  // 1. Before turning on a focus session, the system do not disturb is off. The
  // default value for the toggle button is set to enabled.
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_TRUE(toggle_button->GetIsOn());

  // Start a focus session and verify that quiet mode is on.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());

  // End the focus session. The system do not disturb will be back to its
  // original state at the end of the current focus session. The toggle button's
  // state will be back to its state before the focus session.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_TRUE(toggle_button->GetIsOn());

  // 2. Before turning on a focus session, the system do not disturb is off. The
  // default value for the toggle button is set to disabled.
  LeftClickOn(toggle_button);
  EXPECT_FALSE(toggle_button->GetIsOn());

  // Start a focus session and verify that quiet mode is off.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());

  // End the focus session. The system do not disturb will be back to its
  // original state at the end of the current focus session. The toggle button's
  // state will be back to its state before the focus session.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_FALSE(toggle_button->GetIsOn());
}

// Tests that the DND in Quick Settings is on and the toggle button is on/off
// respectively. We also test the behavior for user interactions during a focus
// session.
TEST_F(FocusModeDetailedViewTest, DndOnBeforeStart) {
  auto* message_center = message_center::MessageCenter::Get();
  auto* focus_mode_controller = FocusModeController::Get();
  Switch* toggle_button = GetDoNotDisturbToggleButton();

  // 1. Before turning on a focus session, the system do not disturb is on. The
  // default value for the toggle button is set to enabled.
  message_center->SetQuietMode(true);
  EXPECT_TRUE(message_center->IsQuietMode());

  EXPECT_TRUE(toggle_button->GetIsOn());

  // Start a focus session and verify that quiet mode is on.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());

  // During the focus session, the user turned off the DND.
  message_center->SetQuietMode(false);
  EXPECT_FALSE(message_center->IsQuietMode());

  // End the focus session. The system do not disturb will keep disabled at the
  // end of the current focus session. The toggle button's state will be back to
  // its state before the focus session.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_TRUE(toggle_button->GetIsOn());

  // 2. Before turning on a focus session, the system do not disturb is on. The
  // default value for the toggle button is set to disabled.
  message_center->SetQuietMode(true);
  EXPECT_TRUE(message_center->IsQuietMode());

  LeftClickOn(toggle_button);
  EXPECT_FALSE(toggle_button->GetIsOn());

  // Start a focus session and verify that quiet mode is on.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());

  // End the focus session. The system do not disturb will be back to its
  // original state at the end of the current focus session. The toggle button's
  // state will be back to its state before the focus session.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());
  EXPECT_FALSE(toggle_button->GetIsOn());
}

// Tests label texts and start/stop functionalities for the toggle row.
TEST_F(FocusModeDetailedViewTest, ToggleRow) {
  auto* focus_mode_controller = FocusModeController::Get();

  auto validate_labels = [&](bool active) {
    EXPECT_EQ(active, focus_mode_controller->in_focus_session());
    EXPECT_EQ(active ? u"Focusing" : u"Focus", GetToggleRowLabel()->GetText());

    EXPECT_EQ(active, IsToggleRowSubLabelVisible());

    if (active) {
      EXPECT_EQ(focus_mode_util::GetFormattedEndTimeString(
                    focus_mode_controller->end_time()),
                GetToggleRowSubLabel()->GetText());
    }
    EXPECT_EQ(active ? u"End Focus" : u"Start",
              GetToggleRowButton()->GetText());
  };

  validate_labels(/*active=*/false);

  // Starting the focus session closes the bubble, so we need to simulate
  // recreating the detailed view.
  LeftClickOn(GetToggleRowButton());
  CreateFakeFocusModeDetailedView();

  // Wait a minute to test that the time remaining label updates.
  task_environment()->FastForwardBy(base::Seconds(61));
  validate_labels(/*active=*/true);

  LeftClickOn(GetToggleRowButton());
  validate_labels(/*active=*/false);

  // Verify that the time displays correctly in the 24-hour clock format.
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  // Starting the focus session closes the bubble, so we need to simulate
  // recreating the detailed view.
  LeftClickOn(GetToggleRowButton());
  CreateFakeFocusModeDetailedView();

  // Wait a second to avoid the time remaining being either 1500 seconds or
  // 1499.99 seconds.
  task_environment()->FastForwardBy(base::Seconds(1));
  validate_labels(/*active=*/true);

  LeftClickOn(GetToggleRowButton());
  validate_labels(/*active=*/false);
}

// Tests how the textfield for the timer setting view handles valid and invalid
// inputs.
TEST_F(FocusModeDetailedViewTest, TimerSettingViewTextfield) {
  SystemTextfield* timer_textfield = GetTimerSettingTextfield();
  LeftClickOn(timer_textfield);
  ASSERT_TRUE(timer_textfield->IsActive());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DELETE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_3);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  EXPECT_EQ(u"30", timer_textfield->GetText());

  // Test that we can not put in non-numeric characters.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_OEM_PERIOD);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_OEM_PLUS);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE);
  EXPECT_EQ(u"30", timer_textfield->GetText());

  // Try pressing return with no text inside. Should return text to previous
  // value.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DELETE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DELETE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_EQ(u"30", timer_textfield->GetText());

  // Try setting an invalid value.
  LeftClickOn(timer_textfield);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_3);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_3);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_3);
  EXPECT_EQ(u"333", timer_textfield->GetText());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_EQ(u"300", timer_textfield->GetText());
}

// Tests that incrementing the duration of an inactive focus session follows a
// pattern where, when we are starting with a value of:
// - 1 through 4, inclusive, will lead to an increment of 1.
// - 5 through 59, inclusive, will lead to an increment to the next multiple
//   of 5.
// - 60 through 299, inclusive, will lead to an increment to the next multiple
//   of 15.
// - 300, we will not increment further.
TEST_F(FocusModeDetailedViewTest, TimerSettingViewIncrements) {
  SystemTextfield* timer_textfield = GetTimerSettingTextfield();
  IconButton* decrement_button = GetTimerSettingDecrementButton();
  IconButton* increment_button = GetTimerSettingIncrementButton();

  // Check incrementing 1 through 5.
  timer_textfield->SetText(u"1");
  SetInactiveSessionDuration(timer_textfield);

  // The `decrement_button` will be disabled only when setting the duration to
  // the minimum duration.
  EXPECT_FALSE(decrement_button->GetEnabled());
  LeftClickOn(increment_button);
  int expected_next_value = 2;
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value += 1;
    LeftClickOn(increment_button);
  }
  EXPECT_TRUE(decrement_button->GetEnabled());

  // Increment 5 to 10.
  EXPECT_EQ(u"5", timer_textfield->GetText());
  LeftClickOn(increment_button);
  EXPECT_EQ(u"10", timer_textfield->GetText());

  // Try incrementing 6 to 10, and then continue incrementing to 60.
  timer_textfield->SetText(u"6");
  LeftClickOn(increment_button);
  expected_next_value = 10;
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value += 5;
    LeftClickOn(increment_button);
  }

  // Increment 60 to 75.
  EXPECT_EQ(u"60", timer_textfield->GetText());
  LeftClickOn(increment_button);
  EXPECT_EQ(u"75", timer_textfield->GetText());

  // Try incrementing 61 to 75, and then continue incrementing to 300.
  timer_textfield->SetText(u"61");
  LeftClickOn(increment_button);
  expected_next_value = 75;
  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value += 15;
    LeftClickOn(increment_button);
  }

  // Try incrementing past 300.
  EXPECT_EQ(u"300", timer_textfield->GetText());
  LeftClickOn(increment_button);
  EXPECT_EQ(u"300", timer_textfield->GetText());
}

// Tests that decrementing the duration of an inactive focus session follows a
// pattern where, when we are starting with a value of:
// - 1, we will not decrement further
// - 2 through 5, inclusive, will lead to a decrement of 1.
// - 6 through 60, inclusive, will lead to a decrement to the next lowest
//   multiple of 5.
// - 61 through 300, inclusive, will lead to a decrement to the next lowest
//   multiple of 15.
TEST_F(FocusModeDetailedViewTest, TimerSettingViewDecrements) {
  SystemTextfield* timer_textfield = GetTimerSettingTextfield();
  IconButton* decrement_button = GetTimerSettingDecrementButton();
  IconButton* increment_button = GetTimerSettingIncrementButton();

  // Decrement 300 to 285.
  timer_textfield->SetText(u"300");
  SetInactiveSessionDuration(timer_textfield);

  // The `increment_button` will be disabled only when setting the duration to
  // the maximum duration.
  EXPECT_FALSE(increment_button->GetEnabled());
  LeftClickOn(decrement_button);
  EXPECT_EQ(u"285", timer_textfield->GetText());
  EXPECT_TRUE(increment_button->GetEnabled());

  // Try decrementing 299 to 285, and then continue decrementing to 60.
  timer_textfield->SetText(u"299");
  LeftClickOn(decrement_button);
  int expected_next_value = 285;
  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value -= 15;
    LeftClickOn(decrement_button);
  }

  // Decrement 60 to 55.
  EXPECT_EQ(u"60", timer_textfield->GetText());
  LeftClickOn(decrement_button);
  EXPECT_EQ(u"55", timer_textfield->GetText());

  // Try decrementing 59 to 55, and then continue decrementing to 5.
  timer_textfield->SetText(u"59");
  LeftClickOn(decrement_button);
  expected_next_value = 55;
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value -= 5;
    LeftClickOn(decrement_button);
  }

  // Check decrementing 5 to 1.
  EXPECT_EQ(u"5", timer_textfield->GetText());
  LeftClickOn(decrement_button);
  expected_next_value = 4;
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value -= 1;
    LeftClickOn(decrement_button);
  }

  // Try decrementing past 1.
  EXPECT_EQ(u"1", timer_textfield->GetText());
  LeftClickOn(decrement_button);
  EXPECT_EQ(u"1", timer_textfield->GetText());
}

// Tests that the timer setting view is visible outside of a focus session and
// the countdown view is visible in a focus session.
TEST_F(FocusModeDetailedViewTest, TimerViewVisibility) {
  auto* focus_mode_controller = FocusModeController::Get();
  auto* timer_setting_view = GetTimerSettingView();
  auto* countdown_view = GetTimerCountdownView();

  // Before turning on a focus session both views should exist and the setting
  // view should be visible.
  ASSERT_TRUE(countdown_view);
  ASSERT_TRUE(timer_setting_view);
  EXPECT_FALSE(countdown_view->GetVisible());
  EXPECT_TRUE(timer_setting_view->GetVisible());

  const base::TimeDelta session_duration =
      focus_mode_controller->session_duration();
  EXPECT_EQ(focus_mode_util::GetFormattedEndTimeString(base::Time::Now() +
                                                       session_duration),
            GetEndTimeLabel()->GetText());

  // Wait a minute to test that the end time label updates.
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(focus_mode_util::GetFormattedEndTimeString(base::Time::Now() +
                                                       session_duration),
            GetEndTimeLabel()->GetText());

  // In a focus session the countdown view should be visible and the timer view
  // hidden.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  // Starting the focus session closes the bubble, so we need to simulate
  // recreating the detailed view.
  CreateFakeFocusModeDetailedView();
  timer_setting_view = GetTimerSettingView();
  countdown_view = GetTimerCountdownView();
  EXPECT_TRUE(countdown_view->GetVisible());
  EXPECT_FALSE(timer_setting_view->GetVisible());

  // Turning the focus session back off should swap the visibilities again.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(countdown_view->GetVisible());
  EXPECT_TRUE(timer_setting_view->GetVisible());
  EXPECT_EQ(focus_mode_util::GetFormattedEndTimeString(base::Time::Now() +
                                                       session_duration),
            GetEndTimeLabel()->GetText());
}

// Verify that the toggle row sublabel is shown in the first time user flow.
TEST_F(FocusModeDetailedViewTest, FirstTimeUserFlow) {
  // Clear `kFocusModeDoNotDisturb` to trigger the first time user flow.
  prefs()->ClearPref(prefs::kFocusModeDoNotDisturb);

  // Recreate the detailed view so that the UI is updated after we set the user
  // pref.
  CreateFakeFocusModeDetailedView();

  // Verify that the first time user flow text is displayed.
  EXPECT_TRUE(IsToggleRowSubLabelVisible());
  EXPECT_EQ(GetToggleRowSubLabel()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_FIRST_TIME_SUBLABEL));

  // Start and stop a focus session. This puts us back into the focus panel
  // outside of the first time user flow.
  LeftClickOn(GetToggleRowButton());
  CreateFakeFocusModeDetailedView();
  LeftClickOn(GetToggleRowButton());

  // Verify that the first time user flow text no longer is displayed.
  EXPECT_FALSE(IsToggleRowSubLabelVisible());
}

// Tests that changing the duration in the detailed view while the session is
// inactive changes the duration on the feature pod.
TEST_F(FocusModeDetailedViewTest,
       InactiveSessionDurationChangeSyncsWithFeaturePod) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto controller = std::make_unique<FocusModeFeaturePodController>(
      GetPrimaryUnifiedSystemTray()
          ->bubble()
          ->unified_system_tray_controller());
  auto pod = controller->CreateTile();

  auto* timer_textfield = GetTimerSettingTextfield();
  auto textfield_text_before_increment = timer_textfield->GetText();
  LeftClickOn(GetTimerSettingIncrementButton());
  auto textfield_text_after_increment = timer_textfield->GetText();
  ASSERT_NE(textfield_text_before_increment, textfield_text_after_increment);
  EXPECT_EQ(base::StrCat({textfield_text_after_increment, u" min"}),
            pod->sub_label()->GetText());
}

// Tests that the toggle row sub label is only visible outside of a focus
// session.
TEST_F(FocusModeDetailedViewTest, SubLabelVisibility) {
  // Adjusting the focus duration should not make the sub label appear outside
  // of a focus session.
  EXPECT_FALSE(IsToggleRowSubLabelVisible());
  LeftClickOn(GetTimerSettingDecrementButton());
  EXPECT_FALSE(IsToggleRowSubLabelVisible());
  LeftClickOn(GetTimerSettingIncrementButton());
  EXPECT_FALSE(IsToggleRowSubLabelVisible());

  // The label should be visible inside of a focus session.
  auto* focus_mode_controller = FocusModeController::Get();
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  CreateFakeFocusModeDetailedView();
  EXPECT_TRUE(IsToggleRowSubLabelVisible());

  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(IsToggleRowSubLabelVisible());
}

// Tests that when the user select a task or mark a selected task as completed,
// the task view container will shrink or expand.
TEST_F(FocusModeDetailedViewTest, ExpandOrShrinkTaskViewContainer) {
  auto* task_container_view = GetTaskContainerView();
  auto* task_view = GetTaskView();
  auto* chip_carousel = task_view->chip_carousel_for_testing();
  EXPECT_TRUE(chip_carousel->HasTasks());
  EXPECT_TRUE(chip_carousel->GetVisible());

  auto* radio_button = task_view->radio_button_for_testing();
  // `radio_button` is invisible before we select a task title.
  EXPECT_FALSE(radio_button->GetVisible());
  const int old_height_before_shrink = task_container_view->bounds().height();

  // 1. Shrink the `task_container_view`.
  task_view->AddTask(u"my task title");
  views::test::RunScheduledLayout(task_container_view);
  EXPECT_TRUE(radio_button->GetVisible());
  EXPECT_FALSE(chip_carousel->GetVisible());
  EXPECT_GT(old_height_before_shrink, task_container_view->bounds().height());

  // 2. Expand the `task_container_view`.
  LeftClickOn(radio_button);
  task_environment()->FastForwardBy(kStartAnimationDelay);
  views::test::RunScheduledLayout(task_container_view);
  EXPECT_EQ(old_height_before_shrink, task_container_view->bounds().height());
}

}  // namespace ash
