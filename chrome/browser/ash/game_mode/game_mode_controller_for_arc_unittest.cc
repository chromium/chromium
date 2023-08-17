// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/testing/game_mode_controller_test_base.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/test/arc_task_window_builder.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/test_widget_builder.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "ui/views/widget/widget.h"

namespace game_mode {
namespace {

class GameModeControllerForArcTest : public GameModeControllerTestBase {
 protected:
  void SetUp() override {
    features_.InitAndEnableFeature(arc::kGameModeFeature);

    GameModeControllerTestBase::SetUp();

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kEnableArcVm);
    // ARC VM expects the kArcSerialNumberSalt preference to be registered.
    arc::prefs::RegisterLocalStatePrefs(local_pref_service_.registry());

    arc_app_test_.SetUp(profile_.get());

    focus_client_ =
        aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(focus_client_);
  }

  void TearDown() override {
    focus_client_ = nullptr;
    arc_app_test_.TearDown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    GameModeControllerTestBase::TearDown();
  }

  ArcAppTest arc_app_test_;
  raw_ptr<aura::client::FocusClient, ExperimentalAsh> focus_client_ = nullptr;
  base::test::ScopedFeatureList features_;
  TestingPrefServiceSimple local_pref_service_;
};

TEST_F(GameModeControllerForArcTest, ChangingFullScreenTogglesGameMode) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.funstuff.client", arc::mojom::AppCategory::kGame);

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(42)
                         .SetPackageName("org.funstuff.client")
                         .BuildOwnsNativeWidget();
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
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "net.another.game", arc::mojom::AppCategory::kGame);

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(2424)
                         .SetPackageName("net.another.game")
                         .BuildOwnsNativeWidget();
  game_widget->Show();

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);

  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::ARC);

  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "net.recipes.search", arc::mojom::AppCategory::kProductivity);

  EXPECT_EQ(0, fake_resourced_client_->get_exit_game_mode_count());
  auto app_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(9999)
                        .SetPackageName("net.recipes.search")
                        .BuildOwnsNativeWidget();
  app_widget->Show();
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(GameModeControllerForArcTest,
       SwitchToNonArcWindowAndBackTurnsOffGameMode) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.some.game", arc::mojom::AppCategory::kGame);

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(42)
                         .SetPackageName("org.some.game")
                         .BuildOwnsNativeWidget();
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
  arc_app_test_.app_instance()->set_app_category_of_pkg(
    "jp.foo.game", arc::mojom::AppCategory::kGame);

  auto non_game_widget =
      ash::TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(14)
                         .SetPackageName("jp.foo.game")
                         .BuildOwnsNativeWidget();

  std::unique_ptr<views::Widget> borealis_widget =
      borealis::CreateFakeWidget("org.chromium.guest_os.borealis.foo");

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

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(9882)
                         .SetPackageName("org.an_awesome.game")
                         .BuildOwnsNativeWidget();
  game_widget->Show();

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);
  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, IdentifyGameWithKnownGameList) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.an_awesome.game", arc::mojom::AppCategory::kUndefined);

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(9882)
                         .SetPackageName("com.mojang.minecraftedu")
                         .BuildOwnsNativeWidget();
  game_widget->Show();
  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);
  game_widget->SetFullscreen(true);
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(GameModeControllerForArcTest, RecordLengthOfGameModeHistogram) {
  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "org.an_awesome.game", arc::mojom::AppCategory::kGame);

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(9882)
                         .SetPackageName("org.an_awesome.game")
                         .BuildOwnsNativeWidget();

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

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(9882)
                         .SetPackageName("org.an_awesome.gameedu")
                         .BuildOwnsNativeWidget();
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

TEST_F(GameModeControllerForArcTest, DisabledOnContainer) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ash::switches::kEnableArcVm);

  arc_app_test_.app_instance()->set_app_category_of_pkg(
      "net.another.game", arc::mojom::AppCategory::kGame);

  auto game_widget = arc::ArcTaskWindowBuilder()
                         .SetTaskId(2424)
                         .SetPackageName("new.another.game")
                         .BuildOwnsNativeWidget();
  game_widget->Show();

  fake_resourced_client_->set_set_game_mode_response(
      ash::ResourcedClient::GameMode::OFF);

  game_widget->SetFullscreen(true);
  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

}  // namespace
}  // namespace game_mode
