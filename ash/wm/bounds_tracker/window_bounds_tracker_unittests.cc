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
TEST_F(WindowBoundsTrackerTest, OffscreenProtection) {
  UpdateDisplay("400x300,600x500");

  // Initially, the window is half-offscreen inside the second display.
  const gfx::Rect initial_bounds(900, 0, 200, 100);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);
  // Using the shortcut ALT+SEARCH+M to move the window to the 1st display, it
  // should be remapped to be fully visible inside the display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(gfx::Rect(200, 0, 200, 100), window->bounds());

  // Moving the window back to the 2nd display, it should be restored to its
  // previous bounds, even though it was offscreen.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());
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
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  ASSERT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  ASSERT_EQ(initial_bounds, window->bounds());

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
            window->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  const int bottom_inset1 =
      WorkAreaInsets::ForWindow(window)->user_work_area_insets().bottom();
  // Rotating from `kPortraitPrimary` to `kLandscapeSecondary`, the window
  // should go back to its previous size, stay in the same physical position but
  // adjusted based on the updated work area to make sure it is fully visible.
  EXPECT_EQ(
      gfx::Rect(display_bounds.width() - window_initial_width,
                display_bounds.height() - window_initial_height - bottom_inset1,
                window_initial_width, window_initial_height),
      window->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  const int bottom_inset2 =
      WorkAreaInsets::ForWindow(window)->user_work_area_insets().bottom();
  display_bounds = GetPrimaryDisplay().bounds();
  // Rotating from `kLandscapeSecondary` to `kPortraitSecondary`, window's width
  // and height should be swapped, stay in the same physical position but
  // adjusted based on the updated work area to make sure it is fully visible.
  EXPECT_EQ(
      gfx::Rect(bottom_inset1,
                display_bounds.height() - window_initial_width - bottom_inset2,
                window_initial_height, window_initial_width),
      window->bounds());

  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  // Rotating back to `kLandscapePrimary`, the window should be restored to its
  // initial bounds.
  EXPECT_EQ(initial_bounds, window->bounds());
}

// Tests that the window's relative position to the center point of the work
// area is the same after work area size changes.
TEST_F(WindowBoundsTrackerTest, WorkAreaSizeChanges) {
  UpdateDisplay("400x300,800x600");

  display::Display first_display = GetPrimaryDisplay();
  display::Display secondary_display = GetSecondaryDisplay();
  const gfx::Rect first_display_work_area = first_display.work_area();
  const gfx::Rect second_display_work_area = secondary_display.work_area();

  // Creates a window at the center of the work area.
  const gfx::Point first_center_point = first_display_work_area.CenterPoint();
  const gfx::Size window_size(200, 100);
  const gfx::Rect initial_bounds(
      gfx::Point(first_center_point.x() - window_size.width() / 2,
                 first_center_point.y() - window_size.height() / 2),
      window_size);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  EXPECT_TRUE(first_display_work_area.Contains(window->GetBoundsInScreen()));
  EXPECT_EQ(first_center_point, window->GetBoundsInScreen().CenterPoint());

  // After moving the window from primary display to the secondary display, it
  // should still stay at the center of the current work area.
  wm::ActivateWindow(window);
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  const gfx::Point second_center_point = second_display_work_area.CenterPoint();
  EXPECT_TRUE(second_display_work_area.Contains(window->GetBoundsInScreen()));
  EXPECT_EQ(second_center_point, window->GetBoundsInScreen().CenterPoint());

  // Creates another window at the top left center of the primary display.
  const gfx::Point top_left_center(first_center_point.x() / 2,
                                   first_center_point.y() / 2);
  const gfx::Point origin(
      top_left_center -
      gfx::Vector2d(window_size.width() / 2, window_size.height() / 2));
  aura::Window* window2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(origin, window_size));
  wm::ActivateWindow(window2);

  // Using the shortcut to move `window2` to the secondary display, it should
  // stay at the top left center of the secondary display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_TRUE(second_display_work_area.Contains(window2->GetBoundsInScreen()));
  const gfx::Point second_local_work_area_center =
      secondary_display.GetLocalWorkArea().CenterPoint();
  const gfx::Point second_top_left_center(
      second_local_work_area_center.x() / 2,
      second_local_work_area_center.y() / 2);
  EXPECT_EQ(second_top_left_center, window2->bounds().CenterPoint());
}

// Tests that window's bounds stored in the same display configuration can be
// updated correctly. Window can be restored to the update bounds in the tracker
// correctly as well.
TEST_F(WindowBoundsTrackerTest, RestoreToUpdatedBounds) {
  UpdateDisplay("400x300,600x500");

  // Initially, the window is half-offscreen inside the 2nd display. Moving it
  // to the 1st display and back to the 2nd display to set up its bounds in the
  // bounds tracker inside these two displays.
  const gfx::Rect initial_bounds(900, 0, 200, 100);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  const gfx::Rect bounds_in_1st(200, 0, 200, 100);
  EXPECT_EQ(bounds_in_1st, window->bounds());
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());

  // Move the window back to the 1st display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(bounds_in_1st, window->bounds());
  // Update the window's position inside the 1st display, make it half-offscreen
  // as well.
  const gfx::Rect new_bounds_in_1st(300, 0, 200, 100);
  window->SetBounds(new_bounds_in_1st);
  // Move the window to 2nd display and then back to 1st display, it should
  // restore to its updated bounds stored in the tracker.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(new_bounds_in_1st, window->bounds());
}

// Tests of moving a window from a landscape orientation display to a portrait
// orientation display.
TEST_F(WindowBoundsTrackerTest, LandscapeToPortrait) {
  UpdateDisplay("400x300,300x400");

  ASSERT_TRUE(chromeos::IsLandscapeOrientation(
      chromeos::GetDisplayCurrentOrientation(GetPrimaryDisplay())));
  ASSERT_TRUE(chromeos::IsPortraitOrientation(
      chromeos::GetDisplayCurrentOrientation(GetSecondaryDisplay())));

  // Initially, the window is at the top right of the 1st display.
  const gfx::Rect initial_bounds(200, 0, 200, 100);
  const gfx::Rect rotated_90_bounds(0, 0, 100, 200);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);

  // Moving the window from a landscape to portrait display. The window is
  // supposed to be rotated 90 and then being mapped to the target portrait
  // display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(rotated_90_bounds, window->bounds());
  EXPECT_EQ(gfx::Rect(400, 0, 100, 200), window->GetBoundsInScreen());
}

TEST_F(WindowBoundsTrackerTest, PortraitToLandscape) {
  UpdateDisplay("300x400,400x300");

  display::Display primary_display = GetPrimaryDisplay();
  display::Display secondary_display = GetSecondaryDisplay();
  ASSERT_TRUE(chromeos::IsPortraitOrientation(
      chromeos::GetDisplayCurrentOrientation(primary_display)));
  ASSERT_TRUE(chromeos::IsLandscapeOrientation(
      chromeos::GetDisplayCurrentOrientation(secondary_display)));

  // Initially, the window is at the top right of the 1st display.
  const int x = 200;
  const gfx::Size window_size(100, 200);
  const gfx::Rect initial_bounds(gfx::Point(x, 0), window_size);
  const int bottom_inset_1st = primary_display.GetWorkAreaInsets().bottom();
  const int bottom_inset_2nd = secondary_display.GetWorkAreaInsets().bottom();
  EXPECT_EQ(bottom_inset_1st, bottom_inset_2nd);
  const int primary_display_width = primary_display.bounds().width();
  const int rotated_y =
      primary_display_width - bottom_inset_1st - window_size.width();
  gfx::Size transposed_window_size = window_size;
  transposed_window_size.Transpose();
  const gfx::Rect rotated_270_bounds(gfx::Point(x, rotated_y),
                                     transposed_window_size);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);

  // Moving the window from a portrait to landscape display. The window is
  // supposed to be rotated 270 and then being mapped to the target landscape
  // display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(rotated_270_bounds, window->bounds());
  EXPECT_EQ(gfx::Rect(gfx::Point(x + primary_display_width, rotated_y),
                      transposed_window_size),
            window->GetBoundsInScreen());
}

// Tests the window's remapping and restoration on removing and reconnecting a
// non-primary display.
TEST_F(WindowBoundsTrackerTest, RemoveNonPrimaryDisplay) {
  UpdateDisplay("400x300,600x500");

  // Initially, the window is half-offscreen inside the 2nd display.
  const gfx::Size window_size(200, 100);
  const gfx::Rect initial_bounds(gfx::Point(900, 0), window_size);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);

  const int64_t primary_id = GetPrimaryDisplay().id();
  const int64_t secondary_id = GetSecondaryDisplay().id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnect the 2nd display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // `window` is fully visible inside the primary display.
  const gfx::Rect remapping_bounds_in_1st(gfx::Point(200, 0), window_size);
  EXPECT_EQ(remapping_bounds_in_1st, window->GetBoundsInScreen());

  // Reconnect the 2nd display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // `window` should be moved back to the 2nd display at its previous bounds.
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());

  // Disconnect the 2nd display again. And change the window's bounds inside the
  // primary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(remapping_bounds_in_1st, window->GetBoundsInScreen());
  // Change the window's bounds inside the 1st display, make it half-offscreen
  // as well.
  const gfx::Rect updated_bounds_in_1st(gfx::Point(300, 0), window_size);
  window->SetBounds(updated_bounds_in_1st);

  // Reconnects the 2nd display. It should still restore back to its previous
  // bounds even though the user changed its bounds inside the 1st display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());

  // Disconnect the 2nd display, the window should restore to its updated bounds
  // in the 1st display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(updated_bounds_in_1st, window->GetBoundsInScreen());
}

TEST_F(WindowBoundsTrackerTest, RootWindowChanges) {
  UpdateDisplay("400x300,600x500");

  display::Display first_display = GetPrimaryDisplay();
  display::Display secondary_display = GetSecondaryDisplay();
  const gfx::Rect first_display_work_area = first_display.work_area();
  const gfx::Rect second_display_work_area = secondary_display.work_area();

  // Initially, the window is half-offscreen inside the 2nd display.
  const gfx::Size window_size(200, 100);
  const gfx::Rect initial_bounds(gfx::Point(900, 0), window_size);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);
  EXPECT_EQ(window->GetRootWindow(), Shell::GetAllRootWindows()[1]);

  // Drag it to the center of the 1st display.
  const gfx::Point first_center_point = first_display_work_area.CenterPoint();
  const gfx::Rect first_center_bounds(
      gfx::Point(first_center_point.x() - window_size.width() / 2,
                 first_center_point.y() - window_size.height() / 2),
      window_size);
  window->SetBoundsInScreen(first_center_bounds, first_display);
  EXPECT_EQ(window->GetRootWindow(), Shell::GetAllRootWindows()[0]);

  // Using the shortcut to move the window back to the 2nd display. It should be
  // remapped to the center of the display instead of initial half-offscreen
  // bounds. As it was dragged from the 2nd to 1st display, its previous
  // half-offscreen should not be stored in the bounds database.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_NE(initial_bounds, window->GetBoundsInScreen());
  EXPECT_EQ(second_display_work_area.CenterPoint(),
            window->GetBoundsInScreen().CenterPoint());

  // Using the shortcut to move the window to the 1st display. It should be
  // restored to the center of the 1st display. As it was moved from the 1st to
  // the 2nd through the shortcut before.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(first_center_bounds, window->GetBoundsInScreen());
}

}  // namespace ash
