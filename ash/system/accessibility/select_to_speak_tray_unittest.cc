// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_tray.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

SelectToSpeakTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->select_to_speak_tray();
}

ui::GestureEvent CreateTapEvent() {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace

class SelectToSpeakTrayTest : public AshTestBase {
 public:
  SelectToSpeakTrayTest() = default;
  ~SelectToSpeakTrayTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetSelectToSpeakEnabled(true);
  }

 protected:
  // Returns true if the Select to Speak tray is visible.
  bool IsVisible() { return GetTray()->GetVisible(); }

  // Returns true if the background color of the tray is active.
  bool IsTrayBackgroundActive() { return GetTray()->is_active(); }

  // Gets the current tray image view.
  views::ImageView* GetImageView() { return GetTray()->icon_; }

  gfx::ImageSkia GetInactiveImage() { return GetTray()->inactive_image_; }

  gfx::ImageSkia GetSelectingImage() { return GetTray()->selecting_image_; }

  gfx::ImageSkia GetSpeakingImage() { return GetTray()->speaking_image_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakTrayTest);
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
// Also checks that the tray is visible.
TEST_F(SelectToSpeakTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageView());
  EXPECT_TRUE(IsVisible());
}

// Tests the icon disapears when select-to-speak is disabled and re-appears
// when it is enabled.
TEST_F(SelectToSpeakTrayTest, ShowsAndHidesWithSelectToSpeakEnabled) {
  Shell::Get()->accessibility_controller()->SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(IsVisible());
  Shell::Get()->accessibility_controller()->SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(IsVisible());
}

// Test that clicking the button sends a Select to Speak state change request.
TEST_F(SelectToSpeakTrayTest, ButtonRequestsSelectToSpeakStateChange) {
  TestAccessibilityControllerClient client;
  EXPECT_EQ(0, client.select_to_speak_change_change_requests());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_EQ(1, client.select_to_speak_change_change_requests());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_EQ(2, client.select_to_speak_change_change_requests());
}

// Test that changing the SelectToSpeakState in the AccessibilityController
// results in a change of icon and activation in the tray.
TEST_F(SelectToSpeakTrayTest, SelectToSpeakStateImpactsImageAndActivation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(
      GetSelectingImage().BackedBySameObjectAs(GetImageView()->GetImage()));

  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSpeaking);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(
      GetSpeakingImage().BackedBySameObjectAs(GetImageView()->GetImage()));

  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_TRUE(
      GetInactiveImage().BackedBySameObjectAs(GetImageView()->GetImage()));
}

}  // namespace ash
