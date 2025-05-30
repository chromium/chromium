// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_service_launcher.h"

#include <optional>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_web_app_launcher_base.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"
#include "url/origin.h"

namespace ash {

KioskWebAppServiceLauncher::KioskWebAppServiceLauncher(
    Profile* profile,
    const AccountId& account_id,
    KioskAppLauncher::NetworkDelegate* network_delegate)
    : KioskWebAppLauncherBase(profile, account_id, network_delegate) {}

KioskWebAppServiceLauncher::~KioskWebAppServiceLauncher() = default;

const KioskWebAppData* KioskWebAppServiceLauncher::GetCurrentApp() const {
  const KioskWebAppData* app =
      KioskWebAppManager::Get()->GetAppByAccountId(account_id());
  DCHECK(app);
  return app;
}

void KioskWebAppServiceLauncher::Initialize() {
  KioskWebAppLauncherBase::Initialize();

  // By default the app service will try to launch the start_url as defined by
  // the web app's manifest. This is generally not what we want, so we need to
  // set the complete url as override url.
  app_service_launcher().SetLaunchUrl(GetCurrentApp()->install_url());

  profile()->GetExtensionSpecialStoragePolicy()->AddOriginWithUnlimitedStorage(
      url::Origin::Create(GetCurrentApp()->install_url()));
}

void KioskWebAppServiceLauncher::ContinueWithNetworkReady() {
  KioskWebAppLauncherBase::ContinueWithNetworkReady();

  // Start observing app update as soon a web app system is ready so that app
  // updates being applied while launching can be handled.
  KioskWebAppManager::Get()->StartObservingAppUpdate(profile(), account_id());

  chromeos::InstallKioskWebApp(
      CHECK_DEREF(profile()), GetCurrentApp()->install_url(),
      base::BindOnce(&KioskWebAppServiceLauncher::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskWebAppServiceLauncher::OnInstallComplete(
    const std::optional<webapps::AppId>& app_id) {
  if (app_id.has_value()) {
    NotifyAppPrepared(app_id);
    return;
  }

  NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
}

void KioskWebAppServiceLauncher::NotifyAppPrepared(
    const std::optional<webapps::AppId>& app_id) {
  CHECK(app_id.has_value());
  installed_app_id_ = app_id;
  KioskWebAppLauncherBase::NotifyAppPrepared();
}

void KioskWebAppServiceLauncher::CheckAppInstallState() {
  auto [state, app_id] = chromeos::GetKioskWebAppInstallState(
      CHECK_DEREF(profile()), GetCurrentApp()->install_url());

  if (state != chromeos::WebKioskInstallState::kInstalled ||
      !profile()->GetPrefs()->GetBoolean(::prefs::kKioskWebAppOfflineEnabled)) {
    delegate_->InitializeNetwork();
    return;
  }

  NotifyAppPrepared(app_id);
}

const webapps::AppId& KioskWebAppServiceLauncher::GetInstalledWebAppId() {
  CHECK(installed_app_id_.has_value());
  return installed_app_id_.value();
}

}  // namespace ash
