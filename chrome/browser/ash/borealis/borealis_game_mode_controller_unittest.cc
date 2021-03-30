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
};

TEST_F(BorealisGameModeControllerTest, ChangingFullScreenTogglesGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_NE(nullptr, game_mode_controller_->GetGameModeForTesting());
  test_widget->SetFullscreen(false);
  EXPECT_FALSE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_EQ(nullptr, game_mode_controller_->GetGameModeForTesting());
}

TEST_F(BorealisGameModeControllerTest, NonBorealisWindowDoesNotEnterGameMode) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::Widget::GetTopLevelWidgetForNativeView(window.get())
      ->SetFullscreen(true);
  EXPECT_TRUE(ash::WindowState::Get(window.get())->IsFullscreen());
  EXPECT_EQ(nullptr, game_mode_controller_->GetGameModeForTesting());
}

TEST_F(BorealisGameModeControllerTest, SwitchingWindowsTogglesGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_NE(nullptr, game_mode_controller_->GetGameModeForTesting());

  std::unique_ptr<views::Widget> other_test_widget =
      CreateTestWidget("org.chromium.borealis.bar");
  aura::Window* other_window = other_test_widget->GetNativeWindow();

  EXPECT_TRUE(other_window->HasFocus());
  EXPECT_EQ(nullptr, game_mode_controller_->GetGameModeForTesting());

  window->Focus();

  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_NE(nullptr, game_mode_controller_->GetGameModeForTesting());
}

TEST_F(BorealisGameModeControllerTest, DestroyingWindowExitsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
  EXPECT_NE(nullptr, game_mode_controller_->GetGameModeForTesting());

  test_widget.reset();

  EXPECT_EQ(nullptr, game_mode_controller_->GetGameModeForTesting());
}

TEST_F(BorealisGameModeControllerTest, SwitchingWindowsMaintainsGameMode) {
  std::unique_ptr<views::Widget> test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);
  aura::Window* window = test_widget->GetNativeWindow();
  BorealisGameModeController::ScopedGameMode* game_mode =
      game_mode_controller_->GetGameModeForTesting();
  EXPECT_NE(nullptr, game_mode);

  std::unique_ptr<views::Widget> other_test_widget =
      CreateTestWidget("org.chromium.borealis.foo", true);

  EXPECT_NE(nullptr, game_mode_controller_->GetGameModeForTesting());
  EXPECT_EQ(game_mode, game_mode_controller_->GetGameModeForTesting());

  window->Focus();
  EXPECT_NE(nullptr, game_mode_controller_->GetGameModeForTesting());
  EXPECT_EQ(game_mode, game_mode_controller_->GetGameModeForTesting());
}

}  // namespace
}  // namespace borealis
