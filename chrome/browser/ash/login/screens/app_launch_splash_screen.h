// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_LAUNCH_SPLASH_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_LAUNCH_SPLASH_SCREEN_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

class ErrorScreen;

// Controller for the app launch splash screen.
// The app launch splash screen is shown during the launching of a kiosk app
// to provide a visual feedback while the app is loading.
class AppLaunchSplashScreen : public BaseScreen {
 public:
  class Delegate {
   public:
    // Invoked when the configure network control is clicked.
    virtual void OnConfigureNetwork() = 0;

    // Invoked when the network config did prepare network and is closed.
    virtual void OnNetworkConfigFinished() = 0;
  };

  // The data struct needed to populate this screen.
  struct Data {
    Data(std::string_view name, gfx::ImageSkia icon, const GURL& url);
    Data(const Data&) = delete;
    Data(Data&&);
    Data& operator=(const Data&) = delete;
    Data& operator=(Data&&);
    ~Data();

    // The name of the app.
    std::string name;
    // The icon of the app.
    gfx::ImageSkia icon;
    // The URL of the app.
    GURL url;
  };

  using TView = AppLaunchSplashScreenView;

  explicit AppLaunchSplashScreen(base::WeakPtr<AppLaunchSplashScreenView> view,
                                 ErrorScreen* error_screen,
                                 const base::RepeatingClosure& exit_callback);

  AppLaunchSplashScreen(const AppLaunchSplashScreen&) = delete;
  AppLaunchSplashScreen& operator=(const AppLaunchSplashScreen&) = delete;

  ~AppLaunchSplashScreen() override;

  // Sets whether configure network control is visible.
  void ToggleNetworkConfig(bool visible);

  // Continues app launch after error screen is shown.
  virtual void ContinueAppLaunch();

  // Sets the network configuration controller.
  void SetDelegate(Delegate* delegate);

  // Sets the current app launch state.
  virtual void UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState state);

  // Shows the network error and configure UI.
  virtual void ShowNetworkConfigureUI(NetworkStateInformer::State state,
                                      const std::string& network_name);

  // Shows a notification bar with error message.
  virtual void ShowErrorMessage(KioskAppLaunchError::Error error);

  // Sets the app data to populate the screen. If the screen is not shown, the
  // data is stored to populate the screen in the next show.
  virtual void SetAppData(Data data);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  AppLaunchSplashScreenView::AppLaunchState state_ =
      AppLaunchSplashScreenView::AppLaunchState::kPreparingProfile;

  Data app_data_{/*name=*/std::string(), /*icon=*/gfx::ImageSkia(),
                 /*url=*/GURL()};

  raw_ptr<Delegate> delegate_ = nullptr;

 private:
  void HandleConfigureNetwork();

  base::WeakPtr<AppLaunchSplashScreenView> view_;

  raw_ptr<ErrorScreen, DanglingUntriaged> error_screen_;

  base::RepeatingClosure exit_callback_;

  // If this has value it will be populated through ToggleNetworkConfig(value)
  // after screen is shown. Cleared after screen was shown.
  std::optional<bool> toggle_network_config_on_show_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_LAUNCH_SPLASH_SCREEN_H_
