// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_SERVICE_LAUNCHER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_web_app_launcher_base.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace ash {

class WebKioskAppData;

// Responsible of installing and launching web kiosk app using App Service. It's
// destroyed upon successful app launch.
class WebKioskAppServiceLauncher : public KioskWebAppLauncherBase {
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
  void Initialize() override;
  void ContinueWithNetworkReady() override;

 private:
  void NotifyAppPrepared(const std::optional<webapps::AppId>& app_id);
  void OnInstallComplete(const std::optional<webapps::AppId>& app_id);

  // Get the current web application to be launched in the session.
  const WebKioskAppData* GetCurrentApp() const;

  void CheckAppInstallState() override;
  const webapps::AppId& GetInstalledWebAppId() override;

  std::optional<webapps::AppId> installed_app_id_;

  base::WeakPtrFactory<WebKioskAppServiceLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_SERVICE_LAUNCHER_H_
