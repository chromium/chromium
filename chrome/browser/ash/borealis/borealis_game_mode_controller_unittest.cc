// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_game_mode_controller.h"

#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/widgets.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/resourced/fake_resourced_client.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace borealis {
namespace {

class BorealisGameModeControllerTest : public ChromeAshTestBase {
 public:
  BorealisGameModeControllerTest()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    fake_resourced_client_ = new chromeos::FakeResourcedClient();
    profile_ = std::make_unique<TestingProfile>();
    service_fake_ = BorealisServiceFake::UseFakeForTesting(profile_.get());
    window_manager_ = std::make_unique<BorealisWindowManager>(profile_.get());
    service_fake_->SetWindowManagerForTesting(window_manager_.get());
    game_mode_controller_ = std::make_unique<BorealisGameModeController>();
    features_ = std::make_unique<BorealisFeatures>(profile_.get());
    service_fake_->SetFeaturesForTesting(features_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    game_mode_controller_.reset();
    histogram_tester_.reset();
    chromeos::ResourcedClient::Shutdown();
    ChromeAshTestBase::TearDown();
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BorealisWindowManager> window_manager_;
  std::unique_ptr<BorealisGameModeController> game_mode_controller_;
  std::unique_ptr<BorealisFeatures> features_;
  BorealisServiceFake* service_fake_;
  chromeos::FakeResourcedClient* fake_resourced_client_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(BorealisGameModeControllerTest, ChangingFullScreenTogglesGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, NonBorealisWindowDoesNotEnterGameMode) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::Widget::GetTopLevelWidgetForNativeView(window.get())
      ->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window.get())->IsFullscreen());
  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, SwitchingWindowsTogglesGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  std::unique_ptr<views::Widget> other_test_widget =
      CreateFakeWidget("org.chromium.borealis.bar");
  aura::Window* other_window = other_test_widget->GetNativeWindow();

  EXPECT_TRUE(other_window->HasFocus());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());

  window->Focus();

  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, DestroyingWindowExitsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  test_widget.reset();

  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, SwitchingWindowsMaintainsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  std::unique_ptr<views::Widget> other_test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);

  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  window->Focus();
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, SetGameModeFailureDoesNotCrash) {
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      absl::optional<bool>(absl::nullopt));
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
}

TEST_F(BorealisGameModeControllerTest, GameModeRefreshes) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, GameModeMetricsRecorded) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kFailed, 0);

  // Game mode refreshes
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kFailed, 0);

  // Previous game mode timed out/failed followed by refresh.
  fake_resourced_client_->set_set_game_mode_with_timeout_response(false);
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(3, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kFailed, 1);

  // Previous game mode timed out/failed followed by exit.
  // Should not record to histogram as it was already recorded above.
  fake_resourced_client_->set_set_game_mode_with_timeout_response(false);
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kFailed, 1);

  // Enter game mode again, should record attempted again.
  test_widget->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(4, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kAttempted, 2);
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kFailed, 1);
}

}  // namespace
}  // namespace borealis
