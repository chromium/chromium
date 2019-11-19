// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace web_app {
class WebAppInstallTask;
class WebAppUrlLoader;
}  // namespace web_app

namespace chromeos {

// Object responsible for preparing and launching web kiosk app. Is destroyed
// upon app launch.
class WebKioskAppLauncher {
 public:
  class Delegate {
   public:
    virtual void InitializeNetwork() = 0;
    virtual void OnAppStartedInstalling() = 0;
    virtual void OnAppPrepared() = 0;
    virtual void OnAppLaunched() = 0;
    virtual void OnAppLaunchFailed() = 0;

   protected:
    virtual ~Delegate() {}
  };

  WebKioskAppLauncher(Profile* profile, Delegate* delegate);
  ~WebKioskAppLauncher();

  // Prepares the environment for an app launch.
  void Initialize(const AccountId& account_id);
  // Continues the installation when the network i
  void ContinueWithNetworkReady();
  // Launches the app after the initialization is done.
  void LaunchApp();

 private:
  void OnAppDataObtained(std::unique_ptr<WebApplicationInfo> app_info);

  bool is_installed_ = false;  // Whether the installation was completed.
  AccountId account_id_;
  Profile* const profile_;
  Delegate* const delegate_;  // Not owned. Owns us.

  Browser* browser_ = nullptr;  // Browser instance that runs the web kiosk app.

  std::unique_ptr<web_app::WebAppInstallTask>
      install_task_;  // task that is used to install the app.
  std::unique_ptr<web_app::WebAppUrlLoader>
      url_loader_;  // Loads the app to be installed.

  base::WeakPtrFactory<WebKioskAppLauncher> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebKioskAppLauncher);
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
