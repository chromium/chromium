// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/switch.h"
#include "ash/style/system_textfield.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

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

  views::Label* GetToggleRowLabel() {
    return focus_mode_detailed_view_->toggle_view_->text_label();
  }

  views::Label* GetToggleRowSubLabel() {
    return focus_mode_detailed_view_->toggle_view_->sub_text_label();
  }

  PillButton* GetToggleRowButton() {
    return views::AsViewClass<PillButton>(
        focus_mode_detailed_view_->toggle_view_->right_view());
  }

  views::BoxLayoutView* GetTimerSettingView() {
    CHECK(!FocusModeController::Get()->in_focus_session());
    return focus_mode_detailed_view_->timer_setting_view_;
  }

  SystemTextfield* GetTimerSettingTextfield() {
    CHECK(!FocusModeController::Get()->in_focus_session());
    return focus_mode_detailed_view_->timer_textfield_;
  }

  IconButton* GetTimerSettingIncrementButton() {
    return views::AsViewClass<IconButton>(GetTimerSettingView()->children()[3]);
  }

  IconButton* GetTimerSettingDecrementButton() {
    return views::AsViewClass<IconButton>(GetTimerSettingView()->children()[2]);
  }

  Switch* GetDoNotDisturbToggleButton() {
    return focus_mode_detailed_view_->do_not_disturb_toggle_button_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_;
  std::unique_ptr<views::Widget> widget_;
  FakeDetailedViewDelegate detailed_view_delegate_;
  raw_ptr<FocusModeDetailedView> focus_mode_detailed_view_ = nullptr;
};

TEST_F(FocusModeDetailedViewTest, DoNotDisturbToggleButtonAndQuietMode) {
  auto* message_center = message_center::MessageCenter::Get();
  auto* focus_mode_controller = FocusModeController::Get();
  Switch* toggle_button = GetDoNotDisturbToggleButton();

  // Before turning on a focus session, the system do not disturb is off. The
  // default value for the toggle button is set to enabled.
  bool quiet_mode_before_focus_session = message_center->IsQuietMode();
  EXPECT_FALSE(quiet_mode_before_focus_session);

  bool turn_on_do_not_disturb_before_focus_session =
      focus_mode_controller->turn_on_do_not_disturb();
  EXPECT_TRUE(turn_on_do_not_disturb_before_focus_session);
  EXPECT_TRUE(toggle_button->GetIsOn());

  // 1. Start a focus session.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());

  // Initially, the toggle button and the quiet mode are all on.
  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_TRUE(message_center->IsQuietMode());

  // Turn off the do not disturb toggle button, the system do not disturb will
  // be off.
  LeftClickOn(toggle_button);
  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_TRUE(focus_mode_controller->turn_on_do_not_disturb());

  // Enable the system do not disturb, the do not disturb toggle button will be
  // on.
  message_center->SetQuietMode(true);
  EXPECT_TRUE(toggle_button->GetIsOn());

  // 2. End the focus session. The system do not disturb will be back to its
  // original state at the end of the current focus session. The toggle button's
  // state will be back to its state before the focus session.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_EQ(quiet_mode_before_focus_session, message_center->IsQuietMode());
  EXPECT_EQ(turn_on_do_not_disturb_before_focus_session,
            toggle_button->GetIsOn());

  // Enable and then disable the system do not disturb; the do not disturb
  // toggle button won't be changed, which will be enabled.
  message_center->SetQuietMode(true);
  message_center->SetQuietMode(false);
  EXPECT_TRUE(toggle_button->GetIsOn());

  message_center->SetQuietMode(true);
  // Turn on the toggle button, the system do not disturb won't be changed.
  LeftClickOn(toggle_button);
  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(focus_mode_controller->turn_on_do_not_disturb());
  EXPECT_TRUE(message_center->IsQuietMode());
}

// Tests label texts and start/stop functionalities for the toggle row.
TEST_F(FocusModeDetailedViewTest, ToggleRow) {
  auto* focus_mode_controller = FocusModeController::Get();

  auto validate_labels = [&](bool active) {
    EXPECT_EQ(active, focus_mode_controller->in_focus_session());
    EXPECT_EQ(active ? u"Focusing" : u"Focus", GetToggleRowLabel()->GetText());

    const base::Time now = base::Time::Now();
    const base::TimeDelta session_duration =
        focus_mode_controller->session_duration();
    const int remaining_minutes =
        active ? (focus_mode_controller->end_time() - now).InMinutes()
               : session_duration.InMinutes();

    EXPECT_EQ(base::UTF8ToUTF16(base::StringPrintf(
                  "%d min ⋅ Until %s", remaining_minutes,
                  base::UTF16ToUTF8(focus_mode_util::GetFormattedClockString(
                                        now + session_duration))
                      .c_str())),
              GetToggleRowSubLabel()->GetText());
    EXPECT_EQ(active ? u"End" : u"Start", GetToggleRowButton()->GetText());
  };

  validate_labels(/*active=*/false);

  // Starting the focus session closes the bubble, so we need to recreate the
  // detailed view.
  LeftClickOn(GetToggleRowButton());
  CreateFakeFocusModeDetailedView();

  // Wait a second to avoid the time remaining being either 1500 seconds or
  // 1499.99 seconds.
  task_environment()->FastForwardBy(base::Seconds(1));
  validate_labels(/*active=*/true);

  LeftClickOn(GetToggleRowButton());
  validate_labels(/*active=*/false);

  // Verify that the time displays correctly in the 24-hour clock format.
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  // Starting the focus session closes the bubble, so we need to recreate the
  // detailed view.
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
  IconButton* increment_button = GetTimerSettingIncrementButton();

  // Check incrementing 1 through 5.
  timer_textfield->SetText(u"1");
  LeftClickOn(increment_button);
  int expected_next_value = 2;
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(
        expected_next_value,
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield));
    expected_next_value += 1;
    LeftClickOn(increment_button);
  }

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

  // Decrement 300 to 285.
  timer_textfield->SetText(u"300");
  LeftClickOn(decrement_button);
  EXPECT_EQ(u"285", timer_textfield->GetText());

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

}  // namespace ash
