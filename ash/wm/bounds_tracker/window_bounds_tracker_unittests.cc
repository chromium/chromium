// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/bounds_tracker/window_bounds_tracker.h"
#include "ash/wm/window_state.h"
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

// Tests that window should be restored to the user assigned bounds correctly.
TEST_F(WindowBoundsTrackerTest, UserAssignedBounds) {
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
  WindowState::Get(window)->SetBoundsChangedByUser(true);
  // Move the window to 2nd display and then back to 1st display, it should
  // restore to its user-assigned bounds stored in the tracker.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(new_bounds_in_1st, window->bounds());
}

TEST_F(WindowBoundsTrackerTest, NonUserAssignedBounds) {
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
  // Moving the window to the 1st display and back to the 2nd display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  const gfx::Rect bounds_in_1st(200, 0, 200, 100);
  EXPECT_EQ(bounds_in_1st, window->bounds());
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_bounds, window->GetBoundsInScreen());

  // Moving the window inside the 2nd display from half-offscreen to the center
  // of the display.
  const gfx::Point second_center_point = second_display_work_area.CenterPoint();
  window->SetBoundsInScreen(
      gfx::Rect(gfx::Point(second_center_point.x() - window_size.width() / 2,
                           second_center_point.y() - window_size.height() / 2),
                window_size),
      secondary_display);
  // Moving the window to the 1st display, it should be remapped to the center
  // of the 1st display. As its previous non-user-assigned bounds should not be
  // stored in the bounds database.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(first_display_work_area.CenterPoint(),
            window->bounds().CenterPoint());
}

TEST_F(WindowBoundsTrackerTest,
       NonUserAssignedBoundsWithUpdatedDisplayWindowInfo) {
  UpdateDisplay("400x300,600x500");

  display::Display first_display = GetPrimaryDisplay();
  display::Display secondary_display = GetSecondaryDisplay();
  const gfx::Rect initial_first_display_work_area = first_display.work_area();
  const gfx::Rect second_display_work_area = secondary_display.work_area();

  // Initially, the window is half-offscreen inside the 2nd display.
  const gfx::Size window_size(200, 100);
  const gfx::Rect initial_bounds(gfx::Point(900, 0), window_size);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds);
  wm::ActivateWindow(window);
  // Moving the window to the 1st display, it will be remapped to be fully
  // visible.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  const gfx::Rect bounds_in_1st(200, 0, 200, 100);
  EXPECT_EQ(bounds_in_1st, window->bounds());

  // Change shelf alignment, which will change the work area. The window's
  // bounds should have no changes on this.
  Shelf* shelf =
      Shell::GetRootWindowControllerWithDisplayId(first_display.id())->shelf();
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());
  const gfx::Rect left_shelf_first_display_work_area =
      GetPrimaryDisplay().work_area();
  EXPECT_NE(initial_first_display_work_area,
            left_shelf_first_display_work_area);
  EXPECT_EQ(bounds_in_1st, window->bounds());

  // Move the window to the 2nd display when the shelf is still left aligned.
  // And then update its bounds inside the 2nd display from half-offscreen to
  // the center of the display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  const gfx::Point second_center_point = second_display_work_area.CenterPoint();
  window->SetBoundsInScreen(
      gfx::Rect(gfx::Point(second_center_point.x() - window_size.width() / 2,
                           second_center_point.y() - window_size.height() / 2),
                window_size),
      secondary_display);
  WindowState::Get(window)->SetBoundsChangedByUser(true);

  // Move the window from the 2nd to the 1st, it should be remapped to the
  // center of the work area when the shelf is left aligned.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(left_shelf_first_display_work_area.CenterPoint(),
            window->GetBoundsInScreen().CenterPoint());

  // Move the window from the 1st display to the 2nd display again.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(second_center_point, window->GetBoundsInScreen().CenterPoint());

  // Move the window from the 2nd display to the 1st display after its shelf has
  // been changed back to bottom aligned. The window should be remapped to the
  // center of the display instead of restoring back to its previous remapped
  // bounds. As its previous remapped bounds should be removed regardless the
  // display configuration (e.g, different work area) when it was moved to
  // another display.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_EQ(initial_first_display_work_area, GetPrimaryDisplay().work_area());
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(initial_first_display_work_area.CenterPoint(),
            window->GetBoundsInScreen().CenterPoint());
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

  // Create 2 windows, one at the top left of the 1st display, another
  // half-offscreen inside the 2nd display.
  const gfx::Size window_size(200, 100);
  const gfx::Rect initial_bounds_1st(window_size);
  aura::Window* window1 = CreateTestWindowInShellWithBounds(initial_bounds_1st);
  wm::ActivateWindow(window1);
  const gfx::Rect initial_bounds_2nd(gfx::Point(900, 0), window_size);
  aura::Window* window2 = CreateTestWindowInShellWithBounds(initial_bounds_2nd);
  wm::ActivateWindow(window2);

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
  // `window2` is fully visible inside the primary display.
  const gfx::Rect remapping_bounds_in_1st(gfx::Point(200, 0), window_size);
  EXPECT_EQ(remapping_bounds_in_1st, window2->GetBoundsInScreen());

  // Reconnect the 2nd display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // `window1` should stay inside the 1st display while `window2` should be
  // moved back to the 2nd display at its previous bounds.
  EXPECT_EQ(initial_bounds_1st, window1->GetBoundsInScreen());
  EXPECT_EQ(initial_bounds_2nd, window2->GetBoundsInScreen());

  // Disconnect the 2nd display again. And change the bounds of `window2` inside
  // the primary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(remapping_bounds_in_1st, window2->GetBoundsInScreen());
  // Change the bounds of `window2` inside the 1st display, make it
  // half-offscreen as well.
  const gfx::Rect updated_bounds_in_1st(gfx::Point(300, 0), window_size);
  window2->SetBounds(updated_bounds_in_1st);
  WindowState::Get(window2)->SetBoundsChangedByUser(true);

  // Reconnects the 2nd display. It should still restore back to its previous
  // bounds even though the user changed its bounds inside the 1st display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(initial_bounds_1st, window1->GetBoundsInScreen());
  EXPECT_EQ(initial_bounds_2nd, window2->GetBoundsInScreen());

  // Disconnect the 2nd display, `window2` should restore to its user-assigned
  // bounds in the 1st display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(updated_bounds_in_1st, window2->GetBoundsInScreen());
}

TEST_F(WindowBoundsTrackerTest, RemoveDisplayInLockScreen) {
  UpdateDisplay("400x300,600x500");

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(GetPrimaryDisplay().id());
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(GetSecondaryDisplay().id());

  // Initially, the window is half-offscreen inside the 2nd display.
  const gfx::Size window_size(200, 100);
  const gfx::Rect initial_bounds_2nd(gfx::Point(900, 0), window_size);
  aura::Window* window = CreateTestWindowInShellWithBounds(initial_bounds_2nd);
  wm::ActivateWindow(window);

  // Enter locked session state and disconnect the 2nd display.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(gfx::Point(200, 0), window_size),
            window->GetBoundsInScreen());

  // Reconnect the 2nd display, `window` should be restored to the 2nd display
  // at its previous bounds even though it is in lock screen.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(initial_bounds_2nd, window->GetBoundsInScreen());

  // Unlock and `window` should still at the correct position.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_EQ(initial_bounds_2nd, window->GetBoundsInScreen());
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

// Tests the windows remapping and restoration by changing the current primary
// display.
TEST_F(WindowBoundsTrackerTest, ChangeCurrentPrimaryDisplay) {
  UpdateDisplay("400x300,600x500");

  display::Display first_display = GetPrimaryDisplay();
  display::Display secondary_display = GetSecondaryDisplay();
  const int64_t first_display_id = first_display.id();
  const int64_t secondary_display_id = secondary_display.id();
  // Initially, the 1st display is the primary display.
  ASSERT_EQ(first_display_id, WindowTreeHostManager::GetPrimaryDisplayId());

  const gfx::Point center_point_1st = first_display.work_area().CenterPoint();
  const gfx::Size window_size(200, 100);
  // `w1` is at the center of the 1st display.
  const gfx::Rect w1_initial_bounds(
      gfx::Point(center_point_1st.x() - window_size.width() / 2,
                 center_point_1st.y() - window_size.height() / 2),
      window_size);
  // `w2` is half-offscreen inside the 2nd display.
  const gfx::Rect w2_initial_bounds(gfx::Point(900, 0), window_size);
  aura::Window* w1 = CreateTestWindowInShellWithBounds(w1_initial_bounds);
  wm::ActivateWindow(w1);
  aura::Window* w2 = CreateTestWindowInShellWithBounds(w2_initial_bounds);
  wm::ActivateWindow(w2);

  auto* window_tree_host_manager = Shell::Get()->window_tree_host_manager();
  // Set the 2nd display as the primary display, which will swap the root
  // windows of the two displays.
  window_tree_host_manager->SetPrimaryDisplayId(secondary_display_id);
  ASSERT_EQ(secondary_display_id, WindowTreeHostManager::GetPrimaryDisplayId());
  auto* screen = display::Screen::GetScreen();
  screen->GetDisplayWithDisplayId(first_display_id, &first_display);
  screen->GetDisplayWithDisplayId(secondary_display_id, &secondary_display);
  const gfx::Point center_point_2nd =
      secondary_display.work_area().CenterPoint();
  const gfx::Rect center_position_2nd(
      gfx::Point(center_point_2nd.x() - window_size.width() / 2,
                 center_point_2nd.y() - window_size.height() / 2),
      window_size);
  const gfx::Rect fully_visible_bounds_1st(
      gfx::Point(first_display.bounds().right() - window_size.width(), 0),
      window_size);
  // `w1` was at the center of the 1st display, it should stay at the center of
  // the 2nd display after swapping.
  EXPECT_EQ(center_position_2nd, w1->GetBoundsInScreen());
  // `w2` was half-offscreen at the right top of the 2nd display, it should
  // still be at the right top but fully visible after being swapped to the 1st
  // display.
  EXPECT_EQ(fully_visible_bounds_1st, w2->GetBoundsInScreen());

  // Set the 1st display back to be the primary display, `w1` and `w2` should
  // restore to their initial bounds inside the display.
  window_tree_host_manager->SetPrimaryDisplayId(first_display_id);
  EXPECT_EQ(w1_initial_bounds, w1->GetBoundsInScreen());
  EXPECT_EQ(w2_initial_bounds, w2->GetBoundsInScreen());
}

TEST_F(WindowBoundsTrackerTest, MirrorBuiltinPrimaryDisplay) {
  UpdateDisplay("400x300,600x500");

  display::Display first_display = GetPrimaryDisplay();
  display::Display secondary_display = GetSecondaryDisplay();
  const int64_t first_display_id = first_display.id();
  ASSERT_EQ(first_display_id, WindowTreeHostManager::GetPrimaryDisplayId());

  // `w1` is at the top left of the 1st display, a little bit off the screen.
  const gfx::Size window_size(200, 100);
  const gfx::Rect w1_initial_bounds(gfx::Rect(gfx::Point(-10, 0), window_size));
  // `w2` is half-offscreen inside the 2nd display.
  const gfx::Rect w2_initial_bounds(gfx::Rect(gfx::Point(900, 0), window_size));
  aura::Window* w1 = CreateTestWindowInShellWithBounds(w1_initial_bounds);
  wm::ActivateWindow(w1);
  aura::Window* w2 = CreateTestWindowInShellWithBounds(w2_initial_bounds);
  wm::ActivateWindow(w2);

  // Enter mirror mode.
  const gfx::Rect w2_remapping_bounds_in_1st(
      gfx::Rect(gfx::Point(200, 0), window_size));
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(first_display_id, display_manager()->mirroring_source_id());
  EXPECT_EQ(w1_initial_bounds, w1->GetBoundsInScreen());
  EXPECT_EQ(w2_remapping_bounds_in_1st, w2->GetBoundsInScreen());

  // Exit mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(w2_initial_bounds, w2->GetBoundsInScreen());

  // TODO(b/261122785): Add the test case for mirroring the built-in display
  // when it is not the current primary display. But the external display is the
  // primary, which means it will remove the primary display while mirroring.
}

}  // namespace ash
