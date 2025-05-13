// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_launcher.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_web_app_launcher_base.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"  // nogncheck crbug.com/386960384
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

KioskIwaLauncher::KioskIwaLauncher(
    Profile* profile,
    const AccountId& account_id,
    KioskAppLauncher::NetworkDelegate* network_delegate)
    : KioskWebAppLauncherBase(profile, account_id, network_delegate),
      iwa_data_(CHECK_DEREF(KioskIwaManager::Get()->GetApp(account_id))) {}

KioskIwaLauncher::~KioskIwaLauncher() = default;

void KioskIwaLauncher::Initialize() {
  KioskWebAppLauncherBase::Initialize();
  KioskIwaManager::Get()->StartObservingAppUpdate(profile(), account_id());
  CHECK_DEREF(profile()->GetExtensionSpecialStoragePolicy())
      .AddOriginWithUnlimitedStorage(iwa_data().origin());
}

void KioskIwaLauncher::ContinueWithNetworkReady() {
  KioskWebAppLauncherBase::ContinueWithNetworkReady();

  if (IsIsolatedWebAppInstalled()) {
    NotifyAppPrepared();
  } else {
    InstallIsolatedWebApp();
  }
}

bool KioskIwaLauncher::IsIsolatedWebAppInstalled() const {
  const web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  const web_app::WebAppRegistrar& web_app_registrar =
      CHECK_DEREF(provider).registrar_unsafe();
  return web_app_registrar.GetInstallState(iwa_data().app_id()).has_value();
}

void KioskIwaLauncher::InstallIsolatedWebApp() {
  CHECK(!iwa_installer_);

  ASSIGN_OR_RETURN(
      auto install_options,
      web_app::IsolatedWebAppExternalInstallOptions::Create(
          iwa_data().web_bundle_id(), iwa_data().update_manifest_url(),
          iwa_data().update_channel(), iwa_data().pinned_version(),
          iwa_data().allow_downgrades()),
      [this](const std::string& error) {
        LOG(ERROR) << "Cannot configure IWA installation: " << error;
        NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
      });

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  CHECK(provider);

  iwa_installer_ = web_app::IwaInstallerFactory::Create(
      install_options, web_app::IwaInstaller::InstallSourceType::kKiosk,
      profile()->GetURLLoaderFactory(), iwa_install_log_, provider,
      base::BindOnce(&KioskIwaLauncher::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  iwa_installer_->Start();
}

void KioskIwaLauncher::OnInstallComplete(web_app::IwaInstallerResult result) {
  if (result.type() == web_app::IwaInstallerResult::Type::kSuccess) {
    NotifyAppPrepared();
  } else {
    NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
  }
}

void KioskIwaLauncher::CheckAppInstallState() {
  const bool offlineLaunchAllowed =
      profile()->GetPrefs()->GetBoolean(::prefs::kKioskWebAppOfflineEnabled);
  if (IsIsolatedWebAppInstalled() && offlineLaunchAllowed) {
    NotifyAppPrepared();
    return;
  }

  delegate_->InitializeNetwork();
}

const webapps::AppId& KioskIwaLauncher::GetInstalledWebAppId() {
  return iwa_data().app_id();
}

}  // namespace ash
