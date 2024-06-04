// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/slider.h"

namespace ash {

// The step change of volume level if the volume up/down key is pressed.
constexpr float kVolumeStepChange = 0.04;

class UnifiedVolumeViewTest : public AshTestBase {
 public:
  UnifiedVolumeViewTest() = default;
  UnifiedVolumeViewTest(const UnifiedVolumeViewTest&) = delete;
  UnifiedVolumeViewTest& operator=(const UnifiedVolumeViewTest&) = delete;
  ~UnifiedVolumeViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  // Checks `level` corresponds to the expected icon.
  void CheckSliderIcon(float level) {
    const gfx::VectorIcon& icon = GetIcon(level);

    if (level <= 0.0) {
      EXPECT_STREQ(icon.name, UnifiedVolumeView::kQsVolumeLevelIcons[0]->name);
    } else if (level <= 0.5) {
      EXPECT_STREQ(icon.name, UnifiedVolumeView::kQsVolumeLevelIcons[1]->name);
    } else {
      EXPECT_STREQ(icon.name, UnifiedVolumeView::kQsVolumeLevelIcons[2]->name);
    }
  }

  UnifiedVolumeSliderController* volume_slider_controller() {
    return controller()->volume_slider_controller_.get();
  }

  UnifiedVolumeView* unified_volume_view() {
    return static_cast<UnifiedVolumeView*>(controller()->unified_volume_view_);
  }

  views::Slider* slider() { return unified_volume_view()->slider(); }

  IconButton* slider_button() { return unified_volume_view()->slider_button(); }

  const gfx::VectorIcon& GetIcon(float level) {
    return unified_volume_view()->GetVolumeIconForLevel(level);
  }

  UnifiedSystemTrayController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  views::Button* more_button() { return unified_volume_view()->more_button(); }
};

// Tests that `UnifiedVolumeView` is made up of a `QuickSettingsSlider`, a
// `LiveCaption` button, and a drill-in button that leads to
// `AudioDetailedView`.
TEST_F(UnifiedVolumeViewTest, SliderButtonComponents) {
  EXPECT_STREQ(unified_volume_view()->children()[0]->GetClassName(),
               "QuickSettingsSlider");

  // TODO(b/257151067): Updates the a11y name id and tooltip text.
  auto* live_caption_button =
      static_cast<IconButton*>(unified_volume_view()->children()[1]);
  EXPECT_STREQ(live_caption_button->GetClassName(), "IconButton");
  EXPECT_EQ(live_caption_button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP)));
  EXPECT_EQ(live_caption_button->GetTooltipText(),
            u"Toggle Live Caption. Live Caption is off.");

  auto* audio_subpage_drill_in_button =
      static_cast<IconButton*>(unified_volume_view()->children()[2]);
  EXPECT_STREQ(audio_subpage_drill_in_button->GetClassName(), "IconButton");
  EXPECT_EQ(
      audio_subpage_drill_in_button->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO));
  EXPECT_EQ(audio_subpage_drill_in_button->GetTooltipText(), u"Audio settings");

  // Clicks on the drill-in button and checks `AudioDetailedView` is shown.
  EXPECT_FALSE(controller()->IsDetailedViewShown());
  LeftClickOn(unified_volume_view()->children()[2]);
  EXPECT_TRUE(controller()->showing_audio_detailed_view());
}

// Tests the slider icon matches the slider level.
TEST_F(UnifiedVolumeViewTest, SliderIcon) {
  const float levels[] = {0.0, 0.2, 0.25, 0.49, 0.5, 0.7, 0.75, 0.9, 1};

  for (auto level : levels) {
    // Should mock that the user changes the slider value.
    volume_slider_controller()->SliderValueChanged(
        slider(), level, slider()->GetValue(),
        views::SliderChangeReason::kByUser);

    CheckSliderIcon(level);
  }
}

// Tests that showing the `UnifiedVolumeView` more button is disabled if and
// only if there is a trusted pinned window.
TEST_F(UnifiedVolumeViewTest, MoreButton) {
  // At the start of the test, the system tray containing the volume view is
  // already shown. Since there is no pinned window, the `more_button_` should
  // not be disabled.
  EXPECT_TRUE(more_button()->GetEnabled());

  // Close the bubble so the volume view can be recreated.
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // Create and trusted pin a window.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  wm::ActivateWindow(window.get());
  window_util::PinWindow(window.get(), /*trusted=*/true);

  // Open the bubble and check that the new volume view more button is in the
  // correct state.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(more_button()->GetEnabled());

  // Close the bubble so the volume view can be recreated.
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // Unpin the window
  WindowState::Get(window.get())->Restore();

  // Make sure the more button is not disabled.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_TRUE(more_button()->GetEnabled());
}

// Tests that pressing the keyboard volume mute key will mute the slider, and
// pressing the volume up key will unmute and increase the volume level by one
// step.
TEST_F(UnifiedVolumeViewTest, VolumeMuteThenVolumeUp) {
  // Sets the volume level by user.
  const float level = 0.8;
  volume_slider_controller()->SliderValueChanged(
      slider(), level, slider()->GetValue(),
      views::SliderChangeReason::kByUser);

  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);
  const bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_MUTE);
  // The slider level should remain as `level` and the mute state toggles.
  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);
  EXPECT_EQ(CrasAudioHandler::Get()->IsOutputMuted(), !is_muted);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_UP);
  // The slider level should increase by `kVolumeStepChange` and the icon should
  // change accordingly. The mute state toggles back to the original state.
  const float new_level = level + kVolumeStepChange;
  EXPECT_FLOAT_EQ(slider()->GetValue(), new_level);
  CheckSliderIcon(new_level);
  EXPECT_EQ(CrasAudioHandler::Get()->IsOutputMuted(), is_muted);
}

// Tests that pressing the keyboard volume mute key will mute the slider, and
// pressing the volume down key will preserve the mute state and decrease the
// volume level by one step.
TEST_F(UnifiedVolumeViewTest, VolumeMuteThenVolumeDown) {
  // Sets the volume level by user.
  const float level = 0.8;
  volume_slider_controller()->SliderValueChanged(
      slider(), level, slider()->GetValue(),
      views::SliderChangeReason::kByUser);

  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);
  const bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_MUTE);
  // The slider level should remain as `level` and the mute state toggles.
  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);
  EXPECT_EQ(CrasAudioHandler::Get()->IsOutputMuted(), !is_muted);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_DOWN);
  // The slider level should decrease by `kVolumeStepChange` and the icon should
  // change accordingly. The mute state remains the same as before pressing the
  // volume down.
  const float new_level = level - kVolumeStepChange;
  EXPECT_FLOAT_EQ(slider()->GetValue(), new_level);
  CheckSliderIcon(new_level);
  EXPECT_EQ(CrasAudioHandler::Get()->IsOutputMuted(), !is_muted);
}

// Tests when the slider is focused, press enter will toggle the mute state.
TEST_F(UnifiedVolumeViewTest, SliderFocusToggleMute) {
  // `slider()` is normally focusable, and `slider_button()` is accessibility
  // focusable.
  EXPECT_TRUE(slider()->IsFocusable());
  EXPECT_FALSE(slider_button()->IsFocusable());
  EXPECT_TRUE(
      slider_button()->GetViewAccessibility().IsAccessibilityFocusable());

  // Sets the level to make sure the slider's volume is not 0. Otherwise the
  // slider is still muted even if it's toggled on.
  const float level = 0.8;
  volume_slider_controller()->SliderValueChanged(
      slider(), level, slider()->GetValue(),
      views::SliderChangeReason::kByUser);

  auto* generator = GetEventGenerator();
  // Presses the tab key to activate the focus on the bubble.
  generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  slider()->RequestFocus();
  EXPECT_STREQ(unified_volume_view()
                   ->GetFocusManager()
                   ->GetFocusedView()
                   ->GetClassName(),
               "QuickSettingsSlider");

  const bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();
  // Presses the enter key when focused on the slider will toggle mute state.
  generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_EQ(CrasAudioHandler::Get()->IsOutputMuted(), !is_muted);
}

// Regression test for b/310804814.
TEST_F(UnifiedVolumeViewTest, MultipleDisplay) {
  // Add a secondary display to the left of the primary one.
  UpdateDisplay("1280x1024,1980x1080");

  // Sets the volume level by user.
  const float level = 0.5;
  volume_slider_controller()->SliderValueChanged(
      slider(), level, slider()->GetValue(),
      views::SliderChangeReason::kByUser);

  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_UP);
  // The slider level should increase by `kVolumeStepChange` and the icon should
  // change accordingly. The mute state toggles back to the original state.
  const float new_level = level + kVolumeStepChange;
  EXPECT_FLOAT_EQ(slider()->GetValue(), new_level);
  CheckSliderIcon(new_level);
}

}  // namespace ash
