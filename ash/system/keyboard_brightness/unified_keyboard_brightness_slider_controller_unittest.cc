// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/unified_keyboard_brightness_slider_controller.h"

#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/slider.h"

namespace ash {

class UnifiedKeyboardBrightnessSliderControllerTest : public AshTestBase {
 public:
  UnifiedKeyboardBrightnessSliderControllerTest() = default;
  UnifiedKeyboardBrightnessSliderControllerTest(
      const UnifiedKeyboardBrightnessSliderControllerTest&) = delete;
  UnifiedKeyboardBrightnessSliderControllerTest& operator=(
      const UnifiedKeyboardBrightnessSliderControllerTest&) = delete;
  ~UnifiedKeyboardBrightnessSliderControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<UnifiedKeyboardBrightnessSliderController>(
        GetPrimaryUnifiedSystemTray()->model().get());
    keyboard_brightness_slider_ = controller_->CreateView();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(keyboard_brightness_slider_.get());
  }

  void TearDown() override {
    keyboard_brightness_slider_.reset();
    controller_.reset();
    widget_.reset();

    AshTestBase::TearDown();
  }

  UnifiedSliderView* unified_keyboard_brightness_slider() {
    return keyboard_brightness_slider_.get();
  }

  views::Slider* slider() { return keyboard_brightness_slider_->slider(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<UnifiedKeyboardBrightnessSliderController> controller_ =
      nullptr;
  std::unique_ptr<UnifiedSliderView> keyboard_brightness_slider_ = nullptr;
};

// Tests to ensure that the `slider_button` does not handle any events,
// letting them get through to the slider. Effectively the `slider_button` is
// part of the slider in the keyboard brightness view.
TEST_F(UnifiedKeyboardBrightnessSliderControllerTest,
       SliderButtonClickThrough) {
  slider()->SetValue(1.0);
  EXPECT_FLOAT_EQ(slider()->GetValue(), 1.0);

  // A click on the `slider_button` for `unified_keyboard_brightness_slider()`
  // should go through to the slider and change the value to the minimum.
  LeftClickOn(unified_keyboard_brightness_slider()->slider_button());
  EXPECT_FLOAT_EQ(slider()->GetValue(), 0.0);
}

}  // namespace ash
