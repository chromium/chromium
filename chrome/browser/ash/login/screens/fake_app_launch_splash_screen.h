// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FAKE_APP_LAUNCH_SPLASH_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FAKE_APP_LAUNCH_SPLASH_SCREEN_H_

#include <string>

#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"

namespace ash {

class FakeAppLaunchSplashScreen : public AppLaunchSplashScreen {
 public:
  FakeAppLaunchSplashScreen();
  ~FakeAppLaunchSplashScreen() override;
  FakeAppLaunchSplashScreen(const FakeAppLaunchSplashScreen&) = delete;
  FakeAppLaunchSplashScreen& operator=(const FakeAppLaunchSplashScreen&) =
      delete;

  // AppLaunchSplashScreen overrides:
  void ShowNetworkConfigureUI(NetworkStateInformer::State state,
                              const std::string& network_name) override;
  void ContinueAppLaunch() override;
  void ShowErrorMessage(KioskAppLaunchError::Error error) override;

  // Returns the app launch error last passed to `ShowErrorMessage`.
  KioskAppLaunchError::Error GetLaunchError() const;

  // Returns the app launch state last passed to
  // `AppLaunchSplashScreen::UpdateAppLaunchState`.
  AppLaunchSplashScreenView::AppLaunchState GetAppLaunchState() const;

  // Returns the app data last passed to `AppLaunchSplashScreen::SetAppData`.
  const Data& GetAppData() const;

 private:
  // The launch error last passed to `ShowErrorMessage`.
  KioskAppLaunchError::Error launch_error_ = KioskAppLaunchError::Error::kNone;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FAKE_APP_LAUNCH_SPLASH_SCREEN_H_
