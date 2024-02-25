// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_affordance.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

namespace {

constexpr int kDisplayWidth = 1200;
constexpr int kDisplayHeight = 800;

}  // namespace

using BackGestureAffordanceTest = AshTestBase;

// Tests that the affordance should never be shown outside of the display.
TEST_F(BackGestureAffordanceTest, AffordaceShouldNotOutsideDisplay) {
  TabletModeControllerTestApi().EnterTabletMode();
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
      std::make_unique<BackGestureAffordance>(start_point);
  affordance_bounds =
      back_gesture_affordance->affordance_widget_bounds_for_testing();
  EXPECT_LE(0, affordance_bounds.y());
  EXPECT_LE(start_point.y(), affordance_bounds.y());
}

// Tests the affordance layout while swiping from the bottom snapped window in
// portrait screen orientation.
TEST_F(BackGestureAffordanceTest,
       DoNotExceedSplitViewDividerInPortraitOrientation) {
  TabletModeControllerTestApi().EnterTabletMode();
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  std::unique_ptr<aura::Window> bottom_window = CreateTestWindow();
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(bottom_window.get(),
                                    SnapPosition::kSecondary);

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          false);

  // Affordance should not exceed or overlap with the splitview divider, put it
  // below the start point instead.
  gfx::Point start_point(0, divider_bounds.bottom() + 10);
  std::unique_ptr<BackGestureAffordance> back_gesture_affordance =
      std::make_unique<BackGestureAffordance>(start_point);
  gfx::Rect affordance_bounds(
      back_gesture_affordance->affordance_widget_bounds_for_testing());
  EXPECT_LT(divider_bounds.bottom(), affordance_bounds.y());
  EXPECT_LT(start_point.y(), affordance_bounds.y());

  // Affordance should still above the start point if it will not exceed or
  // overlap with the splitview divider.
  start_point.set_y(divider_bounds.bottom() + 150);
  back_gesture_affordance =
      std::make_unique<BackGestureAffordance>(start_point);
  affordance_bounds =
      back_gesture_affordance->affordance_widget_bounds_for_testing();
  EXPECT_LE(divider_bounds.bottom(), affordance_bounds.y());
  EXPECT_GE(start_point.y(), affordance_bounds.y());
}

}  // namespace ash
