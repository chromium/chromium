// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_button_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

DictationButtonTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->dictation_button_tray();
}

ui::GestureEvent CreateTapEvent(
    base::TimeDelta delta_from_start = base::TimeDelta()) {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks() + delta_from_start,
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace

class DictationButtonTrayTest : public AshTestBase {
 public:
  DictationButtonTrayTest() = default;
  ~DictationButtonTrayTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
  }

 protected:
  views::ImageView* GetImageView(DictationButtonTray* tray) {
    return tray->icon_;
  }
  void CheckDictationStatusAndUpdateIcon(DictationButtonTray* tray) {
    tray->CheckDictationStatusAndUpdateIcon();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DictationButtonTrayTest);
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
// Also checks that the tray is visible.
TEST_F(DictationButtonTrayTest, BasicConstruction) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetDictationAcceleratorDialogAccepted();
  controller->SetDictationEnabled(true);
  EXPECT_TRUE(GetImageView(GetTray()));
  EXPECT_TRUE(GetTray()->GetVisible());
}

// Test that clicking the button activates dictation.
TEST_F(DictationButtonTrayTest, ButtonActivatesDictation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->SetDictationAcceleratorDialogAccepted();
  controller->SetDictationEnabled(true);
  EXPECT_FALSE(controller->dictation_active());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_TRUE(controller->dictation_active());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_FALSE(controller->dictation_active());
}

// Test that activating dictation causes the button to activate.
TEST_F(DictationButtonTrayTest, ActivatingDictationActivatesButton) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetDictationAcceleratorDialogAccepted();
  controller->SetDictationEnabled(true);
  Shell::Get()->OnDictationStarted();
  EXPECT_TRUE(GetTray()->is_active());

  Shell::Get()->OnDictationEnded();
  EXPECT_FALSE(GetTray()->is_active());
}

// Tests that the tray only renders as active while dictation is listening. Any
// termination of dictation clears the active state.
TEST_F(DictationButtonTrayTest, ActiveStateOnlyDuringDictation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->SetDictationAcceleratorDialogAccepted();
  controller->SetDictationEnabled(true);

  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(GetTray()->is_active());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::TOGGLE_DICTATION, {});
  EXPECT_TRUE(controller->dictation_active());
  EXPECT_TRUE(GetTray()->is_active());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::TOGGLE_DICTATION, {});
  EXPECT_FALSE(controller->dictation_active());
  EXPECT_FALSE(GetTray()->is_active());
}

}  // namespace ash
