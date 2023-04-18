// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <vector>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace ash {

class GameDashboardControllerTest : public AshTestBase {
 protected:
  GameDashboardControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~GameDashboardControllerTest() override = default;

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

// Tests
// -----------------------------------------------------------------------
// Verifies that GameDashboard is supported only on ARC apps.
TEST_F(GameDashboardControllerTest, IsSupported) {
  // Verifies ARC app windows are supported.
  auto owned_arc_window =
      CreateAppWindow(gfx::Rect(5, 5, 20, 20), ash::AppType::ARC_APP);
  EXPECT_TRUE(
      GameDashboardController::Get()->IsSupported(owned_arc_window.get()));

  // Verifies non-ARC app windows are NOT supported.
  auto owned_browser_window =
      CreateAppWindow(gfx::Rect(5, 5, 20, 20), ash::AppType::BROWSER);
  EXPECT_FALSE(
      GameDashboardController::Get()->IsSupported(owned_browser_window.get()));
}

}  // namespace ash
