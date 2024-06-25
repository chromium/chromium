// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_bubble_controller.h"

#include <vector>

#include "ash/shelf/shelf.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using SliderType = UnifiedSliderBubbleController::SliderType;
using UnifiedSliderBubbleControllerTest = AshTestBase;

// Tests the bubble and slider view lifetime when opening and closing each type
// of bubble.
TEST_F(UnifiedSliderBubbleControllerTest, ShowAndHideBubble) {
  std::vector<SliderType> types = {
      SliderType::SLIDER_TYPE_VOLUME,
      SliderType::SLIDER_TYPE_DISPLAY_BRIGHTNESS,
      SliderType::SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_OFF,
      SliderType::SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_ON,
      SliderType::SLIDER_TYPE_KEYBOARD_BRIGHTNESS,
      SliderType::SLIDER_TYPE_MIC};

  UnifiedSliderBubbleController controller(
      GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray());

  for (auto type : types) {
    controller.ShowBubble(type);

    EXPECT_TRUE(controller.IsBubbleShown());
    EXPECT_TRUE(controller.slider_view());

    controller.CloseBubble();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(controller.IsBubbleShown());
    EXPECT_FALSE(controller.slider_view());
  }
}

// Regression test for b/342513656.
TEST_F(UnifiedSliderBubbleControllerTest, UpdateBubble) {
  std::vector<SliderType> types = {
      SliderType::SLIDER_TYPE_VOLUME,
      SliderType::SLIDER_TYPE_DISPLAY_BRIGHTNESS,
      SliderType::SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_OFF,
      SliderType::SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_ON,
      SliderType::SLIDER_TYPE_KEYBOARD_BRIGHTNESS,
      SliderType::SLIDER_TYPE_MIC};

  UnifiedSliderBubbleController controller(
      GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray());

  for (auto type : types) {
    controller.ShowBubble(type);

    EXPECT_TRUE(controller.IsBubbleShown());
    EXPECT_TRUE(controller.slider_view());
  }
}

}  // namespace
}  // namespace ash
