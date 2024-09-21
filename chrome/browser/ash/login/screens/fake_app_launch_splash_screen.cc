
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fake_app_launch_splash_screen.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

namespace ash {

FakeAppLaunchSplashScreen::FakeAppLaunchSplashScreen()
    : AppLaunchSplashScreen(/*view=*/nullptr,
                            /*error_screen=*/nullptr,
                            /*exit_callback=*/base::DoNothing()) {}

FakeAppLaunchSplashScreen::~FakeAppLaunchSplashScreen() = default;

void FakeAppLaunchSplashScreen::ShowNetworkConfigureUI(
    NetworkStateInformer::State state,
    const std::string& network_name) {}

void FakeAppLaunchSplashScreen::ContinueAppLaunch() {
  if (delegate_) {
    delegate_->OnNetworkConfigFinished();
  }
}

AppLaunchSplashScreenView::AppLaunchState
FakeAppLaunchSplashScreen::GetAppLaunchState() const {
  return state_;
}

void FakeAppLaunchSplashScreen::ShowErrorMessage(
    KioskAppLaunchError::Error error) {
  launch_error_ = error;
}

KioskAppLaunchError::Error FakeAppLaunchSplashScreen::GetLaunchError() const {
  return launch_error_;
}

const AppLaunchSplashScreen::Data& FakeAppLaunchSplashScreen::GetAppData()
    const {
  return app_data_;
}

}  // namespace ash
