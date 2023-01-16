// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/account_id/account_id.h"
#include "components/exo/wm_helper.h"

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
                            public exo::WMHelper::ExoWindowObserver,
                            public ProfileObserver {
 public:
  WebKioskAppLauncher(Profile* profile,
                      const AccountId& account_id,
                      bool should_skip_install,
                      NetworkDelegate* network_delegate);
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

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override;
  void RemoveObserver(KioskAppLauncher::Observer* observer) override;
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;
  void RestartLauncher() override;

 private:
  // crosapi::BrowserManagerObserver:
  void OnStateChanged() override;

  // exo::WMHelper::ExoWindowObserver:
  void OnExoWindowCreated(aura::Window* window) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

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

  void NotifyAppWindowCreated();

  bool is_installed_ = false;  // Whether the installation was completed.
  // |profile_| may become nullptr if the profile is being destroyed.
  Profile* profile_;
  const AccountId account_id_;
  const bool should_skip_install_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  KioskAppLauncher::ObserverList observers_;
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

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_LAUNCHER_H_
