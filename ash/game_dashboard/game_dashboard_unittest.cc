// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

class GameDashboardTest : public AshTestBase {
 protected:
  GameDashboardTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~GameDashboardTest() override = default;

  void SetUp() override {
    base::SysInfo::SetChromeOSVersionInfoForTest(
        "CHROMEOS_RELEASE_TRACK=testimage-channel",
        base::SysInfo::GetLsbReleaseTime());
    scoped_feature_list_.InitAndEnableFeature({features::kGameDashboard});
    AshTestBase::SetUp();
    EXPECT_TRUE(features::IsGameDashboardEnabled());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    base::SysInfo::ResetChromeOSVersionInfoForTest();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GameDashboardTest, StartStopController) {
  GameDashboardController* controller =
      Shell::Get()->game_dashboard_controller();
  ASSERT_TRUE(controller);

  // Without a focused game window, the controller shouldn't start.
  controller->Start(nullptr);
  EXPECT_FALSE(controller->IsActive(nullptr));

  // With a non-ARC app window, the controller shouldn't start, even if start is
  // requested.
  auto owned_browser_window =
      CreateAppWindow(gfx::Rect(5, 5, 20, 20), ash::AppType::BROWSER);
  aura::Window* browser_window = owned_browser_window.get();
  EXPECT_FALSE(controller->IsActive(browser_window));
  controller->Start(browser_window);
  EXPECT_FALSE(controller->IsActive(browser_window));

  // Make sure it is safe to try to stop an inactive controller.
  controller->Stop(browser_window);
  EXPECT_FALSE(controller->IsActive(browser_window));

  // With an active ARC window, the controller should start.
  auto owned_arc_window =
      CreateAppWindow(gfx::Rect(5, 5, 20, 20), ash::AppType::ARC_APP);
  aura::Window* arc_window = owned_arc_window.get();
  controller->Start(arc_window);
  EXPECT_TRUE(controller->IsActive(arc_window));

  // Make sure an active controller stops.
  controller->Stop(arc_window);
  EXPECT_FALSE(controller->IsActive(arc_window));

  // Test start following a stop.
  controller->Start(arc_window);
  EXPECT_TRUE(controller->IsActive(arc_window));
}

TEST_F(GameDashboardTest, DestroyWindow) {
  GameDashboardController* controller =
      Shell::Get()->game_dashboard_controller();
  ASSERT_TRUE(controller);

  // With an active ARC window, the controller should start.
  auto owned_window =
      CreateAppWindow(gfx::Rect(5, 5, 20, 20), ash::AppType::ARC_APP);
  aura::Window* window = owned_window.get();
  controller->Start(window);
  EXPECT_TRUE(controller->IsActive(window));

  owned_window.reset();
  EXPECT_FALSE(controller->IsActive(window));
}

}  // namespace ash
