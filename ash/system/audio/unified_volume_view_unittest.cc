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
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
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
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    volume_slider_controller_ = controller()->volume_slider_controller_.get();
    unified_volume_view_ =
        static_cast<UnifiedVolumeView*>(controller()->unified_volume_view_);
  }

  UnifiedVolumeSliderController* volume_slider_controller() {
    return volume_slider_controller_;
  }

  UnifiedVolumeView* unified_volume_view() { return unified_volume_view_; }

  views::Slider* slider() { return unified_volume_view_->slider(); }

  views::ImageView* slider_icon() {
    return unified_volume_view_->slider_icon();
  }

  UnifiedSystemTrayController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

 private:
  UnifiedVolumeView* unified_volume_view_ = nullptr;
  UnifiedVolumeSliderController* volume_slider_controller_ = nullptr;
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
            l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION));
  EXPECT_EQ(live_caption_button->GetTooltipText(), u"Live Caption");

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
}

}  // namespace ash
