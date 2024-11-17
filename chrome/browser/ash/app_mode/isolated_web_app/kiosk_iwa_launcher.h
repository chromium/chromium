// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_LAUNCHER_H_

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_web_app_launcher_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"

namespace ash {

// TODO(crbug.com/374730382): Add unit tests.
class KioskIwaLauncher : public KioskWebAppLauncherBase {
 public:
  KioskIwaLauncher(Profile* profile,
                   const AccountId& account_id,
                   KioskAppLauncher::NetworkDelegate* network_delegate);
  KioskIwaLauncher(const KioskIwaLauncher&) = delete;
  KioskIwaLauncher& operator=(const KioskIwaLauncher&) = delete;
  ~KioskIwaLauncher() override;

  // `KioskAppLauncher`:
  void Initialize() override;
  void ContinueWithNetworkReady() override;

 private:
  const KioskIwaData& iwa_data() { return iwa_data_.get(); }

  void InstallIsolatedWebApp();
  void OnInstallComplete(web_app::IwaInstallerResult result);

  void CheckAppInstallState() override;
  const webapps::AppId& GetInstalledWebAppId() override;

  const raw_ref<const KioskIwaData> iwa_data_;
  std::unique_ptr<web_app::IwaInstaller> iwa_installer_;
  base::Value::List iwa_install_log_;

  base::WeakPtrFactory<KioskIwaLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_LAUNCHER_H_
