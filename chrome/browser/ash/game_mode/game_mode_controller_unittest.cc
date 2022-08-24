// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/game_mode_controller.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_features_parser.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/widgets.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace game_mode {
namespace {

using borealis::BorealisFeatures;
using borealis::BorealisGameModeResult;
using borealis::BorealisServiceFake;
using borealis::BorealisWindowManager;
using borealis::CreateFakeWidget;
using borealis::kBorealisGameModeResultHistogram;

class GameModeControllerTest : public ChromeAshTestBase {
 public:
  GameModeControllerTest()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    fake_resourced_client_ = new ash::FakeResourcedClient();
    profile_ = std::make_unique<TestingProfile>();
    borealis_service_fake_ =
        BorealisServiceFake::UseFakeForTesting(profile_.get());
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(profile_.get());
    borealis_service_fake_->SetWindowManagerForTesting(
        borealis_window_manager_.get());
    game_mode_controller_ = std::make_unique<GameModeController>();
    features_ = std::make_unique<BorealisFeatures>(profile_.get());
    borealis_service_fake_->SetFeaturesForTesting(features_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    game_mode_controller_.reset();
    histogram_tester_.reset();
    ash::ResourcedClient::Shutdown();
    ChromeAshTestBase::TearDown();
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  std::unique_ptr<GameModeController> game_mode_controller_;
  std::unique_ptr<BorealisFeatures> features_;
  BorealisServiceFake* borealis_service_fake_;
  ash::FakeResourcedClient* fake_resourced_client_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(GameModeControllerTest, ChangingFullScreenTogglesGameMode_Borealis) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerTest, NonBorealisWindowDoesNotEnterGameMode) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::Widget::GetTopLevelWidgetForNativeView(window.get())
      ->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window.get())->IsFullscreen());
  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerTest, SwitchingWindowsTogglesGameMode) {
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

TEST_F(GameModeControllerTest, DestroyingWindowExitsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  test_widget.reset();

  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerTest, SwitchingWindowsMaintainsGameMode) {
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

TEST_F(GameModeControllerTest, SetGameModeFailureDoesNotCrash) {
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      absl::nullopt);
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
}

TEST_F(GameModeControllerTest, GameModeRefreshes) {
  std::unique_ptr<views::Widget> test_widget =
      CreateFakeWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerTest, GameModeMetricsRecorded) {
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
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      ash::ResourcedClient::GameMode::OFF);
  task_environment()->FastForwardBy(base::Seconds(61));
  EXPECT_EQ(3, fake_resourced_client_->get_enter_game_mode_count());
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kAttempted, 1);
  histogram_tester_->ExpectBucketCount(kBorealisGameModeResultHistogram,
                                       BorealisGameModeResult::kFailed, 1);

  // Previous game mode timed out/failed followed by exit.
  // Should not record to histogram as it was already recorded above.
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      ash::ResourcedClient::GameMode::OFF);
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

TEST_F(GameModeControllerTest, BorealisWindowLosesFocusAndGoesFullscreen) {
  // If a game window without focus goes fullscreen, game mode should not
  // activate.
  std::unique_ptr<views::Widget> borealis_widget =
      CreateFakeWidget("org.chromium.borealis.foo");
  std::unique_ptr<views::Widget> other_widget =
      CreateFakeWidget("org.chromium.other.baz");

  // other_widget is non-Borealis and has focus.
  borealis_widget->SetFullscreen(true);

  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

class GameModeControllerForArcTest : public GameModeControllerTest {
 protected:
  void SetUp() override {
    features_.InitAndEnableFeature(arc::kGameModeFeature);

    GameModeControllerTest::SetUp();

    arc_app_test_.SetUp(profile_.get());

    focus_client_ =
        aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(focus_client_);
  }

  void TearDown() override {
    game_mode::ClearArcPkgNamesForTesting();
    focus_client_ = nullptr;
    arc_app_test_.TearDown();

    GameModeControllerTest::TearDown();
  }

  std::unique_ptr<views::Widget> CreateArcTaskWidget(int task_id) {
    ash::TestWidgetBuilder builder;
    builder.SetShow(false);
    auto widget = builder.BuildOwnsNativeWidget();
    exo::SetShellApplicationId(
        widget->GetNativeWindow(),
        base::StringPrintf("org.chromium.arc.%d", task_id));
    return widget;
  }

  ArcAppTest arc_app_test_;
  aura::client::FocusClient* focus_client_ = nullptr;
  base::test::ScopedFeatureList features_;
};

TEST_F(GameModeControllerForArcTest, ChangingFullScreenTogglesGameMode) {
  arc_app_test_.app_instance()->SetTaskInfo(42, "org.funstuff.client",
                                            "activity");
  game_mode::AddArcPkgNameForTesting("org.funstuff.client");

  auto game_widget = CreateArcTaskWidget(42);
  game_widget->Show();

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);

  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::ARC);

  EXPECT_EQ(0, fake_resourced_client_->get_exit_game_mode_count());
  game_widget->SetFullscreen(false);
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, SwitchToNonGameArcAppTurnsOffGameMode) {
  arc_app_test_.app_instance()->SetTaskInfo(2424, "net.another.game",
                                            "activity");
  game_mode::AddArcPkgNameForTesting("net.another.game");

  auto game_widget = CreateArcTaskWidget(2424);
  game_widget->Show();

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);

  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::ARC);

  arc_app_test_.app_instance()->set_app_category_of_pkg(
        "net.recipes.search", arc::mojom::AppCategory::kProductivity);
  arc_app_test_.app_instance()->SetTaskInfo(9999, "net.recipes.search",
                                            "activity");

  EXPECT_EQ(0, fake_resourced_client_->get_exit_game_mode_count());
  auto app_widget = CreateArcTaskWidget(9999);
  app_widget->Show();
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerForArcTest,
       SwitchToNonArcWindowAndBackTurnsOffGameMode) {
  arc_app_test_.app_instance()->SetTaskInfo(42, "org.some.game", "activity");
  game_mode::AddArcPkgNameForTesting("org.some.game");

  auto game_widget = CreateArcTaskWidget(42);
  game_widget->Show();

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);

  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::ARC);

  EXPECT_EQ(0, fake_resourced_client_->get_exit_game_mode_count());
  auto other_window = CreateTestWindow();
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());

  // Move focus back to window already fullscreen, and we should turn game mode
  // back on.
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
  EXPECT_TRUE(game_widget->IsFullscreen());
  game_widget->Show();
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, SwitchToBorealisWindowAndBack) {
  arc_app_test_.app_instance()->SetTaskInfo(14, "jp.foo.game", "activity");
  game_mode::AddArcPkgNameForTesting("jp.foo.game");

  std::unique_ptr<views::Widget> non_game_widget =
      CreateFakeWidget("org.chromium.other.baz");
  non_game_widget->Show();

  auto game_widget = CreateArcTaskWidget(14);

  std::unique_ptr<views::Widget> borealis_widget =
      CreateFakeWidget("org.chromium.borealis.foo");

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);
  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());

  game_widget->Show();
  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::ARC);
  borealis_widget->Show();
  borealis_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::BOREALIS);
  game_widget->Show();
  EXPECT_EQ(2, fake_resourced_client_->get_exit_game_mode_count());
  EXPECT_EQ(3, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, IdentifyGameWithGetAppCategory) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.an_awesome.game", arc::mojom::AppCategory::kGame);
  arc_app_test_.app_instance()->SetTaskInfo(
      9882, "org.an_awesome.game", "activity");

  auto game_widget = CreateArcTaskWidget(9882);
  game_widget->Show();
  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);
  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

}  // namespace
}  // namespace game_mode
