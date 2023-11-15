// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/bounds_tracker/window_bounds_tracker.h"
#include "ash/wm/work_area_insets.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WindowBoundsTrackerTest : public AshTestBase {
 public:
  WindowBoundsTrackerTest()
      : scoped_feature_list_(features::kWindowBoundsTracker) {}
  WindowBoundsTrackerTest(const WindowBoundsTrackerTest&) = delete;
  WindowBoundsTrackerTest& operator=(const WindowBoundsTrackerTest&) = delete;
  ~WindowBoundsTrackerTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the window is fully visible after being moved to a new display.
TEST_F(WindowBoundsTrackerTest, DISABLED_OffscreenProtection) {
  UpdateDisplay("400x300,600x500");

  // Initially, the window is half-offscreen inside the second display.
  const gfx::Rect initial_bounds(900, 0, 200, 100);
  aura::Window* w = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(w);
  // Using the shortcut ALT+SEARCH+M to move the window to the 1st display, it
  // should be remapped to be fully visible inside the display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(gfx::Rect(200, 0, 200, 100), w->bounds());

  // Moving the window back to the 2nd display, it should be restored to its
  // previous bounds, even though it was offscreen.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_bounds, w->bounds());
}

// Tests that the window will stay in the same physical position after rotation.
TEST_F(WindowBoundsTrackerTest, DISABLED_ScreenRotation) {
  UpdateDisplay("400x300");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  const int window_initial_width = 200;
  const int window_initial_height = 100;
  const gfx::Rect initial_bounds(0, 0, window_initial_width,
                                 window_initial_height);
  aura::Window* w = CreateTestWindowInShellWithBounds(initial_bounds);
  ASSERT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  ASSERT_EQ(initial_bounds, w->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  // Rotating from `kLandscapePrimary` to `kPortraitPrimary`, the window should
  // stay in the same physical position. Window's width and height should be
  // swapped in this process.
  gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
  EXPECT_EQ(gfx::Rect(display_bounds.width() - window_initial_height, 0,
                      window_initial_height, window_initial_width),
            w->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  const int bottom_inset1 =
      WorkAreaInsets::ForWindow(w)->user_work_area_insets().bottom();
  // Rotating from `kPortraitPrimary` to `kLandscapeSecondary`, the window
  // should go back to its previous size, stay in the same physical position but
  // adjusted based on the updated work area to make sure it is fully visible.
  EXPECT_EQ(
      gfx::Rect(display_bounds.width() - window_initial_width,
                display_bounds.height() - window_initial_height - bottom_inset1,
                window_initial_width, window_initial_height),
      w->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  const int bottom_inset2 =
      WorkAreaInsets::ForWindow(w)->user_work_area_insets().bottom();
  display_bounds = GetPrimaryDisplay().bounds();
  // Rotating from `kLandscapeSecondary` to `kPortraitSecondary`, window's width
  // and height should be swapped, stay in the same physical position but
  // adjusted based on the updated work area to make sure it is fully visible.
  EXPECT_EQ(
      gfx::Rect(bottom_inset1,
                display_bounds.height() - window_initial_width - bottom_inset2,
                window_initial_height, window_initial_width),
      w->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  // Rotating back to `kLandscapePrimary`, the window should be restored to its
  // initial bounds.
  EXPECT_EQ(initial_bounds, w->bounds());
}

// Tests that the window's relative position to the center point of the work
// area is the same after work area size changes.
TEST_F(WindowBoundsTrackerTest, DISABLED_WorkAreaSizeChanges) {
  UpdateDisplay("400x300,600x500");

  const gfx::Rect first_display_work_area = GetPrimaryDisplay().work_area();
  const gfx::Rect second_display_work_area = GetSecondaryDisplay().work_area();

  // Creates a window at the center of the work area.
  const gfx::Point first_center_point = first_display_work_area.CenterPoint();
  const gfx::Rect initial_bounds(first_center_point.x() - 100,
                                 first_center_point.y() - 50, 200, 100);
  aura::Window* w = CreateTestWindowInShellWithBounds(initial_bounds);
  EXPECT_TRUE(first_display_work_area.Contains(w->GetBoundsInScreen()));
  EXPECT_EQ(first_center_point, w->GetBoundsInScreen().CenterPoint());

  // After moving the window from primary display to the secondary display, it
  // should still stay at the center of the current work area.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  const gfx::Point second_center_point = second_display_work_area.CenterPoint();
  EXPECT_TRUE(second_display_work_area.Contains(w->GetBoundsInScreen()));
  EXPECT_EQ(second_center_point, w->GetBoundsInScreen().CenterPoint());
}

}  // namespace ash
