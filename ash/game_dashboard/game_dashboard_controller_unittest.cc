// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <vector>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/test/ash_test_base.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/window_util.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

class IsGameWindowPropertyObserver : public aura::WindowObserver {
 public:
  explicit IsGameWindowPropertyObserver(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
    received_on_property_change = false;
  }

  IsGameWindowPropertyObserver(const IsGameWindowPropertyObserver&) = delete;
  IsGameWindowPropertyObserver& operator=(const IsGameWindowPropertyObserver&) =
      delete;

  ~IsGameWindowPropertyObserver() override { window_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != chromeos::kIsGameKey) {
      return;
    }
    received_on_property_change = true;
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  bool received_on_property_change;

 private:
  aura::Window* window_;
  base::RunLoop run_loop_;
};

}  // namespace

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

  bool IsObservingWindow(aura::Window* window) const {
    return GameDashboardController::Get()
        ->window_observations_.IsObservingSource(window);
  }

  void VerifyIsGameWindowProperty(const char app_id[],
                                  bool expected_is_game,
                                  AppType app_type = AppType::NON_APP) {
    std::unique_ptr<aura::Window> window =
        CreateAppWindow(gfx::Rect(5, 5, 20, 20), app_type);
    EXPECT_TRUE(IsObservingWindow(window.get()));
    const auto observer =
        std::make_unique<IsGameWindowPropertyObserver>(window.get());
    EXPECT_FALSE(observer->received_on_property_change);
    window->SetProperty(kAppIDKey, std::string(app_id));
    observer->Wait();
    EXPECT_TRUE(observer->received_on_property_change);

    EXPECT_EQ(expected_is_game, IsObservingWindow(window.get()));
    EXPECT_EQ(expected_is_game, chromeos::wm::IsGameWindow(window.get()));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests
// -----------------------------------------------------------------------
// Verifies a window is a game if chromeos::kIsGameKey is set to true.
TEST_F(GameDashboardControllerTest, IsGame) {
  auto owned_window = CreateAppWindow();
  EXPECT_FALSE(chromeos::wm::IsGameWindow(owned_window.get()));
  owned_window->SetProperty(chromeos::kIsGameKey, true);
  EXPECT_TRUE(chromeos::wm::IsGameWindow(owned_window.get()));
}

// Verifies a non-normal window type is not a game and not being observed.
TEST_F(GameDashboardControllerTest, IsGameWindowProperty_NonNormalWindowType) {
  auto non_normal_window = CreateTestWindow(
      gfx::Rect(5, 5, 20, 20), aura::client::WindowType::WINDOW_TYPE_MENU);
  const auto observer =
      std::make_unique<IsGameWindowPropertyObserver>(non_normal_window.get());
  EXPECT_FALSE(observer->received_on_property_change);
  EXPECT_FALSE(IsObservingWindow(non_normal_window.get()));
  EXPECT_FALSE(chromeos::wm::IsGameWindow(non_normal_window.get()));
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_GameArcWindow) {
  // Verifies a game ARC window is a game.
  VerifyIsGameWindowProperty(TestGameDashboardDelegate::kGameAppId,
                             true /* expected_is_game */, AppType::ARC_APP);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_OtherArcWindow) {
  // Verifies a not-game ARC window is not a game.
  VerifyIsGameWindowProperty(TestGameDashboardDelegate::kOtherAppId,
                             false /* expected_is_game */, AppType::ARC_APP);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_GFNWindows) {
  // Verifies a GeForceNow window is a game.
  VerifyIsGameWindowProperty(extension_misc::kGeForceNowAppId,
                             true /* expected_is_game */);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_OtherWindows) {
  // Verifies a non-game non-ARC window is not a game.
  VerifyIsGameWindowProperty(TestGameDashboardDelegate::kOtherAppId,
                             false /* expected_is_game */);
}

}  // namespace ash
