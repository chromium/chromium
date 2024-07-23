// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

SelectToSpeakTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->select_to_speak_tray();
}

}  // namespace

class SelectToSpeakTrayTest : public AshTestBase {
 public:
  SelectToSpeakTrayTest() = default;

  SelectToSpeakTrayTest(const SelectToSpeakTrayTest&) = delete;
  SelectToSpeakTrayTest& operator=(const SelectToSpeakTrayTest&) = delete;

  ~SelectToSpeakTrayTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(
        true);
  }

 protected:
  // Returns true if the Select to Speak tray is visible.
  bool IsVisible() { return GetTray()->GetVisible(); }

  // Returns true if the background color of the tray is active.
  bool IsTrayBackgroundActive() { return GetTray()->is_active(); }

  // Gets the current tray image view.
  views::ImageView* GetImageView() { return GetTray()->icon_; }

  // Gets the corresponding image given the |select_to_speak_state|.
  gfx::ImageSkia GetIconImage(SelectToSpeakState select_to_speak_state) {
    const auto color_id =
        chromeos::features::IsJellyEnabled()
            ? (select_to_speak_state ==
                       SelectToSpeakState::kSelectToSpeakStateInactive
                   ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
                   : static_cast<ui::ColorId>(
                         cros_tokens::kCrosSysSystemOnPrimaryContainer))
            : kColorAshIconColorPrimary;
    const auto icon_color = GetTray()->GetColorProvider()->GetColor(color_id);
    switch (select_to_speak_state) {
      case SelectToSpeakState::kSelectToSpeakStateInactive:
        return gfx::CreateVectorIcon(kSystemTraySelectToSpeakNewuiIcon,
                                     icon_color);
      case SelectToSpeakState::kSelectToSpeakStateSelecting:
        return gfx::CreateVectorIcon(kSystemTraySelectToSpeakActiveNewuiIcon,
                                     icon_color);
      case SelectToSpeakState::kSelectToSpeakStateSpeaking:
        return gfx::CreateVectorIcon(kSystemTrayStopNewuiIcon, icon_color);
    }
  }
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
  Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(false);
  EXPECT_FALSE(IsVisible());
  Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(true);
  EXPECT_TRUE(IsVisible());
}

// Test that clicking the button sends a Select to Speak state change request.
TEST_F(SelectToSpeakTrayTest, ButtonRequestsSelectToSpeakStateChange) {
  TestAccessibilityControllerClient client;
  EXPECT_EQ(0, client.select_to_speak_change_change_requests());

  GestureTapOn(GetTray());
  EXPECT_EQ(1, client.select_to_speak_change_change_requests());

  GestureTapOn(GetTray());
  EXPECT_EQ(2, client.select_to_speak_change_change_requests());
}

// Test that changing the SelectToSpeakState in the AccessibilityController
// results in a change of icon and activation in the tray.
TEST_F(SelectToSpeakTrayTest, SelectToSpeakStateImpactsImageAndActivation) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);
  EXPECT_TRUE(IsTrayBackgroundActive());

  gfx::ImageSkia expected_icon_image =
      GetIconImage(SelectToSpeakState::kSelectToSpeakStateSelecting);
  gfx::ImageSkia actual_icon_image = GetImageView()->GetImage();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon_image.bitmap(),
                                         *actual_icon_image.bitmap()));
  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSpeaking);
  EXPECT_TRUE(IsTrayBackgroundActive());

  expected_icon_image =
      GetIconImage(SelectToSpeakState::kSelectToSpeakStateSpeaking);
  actual_icon_image = GetImageView()->GetImage();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon_image.bitmap(),
                                         *actual_icon_image.bitmap()));

  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  EXPECT_FALSE(IsTrayBackgroundActive());
  expected_icon_image =
      GetIconImage(SelectToSpeakState::kSelectToSpeakStateInactive);
  actual_icon_image = GetImageView()->GetImage();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon_image.bitmap(),
                                         *actual_icon_image.bitmap()));
}

// Test that changing the SelectToSpeakState in the AccessibilityController
// results in a change of tooltip text in the tray.
TEST_F(SelectToSpeakTrayTest, SelectToSpeakStateImpactsTooltipText) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);
  std::u16string expected_tooltip_text = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK_INSTRUCTIONS);
  std::u16string actual_tooltip_text = GetImageView()->GetTooltipText();
  EXPECT_TRUE(expected_tooltip_text == actual_tooltip_text);

  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSpeaking);
  expected_tooltip_text = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK_STOP_INSTRUCTIONS);
  actual_tooltip_text = GetImageView()->GetTooltipText();
  EXPECT_TRUE(expected_tooltip_text == actual_tooltip_text);

  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  expected_tooltip_text = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK);
  actual_tooltip_text = GetImageView()->GetTooltipText();
  EXPECT_TRUE(expected_tooltip_text == actual_tooltip_text);
}

// Trivial test to increase coverage of select_to_speak_tray.h. The
// SelectToSpeakTray does not have a bubble, so these are empty functions.
// Without this test, coverage of select_to_speak_tray.h is 0%.
TEST_F(SelectToSpeakTrayTest, OverriddenFunctionsDoNothing) {
  GetTray()->HideBubbleWithView(nullptr);

  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  GetTray()->ClickedOutsideBubble(event);
}

}  // namespace ash
