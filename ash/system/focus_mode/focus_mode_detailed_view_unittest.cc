// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/style/pill_button.h"
#include "ash/style/switch.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class FocusModeDetailedViewTest : public AshTestBase {
 public:
  FocusModeDetailedViewTest() : scoped_feature_(features::kFocusMode) {}
  ~FocusModeDetailedViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto focus_mode_detailed_view =
        std::make_unique<FocusModeDetailedView>(&detailed_view_delegate_);
    focus_mode_detailed_view_ = focus_mode_detailed_view.get();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(std::move(focus_mode_detailed_view));
  }

  void TearDown() override {
    focus_mode_detailed_view_ = nullptr;
    widget_.reset();

    AshTestBase::TearDown();
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
  views::Label* label = GetToggleRowLabel();
  views::Label* sub_label = GetToggleRowSubLabel();
  PillButton* toggle_button = GetToggleRowButton();

  auto validate_labels = [&](bool active) {
    EXPECT_EQ(active, focus_mode_controller->in_focus_session());
    EXPECT_EQ(active ? u"Focusing" : u"Focus", label->GetText());

    const base::TimeDelta session_duration =
        focus_mode_controller->session_duration();
    const int remaining_minutes =
        active ? (focus_mode_controller->end_time() - base::Time::Now())
                     .InMinutes()
               : session_duration.InMinutes();

    EXPECT_EQ(base::UTF8ToUTF16(base::StringPrintf(
                  "%d min ⋅ Until %s", remaining_minutes,
                  base::UTF16ToUTF8(base::TimeFormatTimeOfDay(
                                        base::Time::Now() + session_duration))
                      .c_str())),
              sub_label->GetText());
    EXPECT_EQ(active ? u"End" : u"Start", toggle_button->GetText());
  };

  validate_labels(/*active=*/false);

  LeftClickOn(toggle_button);
  validate_labels(/*active=*/true);

  LeftClickOn(toggle_button);
  validate_labels(/*active=*/false);
}

}  // namespace ash
