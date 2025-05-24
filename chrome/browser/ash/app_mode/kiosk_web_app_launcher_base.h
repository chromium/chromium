// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_WEB_APP_LAUNCHER_BASE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_WEB_APP_LAUNCHER_BASE_H_

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"

namespace ash {

class KioskWebAppLauncherBase : public KioskAppLauncher {
 public:
  KioskWebAppLauncherBase(Profile* profile,
                          const AccountId& account_id,
                          KioskAppLauncher::NetworkDelegate* network_delegate);
  KioskWebAppLauncherBase(const KioskWebAppLauncherBase&) = delete;
  KioskWebAppLauncherBase& operator=(const KioskWebAppLauncherBase&) = delete;
  ~KioskWebAppLauncherBase() override;

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override;
  void RemoveObserver(KioskAppLauncher::Observer* observer) override;
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;

 protected:
  Profile* profile() const { return profile_; }
  const AccountId& account_id() const { return account_id_; }
  chromeos::KioskAppServiceLauncher& app_service_launcher() const {
    return CHECK_DEREF(app_service_launcher_.get());
  }

  void NotifyAppPrepared();
  void NotifyLaunchFailed(KioskAppLaunchError::Error error);

 private:
  KioskAppLauncher::ObserverList& observers() { return observers_; }

  // `KioskAppServiceLauncher` callbacks.
  void OnAppLaunched(bool success);
  void OnAppBecomesVisible();

  void InitAppServiceLauncher();
  void OnWebAppInitialized();

  virtual void CheckAppInstallState() = 0;
  virtual const webapps::AppId& GetInstalledWebAppId() = 0;

  KioskAppLauncher::ObserverList observers_;
  raw_ptr<Profile> profile_;
  const AccountId account_id_;

  std::unique_ptr<chromeos::KioskAppServiceLauncher> app_service_launcher_;

  base::WeakPtrFactory<KioskWebAppLauncherBase> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_WEB_APP_LAUNCHER_BASE_H_
