// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/testing/game_mode_controller_test_base.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/shell.h"
#include "ash/test/test_widget_builder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/testing/widgets.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "components/exo/shell_surface_util.h"
#include "ui/views/widget/widget.h"

namespace game_mode {
namespace {

class GameModeControllerForArcTest : public GameModeControllerTestBase {
 protected:
  void SetUp() override {
    features_.InitAndEnableFeature(arc::kGameModeFeature);

    GameModeControllerTestBase::SetUp();

    arc_app_test_.SetUp(profile_.get());

    focus_client_ =
        aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(focus_client_);
  }

  void TearDown() override {
    game_mode::ClearArcPkgNamesForTesting();
    focus_client_ = nullptr;
    arc_app_test_.TearDown();

    GameModeControllerTestBase::TearDown();
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

  auto non_game_widget =
      ash::TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();

  auto game_widget = CreateArcTaskWidget(14);

  std::unique_ptr<views::Widget> borealis_widget =
      borealis::CreateFakeWidget("org.chromium.borealis.foo");

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
      ash::ResourcedClient::GameMode::ARC);
  game_widget->Show();
  EXPECT_EQ(2, fake_resourced_client_->get_exit_game_mode_count());
  EXPECT_EQ(3, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, IdentifyGameWithGetAppCategory) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.an_awesome.game", arc::mojom::AppCategory::kGame);
  arc_app_test_.app_instance()->SetTaskInfo(9882, "org.an_awesome.game",
                                            "activity");

  auto game_widget = CreateArcTaskWidget(9882);
  game_widget->Show();
  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);
  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, RecordLengthOfGameModeHistogram) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.an_awesome.game", arc::mojom::AppCategory::kGame);
  arc_app_test_.app_instance()->SetTaskInfo(9882, "org.an_awesome.game",
                                            "activity");

  auto game_widget = CreateArcTaskWidget(9882);

  histogram_tester_->ExpectBucketCount(
      TimeInGameModeHistogramName(GameMode::ARC), 5000.0, 0);

  game_widget->Show();
  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);
  game_widget->SetFullscreen(true);
  task_environment()->FastForwardBy(base::Seconds(5));
  game_widget->SetFullscreen(false);

  histogram_tester_->ExpectBucketCount(
      TimeInGameModeHistogramName(GameMode::ARC), 5000.0, 1);
}

TEST_F(GameModeControllerForArcTest, RecordGameModeResultHistogram) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.an_awesome.gameedu", arc::mojom::AppCategory::kGame);
  arc_app_test_.app_instance()->SetTaskInfo(9882, "org.an_awesome.gameedu",
                                            "activity");

  auto game_widget = CreateArcTaskWidget(9882);
  game_widget->SetFullscreen(true);
  histogram_tester_->ExpectBucketCount(
      GameModeResultHistogramName(GameMode::ARC), GameModeResult::kAttempted,
      0);
  histogram_tester_->ExpectBucketCount(
      GameModeResultHistogramName(GameMode::ARC), GameModeResult::kFailed, 0);

  game_widget->Show();
  histogram_tester_->ExpectBucketCount(
      GameModeResultHistogramName(GameMode::ARC), GameModeResult::kAttempted,
      1);
  histogram_tester_->ExpectBucketCount(
      GameModeResultHistogramName(GameMode::ARC), GameModeResult::kFailed, 0);

  // Previous game mode timed out/failed followed by refresh.
  fake_resourced_client_->set_set_game_mode_with_timeout_response(
      ash::ResourcedClient::GameMode::OFF);
  task_environment()->FastForwardBy(base::Seconds(61));
  histogram_tester_->ExpectBucketCount(
      GameModeResultHistogramName(GameMode::ARC), GameModeResult::kAttempted,
      1);
  histogram_tester_->ExpectBucketCount(
      GameModeResultHistogramName(GameMode::ARC), GameModeResult::kFailed, 1);
}

}  // namespace
}  // namespace game_mode
