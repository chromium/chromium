// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_view.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
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

class UnifiedBrightnessViewTest : public AshTestBase {
 public:
  UnifiedBrightnessViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  UnifiedBrightnessViewTest(const UnifiedBrightnessViewTest&) = delete;
  UnifiedBrightnessViewTest& operator=(const UnifiedBrightnessViewTest&) =
      delete;
  ~UnifiedBrightnessViewTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    brightness_slider_controller_ =
        controller()->brightness_slider_controller_.get();
    unified_brightness_view_ = static_cast<UnifiedBrightnessView*>(
        controller()->unified_brightness_view_);
  }

  void TearDown() override {
    // Tests the environment tears down with the bubble closed.
    // In `UnifiedVolumeViewTest`, the environment is torn down with the bubble
    // open, so we can test both cases.
    GetPrimaryUnifiedSystemTray()->CloseBubble();
    AshTestBase::TearDown();
  }

  void WaitUntilUpdated() {
    task_environment()->FastForwardBy(base::Milliseconds(100));
    base::RunLoop().RunUntilIdle();
  }

  UnifiedBrightnessSliderController* brightness_slider_controller() {
    return brightness_slider_controller_;
  }

  UnifiedBrightnessView* unified_brightness_view() {
    return unified_brightness_view_;
  }

  views::Slider* slider() { return unified_brightness_view_->slider(); }

  views::ImageView* slider_icon() {
    return unified_brightness_view_->slider_icon();
  }

  UnifiedSystemTrayController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

 private:
  UnifiedBrightnessView* unified_brightness_view_ = nullptr;
  UnifiedBrightnessSliderController* brightness_slider_controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that `UnifiedBrightnessView` is made up of a `QuickSettingsSlider`, a
// `NightLight` button, and a drill-in button that leads to the display subpage.
TEST_F(UnifiedBrightnessViewTest, SliderButtonComponents) {
  EXPECT_STREQ(
      unified_brightness_view()->children()[0]->children()[0]->GetClassName(),
      "QuickSettingsSlider");

  // TODO(b/257151067): Updates the a11y name id and tooltip text.
  auto* night_light_button =
      static_cast<IconButton*>(unified_brightness_view()->children()[1]);
  EXPECT_STREQ(night_light_button->GetClassName(), "IconButton");
  EXPECT_EQ(
      night_light_button->GetAccessibleName(),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_BUTTON_LABEL));
  EXPECT_EQ(night_light_button->GetTooltipText(), u"Night Light");

  auto* display_subpage_drill_in_button =
      static_cast<IconButton*>(unified_brightness_view()->children()[2]);
  EXPECT_STREQ(display_subpage_drill_in_button->GetClassName(), "IconButton");
  EXPECT_EQ(display_subpage_drill_in_button->GetAccessibleName(),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP));
  EXPECT_EQ(display_subpage_drill_in_button->GetTooltipText(),
            u"Show display settings");

  // TODO(b/259989534): Add a test after adding the display subpage to test the
  // drill-in button.
}

// Tests the slider icon matches the slider level.
TEST_F(UnifiedBrightnessViewTest, SliderIcon) {
  const float levels[] = {0.0, 0.04, 0.2, 0.25, 0.49, 0.5, 0.7, 0.75, 0.9, 1};

  for (auto level : levels) {
    // Sets the slider value. `SetValue()` will pass in
    // `SliderChangeReason::kByApi` as the slider change reason and will not
    // trigger UI updates, so we should call `SliderValueChanged()` afterwards
    // to update the icon.
    slider()->SetValue(level);
    brightness_slider_controller()->SliderValueChanged(
        slider(), level, slider()->GetValue(),
        views::SliderChangeReason::kByUser);

    WaitUntilUpdated();

    const gfx::VectorIcon* icon =
        slider_icon()->GetImageModel().GetVectorIcon().vector_icon();

    if (level <= 0.0) {
      EXPECT_STREQ(icon->name,
                   UnifiedBrightnessView::kBrightnessLevelIcons[0]->name);
    } else if (level <= 0.5) {
      EXPECT_STREQ(icon->name,
                   UnifiedBrightnessView::kBrightnessLevelIcons[1]->name);
    } else {
      EXPECT_STREQ(icon->name,
                   UnifiedBrightnessView::kBrightnessLevelIcons[2]->name);
    }
  }
}

}  // namespace ash
