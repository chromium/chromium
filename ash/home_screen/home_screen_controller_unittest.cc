// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"

namespace ash {
namespace {

class HomeScreenControllerTest : public AshTestBase {
 public:
  HomeScreenControllerTest() = default;
  ~HomeScreenControllerTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  std::unique_ptr<aura::Window> CreatePopupTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400),
                                         aura::client::WINDOW_TYPE_POPUP);
  }

  HomeScreenController* home_screen_controller() {
    return Shell::Get()->home_screen_controller();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeScreenControllerTest);
};

TEST_F(HomeScreenControllerTest, OnlyMinimizeCycleListWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreatePopupTestWindow());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::ET_MOUSE_PRESSED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  home_screen_controller()->GoHome(GetPrimaryDisplay().id());
  ASSERT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  ASSERT_FALSE(WindowState::Get(w2.get())->IsMinimized());
}

// Tests that the home screen is visible after rotating the screen in overview
// mode.
TEST_F(HomeScreenControllerTest,
       HomeScreenVisibleAfterDisplayUpdateInOverview) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();

  // Trigger a display configuration change, this simulates screen rotation.
  Shell::Get()->app_list_controller()->OnDisplayConfigurationChanged();

  // End overview mode, the home launcher should be visible.
  overview_controller->EndOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);

  EXPECT_TRUE(
      home_screen_controller()->delegate()->GetHomeScreenWindow()->IsVisible());
}

}  // namespace
}  // namespace ash
