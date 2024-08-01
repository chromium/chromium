// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_update_observer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/account_id/account_id.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class Profile;

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

namespace ash {

class WebKioskAppData;

// Does the management of web kiosk apps.
class WebKioskAppManager : public KioskAppManagerBase {
 public:
  static const char kWebKioskDictionaryName[];

  // Whether the manager was already created.
  static bool IsInitialized();

  // Will return the manager instance or will crash if it not yet initiazlied.
  static WebKioskAppManager* Get();
  WebKioskAppManager();
  WebKioskAppManager(const WebKioskAppManager&) = delete;
  WebKioskAppManager& operator=(const WebKioskAppManager&) = delete;
  ~WebKioskAppManager() override;

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Create app instance by app data.
  static KioskAppManagerBase::App CreateAppByData(const WebKioskAppData& data);

  // `KioskAppManagerBase` implementation.
  std::vector<App> GetApps() const override;

  void LoadIcons();

  // Returns auto launched account id. If there is none, account is invalid,
  // thus is_valid() returns empty AccountId.
  const AccountId& GetAutoLaunchAccountId() const;

  // Obtains an app associated with given `account_id`.
  const WebKioskAppData* GetAppByAccountId(const AccountId& account_id) const;

  // Updates app by the data obtained during installation.
  void UpdateAppByAccountId(const AccountId& account_id,
                            const web_app::WebAppInstallInfo& app_info);

  // Updates app by title, start_url and icon_bitmaps.
  void UpdateAppByAccountId(const AccountId& account_id,
                            const std::string& title,
                            const GURL& start_url,
                            const web_app::IconBitmaps& icon_bitmaps);

  // Adds fake apps in tests.
  void AddAppForTesting(const AccountId& account_id, const GURL& install_url);

  // Notify this manager that a Kiosk session started with the given `app_id`.
  void OnKioskSessionStarted(const KioskAppId& app_id);

  // Starts observing web app updates from App Service in a Kiosk session.
  void StartObservingAppUpdate(Profile* profile, const AccountId& account_id);

 private:
  // `KioskAppManagerBase` implementation.
  // Updates `apps_` based on CrosSettings.
  void UpdateAppsFromPolicy() override;

  std::vector<std::unique_ptr<WebKioskAppData>> apps_;
  AccountId auto_launch_account_id_;

  // Observes web Kiosk app updates. Persists through the whole web Kiosk
  // session.
  std::unique_ptr<WebKioskAppUpdateObserver> app_update_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_MANAGER_H_
