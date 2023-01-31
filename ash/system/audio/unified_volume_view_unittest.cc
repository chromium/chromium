// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_view.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/slider.h"

namespace ash {

class UnifiedVolumeViewTest : public AshTestBase {
 public:
  UnifiedVolumeViewTest() = default;
  UnifiedVolumeViewTest(const UnifiedVolumeViewTest&) = delete;
  UnifiedVolumeViewTest& operator=(const UnifiedVolumeViewTest&) = delete;
  ~UnifiedVolumeViewTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  // Checks `level` corresponds to the expected icon.
  void CheckSliderIcon(float level) {
    const gfx::VectorIcon* icon =
        slider_icon()->GetImageModel().GetVectorIcon().vector_icon();

    if (level <= 0.0) {
      EXPECT_STREQ(icon->name, UnifiedVolumeView::kQsVolumeLevelIcons[0]->name);
    } else if (level <= 0.5) {
      EXPECT_STREQ(icon->name, UnifiedVolumeView::kQsVolumeLevelIcons[1]->name);
    } else {
      EXPECT_STREQ(icon->name, UnifiedVolumeView::kQsVolumeLevelIcons[2]->name);
    }
  }

  UnifiedVolumeSliderController* volume_slider_controller() {
    return controller()->volume_slider_controller_.get();
  }

  UnifiedVolumeView* unified_volume_view() {
    return static_cast<UnifiedVolumeView*>(controller()->unified_volume_view_);
  }

  views::Slider* slider() { return unified_volume_view()->slider(); }

  views::ImageView* slider_icon() {
    return unified_volume_view()->slider_icon();
  }

  UnifiedSystemTrayController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  views::Button* more_button() { return unified_volume_view()->more_button(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that `UnifiedVolumeView` is made up of a `QuickSettingsSlider`, a
// `LiveCaption` button, and a drill-in button that leads to
// `AudioDetailedView`.
TEST_F(UnifiedVolumeViewTest, SliderButtonComponents) {
  EXPECT_STREQ(
      unified_volume_view()->children()[0]->children()[0]->GetClassName(),
      "QuickSettingsSlider");

  // TODO(b/257151067): Updates the a11y name id and tooltip text.
  auto* live_caption_button =
      static_cast<IconButton*>(unified_volume_view()->children()[1]);
  EXPECT_STREQ(live_caption_button->GetClassName(), "IconButton");
  EXPECT_EQ(live_caption_button->GetAccessibleName(),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP)));
  EXPECT_EQ(live_caption_button->GetTooltipText(),
            u"Toggle Live Caption. Live Caption is off.");

  auto* audio_subpage_drill_in_button =
      static_cast<IconButton*>(unified_volume_view()->children()[2]);
  EXPECT_STREQ(audio_subpage_drill_in_button->GetClassName(), "IconButton");
  EXPECT_EQ(audio_subpage_drill_in_button->GetAccessibleName(),
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
// pressing the volume up key will restore the volume level.
TEST_F(UnifiedVolumeViewTest, VolumeMuteThenVolumeUp) {
  // Sets the volume level by user.
  const float level = 0.8;
  volume_slider_controller()->SliderValueChanged(
      slider(), level, slider()->GetValue(),
      views::SliderChangeReason::kByUser);

  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_MUTE);
  // The slider level should be 0 and icon appears as muted.
  EXPECT_EQ(slider()->GetValue(), 0);
  CheckSliderIcon(0);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_UP);
  // The slider level and icon should be restored.
  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);
}

// Tests that pressing the keyboard volume mute key will mute the slider, and
// pressing the volume down key will preserve the mute state.
TEST_F(UnifiedVolumeViewTest, VolumeMuteThenVolumeDown) {
  // Sets the volume level by user.
  const float level = 0.8;
  volume_slider_controller()->SliderValueChanged(
      slider(), level, slider()->GetValue(),
      views::SliderChangeReason::kByUser);

  EXPECT_EQ(slider()->GetValue(), level);
  CheckSliderIcon(level);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_MUTE);
  // The slider level should be 0 and icon appears as muted.
  EXPECT_EQ(slider()->GetValue(), 0);
  CheckSliderIcon(0);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_VOLUME_DOWN);
  // The slider level and icon should remain muted.
  EXPECT_EQ(slider()->GetValue(), 0);
  CheckSliderIcon(0);
}

}  // namespace ash
