// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_SERVICE_LAUNCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace ash {

class WebKioskAppData;

// Responsible of installing and launching web kiosk app using App Service. It's
// destroyed upon successful app launch.
class WebKioskAppServiceLauncher : public KioskAppLauncher {
 public:
  // Histogram to log the installed web app is a placeholder.
  static constexpr char kWebAppIsPlaceholderUMA[] =
      "Kiosk.AppService.WebApp.IsPlaceholder";

  // Histogram to log the web app install result code.
  static constexpr char kWebAppInstallResultUMA[] =
      "Kiosk.AppService.WebApp.InstallResult";

  WebKioskAppServiceLauncher(
      Profile* profile,
      const AccountId& account_id,
      KioskAppLauncher::NetworkDelegate* network_delegate);
  WebKioskAppServiceLauncher(const WebKioskAppServiceLauncher&) = delete;
  WebKioskAppServiceLauncher& operator=(const WebKioskAppServiceLauncher&) =
      delete;
  ~WebKioskAppServiceLauncher() override;

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override;
  void RemoveObserver(KioskAppLauncher::Observer* observer) override;
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;

 private:
  // `KioskAppServiceLauncher` callbacks.
  void OnWebAppInitialized();
  void NotifyAppPrepared(const std::optional<webapps::AppId>& app_id);
  void OnAppLaunched(bool success);
  void OnAppBecomesVisible();

  void GetInstallState(
      const GURL& url,
      crosapi::mojom::WebKioskInstaller::GetWebKioskInstallStateCallback
          callback);
  void CheckWhetherNetworkIsRequired(crosapi::mojom::WebKioskInstallState state,
                                     const std::optional<webapps::AppId>& id);
  void OnInstallComplete(const std::optional<webapps::AppId>& app_id);

  // Get the current web application to be launched in the session.
  const WebKioskAppData* GetCurrentApp() const;

  raw_ptr<Profile> profile_;
  const AccountId account_id_;
  std::string app_id_;
  KioskAppLauncher::ObserverList observers_;

  std::unique_ptr<chromeos::KioskAppServiceLauncher> app_service_launcher_;

  base::WeakPtrFactory<WebKioskAppServiceLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_SERVICE_LAUNCHER_H_
