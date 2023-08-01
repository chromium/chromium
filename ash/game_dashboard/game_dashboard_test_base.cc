// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_test_base.h"
#include <cstddef>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "base/system/sys_info.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/window_util.h"

namespace ash {

GameDashboardTestBase::GameDashboardTestBase()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

void GameDashboardTestBase::SetUp() {
  base::SysInfo::SetChromeOSVersionInfoForTest(
      "CHROMEOS_RELEASE_TRACK=testimage-channel",
      base::SysInfo::GetLsbReleaseTime());
  scoped_feature_list_.InitWithFeatures(
      {features::kGameDashboard,
       features::kFeatureManagementGameDashboardRecordGame},
      {});
  AshTestBase::SetUp();
  UpdateDisplay(base::StringPrintf("%d+%d-%dx%d", kScreenBounds.x(),
                                   kScreenBounds.y(), kScreenBounds.width(),
                                   kScreenBounds.height()));
  EXPECT_TRUE(features::IsGameDashboardEnabled());
}

void GameDashboardTestBase::TearDown() {
  AshTestBase::TearDown();
  base::SysInfo::ResetChromeOSVersionInfoForTest();
}

bool GameDashboardTestBase::IsControllerObservingWindow(
    aura::Window* window) const {
  return GameDashboardController::Get()->window_observations_.IsObservingSource(
      window);
}

std::unique_ptr<aura::Window> GameDashboardTestBase::CreateAppWindow(
    const std::string& app_id,
    AppType app_type,
    const gfx::Rect& bounds_in_screen) {
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateAppWindow(bounds_in_screen, app_type);
  EXPECT_TRUE(IsControllerObservingWindow(window.get()));
  IsGameWindowPropertyObserver observer(window.get());
  EXPECT_FALSE(observer.received_on_property_change());
  window->SetProperty(kAppIDKey, app_id);
  EXPECT_TRUE(observer.received_on_property_change());
  return window;
}

IsGameWindowPropertyObserver::IsGameWindowPropertyObserver(aura::Window* window)
    : window_(window) {
  window_->AddObserver(this);
}

IsGameWindowPropertyObserver::~IsGameWindowPropertyObserver() {
  window_->RemoveObserver(this);
}

void IsGameWindowPropertyObserver::OnWindowPropertyChanged(aura::Window* window,
                                                           const void* key,
                                                           intptr_t old) {
  if (key == chromeos::kIsGameKey) {
    received_on_property_change_ = true;
  }
}

}  // namespace ash
