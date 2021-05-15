// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_game_mode_controller.h"

#include "ash/test/test_widget_builder.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/resourced/fake_resourced_client.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace borealis {
namespace {

class BorealisGameModeControllerTest : public ChromeAshTestBase {
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
  }

  void TearDown() override {
    game_mode_controller_.reset();
    chromeos::ResourcedClient::Shutdown();
    ChromeAshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> CreateTestWidget(std::string name,
                                                  bool fullscreen = false) {
    ash::TestWidgetBuilder builder;
    builder.SetShow(false);
    std::unique_ptr<views::Widget> widget = builder.BuildOwnsNativeWidget();
    exo::SetShellApplicationId(widget->GetNativeWindow(), name);
    if (fullscreen) {
      widget->SetFullscreen(true);
    }
    widget->Show();
    return widget;
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BorealisWindowManager> window_manager_;
  std::unique_ptr<BorealisGameModeController> game_mode_controller_;
  std::unique_ptr<BorealisFeatures> features_;
  BorealisServiceFake* service_fake_;
  chromeos::FakeResourcedClient* fake_resourced_client_;
};

TEST_F(BorealisGameModeControllerTest, ChangingFullScreenTogglesGameMode) {
  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(true));
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(false));
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, NonBorealisWindowDoesNotEnterGameMode) {
  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(false));
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::Widget::GetTopLevelWidgetForNativeView(window.get())
      ->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window.get())->IsFullscreen());
  EXPECT_EQ(0, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, SwitchingWindowsTogglesGameMode) {
  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(true));
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(false));
  std::unique_ptr<views::Widget> other_test_widget =
      CreateTestWidget("org.chromium.borealis.bar");
  aura::Window* other_window = other_test_widget->GetNativeWindow();

  EXPECT_TRUE(other_window->HasFocus());
  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(true));
  window->Focus();

  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(2, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, DestroyingWindowExitsGameMode) {
  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(true));
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(false));
  test_widget.reset();

  EXPECT_EQ(1, fake_resourced_client_->get_exit_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, SwitchingWindowsMaintainsGameMode) {
  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(true));
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  std::unique_ptr<views::Widget> other_test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);

  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());

  window->Focus();
  EXPECT_EQ(1, fake_resourced_client_->get_enter_game_mode_count());
}

TEST_F(BorealisGameModeControllerTest, SetGameModeFailureDoesNotCrash) {
  fake_resourced_client_->set_set_game_mode_response(
      absl::optional<bool>(absl::nullopt));
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
}

}  // namespace
}  // namespace borealis
