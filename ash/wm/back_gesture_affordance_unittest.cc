// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/back_gesture_affordance.h"

#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"

namespace ash {

namespace {

constexpr int kDisplayWidth = 1200;
constexpr int kDisplayHeight = 800;

}  // namespace

using BackGestureAffordanceTest = AshTestBase;

// Tests that the affordance should never be shown outside of the display.
TEST_F(BackGestureAffordanceTest, AffordaceShouldNotOutsideDisplay) {
  const std::string display = base::NumberToString(kDisplayWidth) + "x" +
                              base::NumberToString(kDisplayHeight);
  UpdateDisplay(display);

  // Affordance is above the start point and inside display if doesn's start
  // from the top area of the display.
  gfx::Point start_point(0, kDisplayHeight / 2);
  std::unique_ptr<BackGestureAffordance> back_gesture_affordance =
      std::make_unique<BackGestureAffordance>(start_point);
  gfx::Rect affordance_bounds =
      back_gesture_affordance->affordance_widget_bounds_for_testing();
  EXPECT_LE(0, affordance_bounds.y());
  EXPECT_GE(start_point.y(), affordance_bounds.y());

  // Affordance should be put below the start point to keep it inside display if
  // starts from the top area of the display.
  start_point.set_y(10);
  back_gesture_affordance =
      std::make_unique<BackGestureAffordance>(gfx::Point(0, 10));
  affordance_bounds =
      back_gesture_affordance->affordance_widget_bounds_for_testing();
  EXPECT_LE(0, affordance_bounds.y());
  EXPECT_LE(start_point.y(), affordance_bounds.y());
}

}  // namespace ash
