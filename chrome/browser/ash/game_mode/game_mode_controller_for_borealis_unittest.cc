// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/browser/ash/game_mode/testing/game_mode_controller_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "ui/views/widget/widget.h"

namespace game_mode {
namespace {

using GameMode = ash::ResourcedClient::GameMode;
using borealis::BorealisFeatures;
using borealis::BorealisServiceFake;
using borealis::BorealisWindowManager;
using borealis::CreateFakeWidget;

class GameModeControllerForBorealisTest : public GameModeControllerTestBase {
 protected:
  void SetUp() override {
    GameModeControllerTestBase::SetUp();
    borealis_service_fake_ =
        BorealisServiceFake::UseFakeForTesting(profile_.get());
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(profile_.get());
    borealis_service_fake_->SetWindowManagerForTesting(
        borealis_window_manager_.get());
    features_ = std::make_unique<BorealisFeatures>(profile_.get());
    borealis_service_fake_->SetFeaturesForTesting(features_.get());
  }

  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  std::unique_ptr<BorealisFeatures> features_;
  raw_ptr<BorealisServiceFake> borealis_service_fake_;
};

TEST_F(GameModeControllerForBorealisTest,
       ChangingFullScreenTogglesGameMode_Borealis) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest,
       NonBorealisWindowDoesNotEnterGameMode) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::Widget::GetTopLevelWidgetForNativeView(window.get())
      ->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window.get())->IsFullscreen());
  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest, SwitchingWindowsTogglesGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  std::unique_ptr<views::Widget> other_test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.bar");
  aura::Window* other_window = other_test_widget->GetNativeWindow();

  EXPECT_TRUE(other_window->HasFocus());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());

  window->Focus();

  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest, DestroyingWindowExitsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  test_widget.reset();

  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest, SwitchingWindowsMaintainsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  std::unique_ptr<views::Widget> other_test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);

  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  window->Focus();
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest, SetGameModeFailureDoesNotCrash) {
  fake_resourced_client_->set_set_game_mode_with_timeout_response(std::nullopt);
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
}

TEST_F(GameModeControllerForBorealisTest, GameModeRefreshes) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest, GameModeMetricsRecorded) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kFailed, 0);

  // Game mode refreshes
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kFailed, 0);

  // Previous game mode timed out/failed followed by refresh.
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      ash::ResourcedClient::GameMode::OFF);
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(3, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kFailed, 1);

  // Previous game mode timed out/failed followed by exit.
  // Should not record to histogram as it was already recorded above.
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      ash::ResourcedClient::GameMode::OFF);
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kFailed, 1);

  // Having left game mode, the time spent in game mode should be recorded.
  histogram_tester_->ExpectTimeBucketCount(kTimeInGameModeHistogramName,
                                           base::Seconds(122), 1);

  // Enter game mode again, should record attempted again.
  test_widget->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(4, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kAttempted, 2);
  histogram_tester_->ExpectBucketCount(kGameModeResultHistogramName,
                                       GameModeResult::kFailed, 1);
}

TEST_F(GameModeControllerForBorealisTest, WindowLosesFocusAndGoesFullscreen) {
  // If a game window without focus goes fullscreen, game mode should not
  // activate.
  std::unique_ptr<views::Widget> borealis_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo");
  std::unique_ptr<views::Widget> other_widget =
      CreateFakeWidget("org.chromium.other.baz");

  // other_widget is non-Borealis and has focus.
  borealis_widget->SetFullscreen(true);

  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForBorealisTest, TriggersObserver) {
  struct State {
    GameMode game_mode;
    raw_ptr<aura::Window> window = nullptr;
  };

  State state;
  game_mode_controller_->set_game_mode_changed_callback(
      base::BindLambdaForTesting(
          [&](aura::Window* window, ash::ResourcedClient::GameMode mode) {
            state.window = window;
            state.game_mode = mode;
          }));

  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.guest_os.borealis.foo", false);

  test_widget->SetFullscreen(true);
  EXPECT_EQ(state.game_mode, ash::ResourcedClient::GameMode::BOREALIS);
  EXPECT_EQ(state.window, test_widget->GetNativeWindow());

  state.window = nullptr;

  test_widget->SetFullscreen(false);

  EXPECT_EQ(state.game_mode, ash::ResourcedClient::GameMode::OFF);
  EXPECT_EQ(state.window, test_widget->GetNativeWindow());
  state.window = nullptr;
}

}  // namespace
}  // namespace game_mode
