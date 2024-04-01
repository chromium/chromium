// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/game_controls_test_base.h"

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/lottie/resource.h"

namespace arc::input_overlay {

GameControlsTestBase::GameControlsTestBase()
    : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>(
              base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

GameControlsTestBase::~GameControlsTestBase() = default;

TouchInjector* GameControlsTestBase::GetTouchInjector(aura::Window* window) {
  auto it =
      arc_test_input_overlay_manager_->input_overlay_enabled_windows_.find(
          window);

  return it != arc_test_input_overlay_manager_->input_overlay_enabled_windows_
                     .end()
             ? it->second.get()
             : nullptr;
}

DisplayOverlayController* GameControlsTestBase::GetDisplayOverlayController() {
  return arc_test_input_overlay_manager_->display_overlay_controller_.get();
}

void GameControlsTestBase::EnableDisplayMode(DisplayMode mode) {
  DCHECK(widget_);
  UpdateFlagAndProperty(widget_->GetNativeWindow(),
                        ash::ArcGameControlsFlag::kEdit,
                        /*turn_on=*/mode == DisplayMode::kEdit);
}

void GameControlsTestBase::SetUp() {
  ui::ResourceBundle::SetLottieParsingFunctions(
      &lottie::ParseLottieAsStillImage, &lottie::ParseLottieAsThemedStillImage);

  ash::AshTestBase::SetUp();

  scoped_feature_list_.InitAndEnableFeature(ash::features::kGameDashboard);

  profile_ = std::make_unique<TestingProfile>();
  arc_app_test_.set_wait_compatibility_mode(true);
  arc_app_test_.SetUp(profile_.get());
  SimulatedAppInstalled(task_environment(), arc_app_test_, kEnabledPackageName,
                        /*is_gc_opt_out=*/false,
                        /*is_game=*/true);

  arc_test_input_overlay_manager_ = base::WrapUnique(
      new ArcInputOverlayManager(/*BrowserContext=*/nullptr,
                                 /*ArcBridgeService=*/nullptr));

  // Create a GIO enabled ARC window in the middle of the primary root window.
  ash::Shell::GetPrimaryRootWindow()->SetBounds(gfx::Rect(1000, 800));
  widget_ = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(310, 300, 300, 280), kEnabledPackageName);

  touch_injector_ = GetTouchInjector(widget_->GetNativeWindow());
  controller_ = GetDisplayOverlayController();
}

void GameControlsTestBase::TearDown() {
  widget_.reset();

  arc_test_input_overlay_manager_->Shutdown();
  arc_test_input_overlay_manager_.reset();
  arc_app_test_.TearDown();
  profile_.reset();
  ash::AshTestBase::TearDown();
}

}  // namespace arc::input_overlay
