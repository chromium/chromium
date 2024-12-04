// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/window_util.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace ash {

class GameDashboardControllerTest : public GameDashboardTestBase {
 public:
  GameDashboardControllerTest() = default;
  GameDashboardControllerTest(const GameDashboardControllerTest&) = delete;
  GameDashboardControllerTest& operator=(const GameDashboardControllerTest&) =
      delete;
  ~GameDashboardControllerTest() override = default;

  void VerifyIsGameWindowProperty(aura::Window* window, bool expected_is_game) {
    EXPECT_EQ(expected_is_game, IsControllerObservingWindow(window));
    EXPECT_EQ(expected_is_game, GameDashboardController::IsGameWindow(window));
    EXPECT_EQ(
        expected_is_game,
        GameDashboardController::Get()->game_window_contexts_.contains(window));
  }

  void CreateAppWindowAndVerifyIsGameWindowProperty(
      const char app_id[],
      bool expected_is_game,
      chromeos::AppType app_type = chromeos::AppType::NON_APP) {
    auto window = CreateAppWindow(app_id, app_type, gfx::Rect(5, 5, 20, 20));
    VerifyIsGameWindowProperty(window.get(), expected_is_game);

    // Verify the window's `GameDashboardContext` is deleted after the game
    // window is closed.
    auto* old_window = window.get();
    window.reset();
    EXPECT_FALSE(GameDashboardController::Get()->game_window_contexts_.contains(
        old_window));
  }
};

// Tests
// -----------------------------------------------------------------------
// Verifies a window is a game if chromeos::kIsGameKey is set to true.
TEST_F(GameDashboardControllerTest, IsGame) {
  auto owned_window = AshTestBase::CreateAppWindow();
  EXPECT_FALSE(GameDashboardController::IsGameWindow(owned_window.get()));
  owned_window->SetProperty(chromeos::kIsGameKey, true);
  EXPECT_TRUE(GameDashboardController::IsGameWindow(owned_window.get()));
}

// Verifies a non-normal window type is not a game and not being observed.
TEST_F(GameDashboardControllerTest, IsGameWindowProperty_NonNormalWindowType) {
  auto non_normal_window = CreateTestWindow(
      gfx::Rect(5, 5, 20, 20), aura::client::WindowType::WINDOW_TYPE_MENU);
  const auto observer =
      std::make_unique<IsGameWindowPropertyObserver>(non_normal_window.get());
  EXPECT_FALSE(observer->received_on_property_change());
  EXPECT_FALSE(IsControllerObservingWindow(non_normal_window.get()));
  EXPECT_FALSE(GameDashboardController::IsGameWindow(non_normal_window.get()));
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_GameArcWindow) {
  // Verifies a game ARC window is a game.
  CreateAppWindowAndVerifyIsGameWindowProperty(
      TestGameDashboardDelegate::kGameAppId, /*expected_is_game=*/true,
      chromeos::AppType::ARC_APP);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_AllowlistedArcWindow) {
  // Verifies an allowlisted ARC window is a game.
  CreateAppWindowAndVerifyIsGameWindowProperty(
      TestGameDashboardDelegate::kAllowlistedAppId,
      /*expected_is_game=*/true, chromeos::AppType::ARC_APP);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_OtherArcWindow) {
  // Verifies a not-game ARC window is not a game.
  CreateAppWindowAndVerifyIsGameWindowProperty(
      TestGameDashboardDelegate::kOtherAppId, /*expected_is_game=*/false,
      chromeos::AppType::ARC_APP);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_GFNWindows) {
  // Verifies a GeForceNow window is a game.
  CreateAppWindowAndVerifyIsGameWindowProperty(extension_misc::kGeForceNowAppId,
                                               /*expected_is_game=*/true);
}

TEST_F(GameDashboardControllerTest, IsGameWindowProperty_OtherWindows) {
  // Verifies a non-game non-ARC window is not a game.
  CreateAppWindowAndVerifyIsGameWindowProperty(
      TestGameDashboardDelegate::kOtherAppId, /*expected_is_game=*/false);
}

TEST_F(GameDashboardControllerTest, GameWindowToNonGameWindow) {
  const auto window =
      CreateAppWindow(TestGameDashboardDelegate::kGameAppId,
                      chromeos::AppType::ARC_APP, gfx::Rect(5, 5, 20, 20));
  VerifyIsGameWindowProperty(window.get(), /*expected_is_game=*/true);

  // Update the window property where the window is no longer considered to be a
  // game.
  window->SetProperty(kAppIDKey,
                      std::string(TestGameDashboardDelegate::kOtherAppId));

  VerifyIsGameWindowProperty(window.get(), /*expected_is_game=*/false);
}

}  // namespace ash
