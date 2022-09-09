// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/account_id/account_id.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class Browser;
class BrowserWindow;
class Profile;

namespace web_app {
class WebAppUrlLoader;
class WebAppDataRetriever;
}  // namespace web_app

namespace ash {

class WebKioskAppData;

// Object responsible for preparing and launching web kiosk app. Is destroyed
// upon app launch.
class WebKioskAppLauncher : public KioskAppLauncher,
                            public crosapi::BrowserManagerObserver,
                            public exo::WMHelper::ExoWindowObserver {
 public:
  WebKioskAppLauncher(Profile* profile,
                      Delegate* delegate,
                      const AccountId& account_id);
  WebKioskAppLauncher(const WebKioskAppLauncher&) = delete;
  WebKioskAppLauncher& operator=(const WebKioskAppLauncher&) = delete;
  ~WebKioskAppLauncher() override;

  // Replaces data retriever used for new WebAppInstallTask in tests.
  void SetDataRetrieverFactoryForTesting(
      base::RepeatingCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>
          data_retriever_factory);

  // Replaces default browser window with |window| during launch.
  void SetBrowserWindowForTesting(BrowserWindow* window);

  // Replaces current |url_loader_| with one provided.
  void SetUrlLoaderForTesting(
      std::unique_ptr<web_app::WebAppUrlLoader> url_loader);

 private:
  // KioskAppLauncher:
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;
  void RestartLauncher() override;

  // crosapi::BrowserManagerObserver:
  void OnStateChanged() override;

  // exo::WMHelper::ExoWindowObserver:
  void OnExoWindowCreated(aura::Window* window) override;

  // Callback method triggered after web application and its icon are obtained
  // from `WebKioskAppManager`.
  void OnAppDataObtained(
      web_app::WebAppInstallTask::WebAppInstallInfoOrErrorCode);

  // Callback method triggered after the lacros-chrome window is created.
  void OnLacrosWindowCreated(crosapi::mojom::CreationResult result);

  // Create a new lacros-chrome window.
  void CreateNewLacrosWindow();

  // Get the current web application to be launched in the session.
  const WebKioskAppData* GetCurrentApp() const;

  bool is_installed_ = false;  // Whether the installation was completed.
  Profile* const profile_;
  const AccountId account_id_;

  Browser* browser_ = nullptr;  // Browser instance that runs the web kiosk app.

  std::unique_ptr<web_app::WebAppInstallTask>
      install_task_;  // task that is used to install the app.
  std::unique_ptr<web_app::WebAppUrlLoader>
      url_loader_;  // Loads the app to be installed.

  // Produces retrievers used to obtain app data during installation.
  base::RepeatingCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>
      data_retriever_factory_;

  BrowserWindow* test_browser_window_ = nullptr;

  // Observe the launch state of `BrowserManager`, and launch the lacros-chrome
  // when it is ready. This object is only used when Lacros is enabled.
  base::ScopedObservation<crosapi::BrowserManager,
                          crosapi::BrowserManagerObserver>
      observation_{this};

  base::WeakPtrFactory<WebKioskAppLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the //chrome/browser/chromeos
// migration is finished.
namespace chromeos {
using ::ash::WebKioskAppLauncher;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
