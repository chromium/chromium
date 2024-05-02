// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr int kFakeDeviceId = 0;

class FakeDelegate : public UnifiedVolumeSliderController::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  void OnAudioSettingsButtonClicked() override {}
};

// Pixel tests for the quick settings `UnifiedSliderView`.
class UnifiedSliderViewPixelTest : public AshTestBase {
 public:
  UnifiedSliderViewPixelTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    delegate_ = std::make_unique<FakeDelegate>();
    unified_volume_slider_controller_ =
        std::make_unique<UnifiedVolumeSliderController>(delegate_.get());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    unified_volume_slider_controller_.reset();
    delegate_.reset();

    AshTestBase::TearDown();
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  std::unique_ptr<FakeDelegate> delegate_;
  std::unique_ptr<UnifiedVolumeSliderController>
      unified_volume_slider_controller_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(UnifiedSliderViewPixelTest, DefaultSlider) {
  // Creates a `UnifiedVolumeView` that's on the main page. This slider is the
  // representative of the style `QuickSettingsSlider::Style::kDefault`.
  auto default_slider = std::make_unique<UnifiedVolumeView>(
      unified_volume_slider_controller_.get(), delegate_.get(),
      /*is_active_output_node=*/true);
  widget_->SetContentsView(default_slider.get());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "default_slider",
      /*revision_number=*/0, widget_.get()));

  default_slider->RequestFocus();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused_default_slider",
      /*revision_number=*/0, widget_.get()));
}

// TODO(crbug.com/40283113): Flaky.
TEST_F(UnifiedSliderViewPixelTest, DISABLED_DefaultSliderMuted) {
  // Creates a `UnifiedVolumeView` that's on the main page. This slider is in
  // `QuickSettingsSlider::Style::kDefault` style.
  auto default_slider = std::make_unique<UnifiedVolumeView>(
      unified_volume_slider_controller_.get(), delegate_.get(),
      /*is_active_output_node=*/true);
  widget_->SetContentsView(default_slider.get());
  // Presses the slider button to mute the slider, then it becomes
  // `QuickSettingsSlider::Style::kDefaultMuted`.
  unified_volume_slider_controller_->SliderButtonPressed();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "default_slider_muted",
      /*revision_number=*/4, widget_.get()));

  default_slider->RequestFocus();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused_default_slider_muted",
      /*revision_number=*/1, widget_.get()));
}

TEST_F(UnifiedSliderViewPixelTest, RadioActiveSlider) {
  // Creates a `UnifiedVolumeView` that's on the audio subpage. This slider is
  // the representative of the style `QuickSettingsSlider::Style::kRadioActive`.
  auto radio_active_slider = std::make_unique<UnifiedVolumeView>(
      unified_volume_slider_controller_.get(), kFakeDeviceId,
      /*is_active_output_node=*/true,
      /*inside_padding=*/kRadioSliderViewPadding);
  widget_->SetContentsView(radio_active_slider.get());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "radio_active_slider",
      /*revision_number=*/0, widget_.get()));

  radio_active_slider->RequestFocus();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused_radio_active_slider",
      /*revision_number=*/0, widget_.get()));
}

// Flaky. See https://crbug.com/1484867
TEST_F(UnifiedSliderViewPixelTest, DISABLED_RadioActiveSliderMuted) {
  // Creates a `UnifiedVolumeView` that's on the audio subpage. This slider is
  // in the `QuickSettingsSlider::Style::kRadioActive` style.
  auto radio_active_slider = std::make_unique<UnifiedVolumeView>(
      unified_volume_slider_controller_.get(), kFakeDeviceId,
      /*is_active_output_node=*/true,
      /*inside_padding=*/kRadioSliderViewPadding);
  widget_->SetContentsView(radio_active_slider.get());
  unified_volume_slider_controller_->SliderButtonPressed();

  // Presses the slider button to mute the slider, then it becomes
  // `QuickSettingsSlider::Style::kRadioActiveMuted`.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "radio_active_slider_muted",
      /*revision_number=*/4, widget_.get()));

  radio_active_slider->RequestFocus();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused_radio_active_slider_muted",
      /*revision_number=*/1, widget_.get()));
}

TEST_F(UnifiedSliderViewPixelTest, RadioInactiveSlider) {
  // Creates a `UnifiedVolumeView` that's on the audio subpage. This slider is
  // the representative of the style
  // `QuickSettingsSlider::Style::kRadioInactive`.
  auto radio_inactive_slider = std::make_unique<UnifiedVolumeView>(
      unified_volume_slider_controller_.get(), kFakeDeviceId,
      /*is_active_output_node=*/false,
      /*inside_padding=*/kRadioSliderViewPadding);
  widget_->SetContentsView(radio_inactive_slider.get());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "radio_inactive_slider",
      /*revision_number=*/0, widget_.get()));

  radio_inactive_slider->RequestFocus();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused_radio_inactive_slider",
      /*revision_number=*/0, widget_.get()));
}

}  // namespace ash
