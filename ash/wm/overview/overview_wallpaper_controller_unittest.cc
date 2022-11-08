// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

float GetWallpaperBlur(const aura::Window* window) {
  return RootWindowController::ForWindow(window)
      ->wallpaper_widget_controller()
      ->GetWallpaperBlur();
}

void CheckWallpaperBlur(float expected) {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows())
    EXPECT_EQ(expected, GetWallpaperBlur(root));
}

}  // namespace

class OverviewWallpaperControllerTest : public AshTestBase {
 public:
  OverviewWallpaperControllerTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kJellyroll);
  }

  OverviewWallpaperControllerTest(const OverviewWallpaperControllerTest&) =
      delete;
  OverviewWallpaperControllerTest& operator=(
      const OverviewWallpaperControllerTest&) = delete;

  ~OverviewWallpaperControllerTest() override { scoped_feature_list_.Reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that entering overview in clamshell mode and toggling to tablet mode
// updates the wallpaper window blur correctly.
TEST_F(OverviewWallpaperControllerTest, OverviewToggleEnterTabletMode) {
  CheckWallpaperBlur(wallpaper_constants::kClear);

  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kOverviewBlur);

  TabletModeControllerTestApi().EnterTabletMode();
  CheckWallpaperBlur(wallpaper_constants::kOverviewBlur);

  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);
}

// Tests that entering overview in tablet mode and toggling to clamshell mode
// updates the wallpaper window blur correctly.
TEST_F(OverviewWallpaperControllerTest, OverviewToggleLeaveTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kOverviewBlur);

  TabletModeControllerTestApi().LeaveTabletMode();
  CheckWallpaperBlur(wallpaper_constants::kOverviewBlur);

  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);
}

// Test fixture for feature flag `kJellyroll`.
class OverviewWallpaperBlurTest : public AshTestBase {
 public:
  OverviewWallpaperBlurTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kJellyroll);
  }

  OverviewWallpaperBlurTest(const OverviewWallpaperBlurTest&) = delete;
  OverviewWallpaperBlurTest& operator=(const OverviewWallpaperBlurTest&) =
      delete;

  ~OverviewWallpaperBlurTest() override { scoped_feature_list_.Reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that entering / exiting overview mode in clamshell / tablet mode won't
// update the wallpaper window blur.
TEST_F(OverviewWallpaperBlurTest, OverviewToggleInTabletAndClamshellMode) {
  CheckWallpaperBlur(wallpaper_constants::kClear);

  // Enter overview mode.
  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  // Exit overview mode.
  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  TabletModeControllerTestApi().EnterTabletMode();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  // Enter overview mode.
  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  // Exit overview mode.
  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);
}

// Verify that entering / exiting tablet mode in overview mode won't update the
// wallpaper window blur.
TEST_F(OverviewWallpaperBlurTest, EnterExitTabletModeWithOverviewModeOn) {
  CheckWallpaperBlur(wallpaper_constants::kClear);

  // Enter overview mode.
  ToggleOverview();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  TabletModeControllerTestApi().EnterTabletMode();
  CheckWallpaperBlur(wallpaper_constants::kClear);

  TabletModeControllerTestApi().LeaveTabletMode();
  CheckWallpaperBlur(wallpaper_constants::kClear);
}

}  // namespace ash
