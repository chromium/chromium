// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_launcher.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_web_app_launcher_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
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

  // TODO(crbug.com/361018151): Is launch_url needed for IWA? See
  // app_service_launcher_->SetLaunchUrl();

  // TODO(crbug.com/372848158): add unlimited storage for IWA kiosk. Try
  // profile.GetExtensionSpecialStoragePolicy().AddOriginWithUnlimitedStorage()
}

void KioskIwaLauncher::ContinueWithNetworkReady() {
  KioskWebAppLauncherBase::ContinueWithNetworkReady();

  // TODO(crbug.com/372847595): add IWA updates in kiosk
  // Add KioskIwaManager::Get()->StartObservingAppUpdate()

  InstallIsolatedWebApp();
}

void KioskIwaLauncher::InstallIsolatedWebApp() {
  CHECK(!iwa_installer_);

  // TODO(crbug.com/374069115): Add IWA update channel.
  auto install_options = web_app::IsolatedWebAppExternalInstallOptions::Create(
      iwa_data().update_manifest_url(), iwa_data().web_bundle_id());
  CHECK(install_options.has_value())
      << "Cannot configure IWA installation: " << install_options.error();

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  CHECK(provider);

  iwa_installer_ = web_app::IwaInstallerFactory::Create(
      install_options.value(), web_app::IwaInstaller::InstallSourceType::kKiosk,
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
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  CHECK(provider);
  auto& web_app_registrar = provider->registrar_unsafe();

  // TODO(crbug.com/372848695): is "kKioskWebAppOfflineEnabled" needed for IWA?
  if (!web_app_registrar.IsInstalled(iwa_data().app_id())) {
    delegate_->InitializeNetwork();
    return;
  }

  NotifyAppPrepared();
}

const webapps::AppId& KioskIwaLauncher::GetInstalledWebAppId() {
  return iwa_data().app_id();
}

}  // namespace ash
