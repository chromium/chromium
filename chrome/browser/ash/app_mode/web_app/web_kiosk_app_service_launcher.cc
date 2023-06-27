// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"
#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/chromeos/app_mode/web_kiosk_app_installer.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

using chromeos::WebKioskAppInstaller;

namespace ash {

WebKioskAppServiceLauncher::WebKioskAppServiceLauncher(
    Profile* profile,
    const AccountId& account_id,
    KioskAppLauncher::NetworkDelegate* network_delegate)
    : KioskAppLauncher(network_delegate),
      profile_(profile),
      account_id_(account_id) {}

WebKioskAppServiceLauncher::~WebKioskAppServiceLauncher() = default;

const WebKioskAppData* WebKioskAppServiceLauncher::GetCurrentApp() const {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  return app;
}

void WebKioskAppServiceLauncher::AddObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void WebKioskAppServiceLauncher::RemoveObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebKioskAppServiceLauncher::Initialize() {
  DCHECK(!app_service_launcher_);

  app_service_launcher_ = std::make_unique<KioskAppServiceLauncher>(profile_);
  app_service_launcher_->EnsureAppTypeInitialized(
      apps::AppType::kWeb,
      base::BindOnce(&WebKioskAppServiceLauncher::OnWebAppInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
  profile_->GetExtensionSpecialStoragePolicy()->AddOriginWithUnlimitedStorage(
      url::Origin::Create(GetCurrentApp()->install_url()));
}

void WebKioskAppServiceLauncher::OnWebAppInitialized() {
  // Start observing app update as soon a web app system is ready so that app
  // updates being applied while launching can be handled.
  WebKioskAppManager::Get()->StartObservingAppUpdate(profile_, account_id_);

  app_installer_ = std::make_unique<chromeos::WebKioskAppInstaller>(
      CHECK_DEREF(profile_.get()), GetCurrentApp()->install_url());

  app_installer_->GetInstallState(
      base::BindOnce(&WebKioskAppServiceLauncher::CheckWhetherNetworkIsRequired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::CheckWhetherNetworkIsRequired(
    chromeos::WebKioskAppInstaller::InstallState state,
    const absl::optional<web_app::AppId>& id) {
  if (state == WebKioskAppInstaller::InstallState::kInstalled) {
    NotifyAppPrepared(id);
    return;
  }

  delegate_->InitializeNetwork();
}

void WebKioskAppServiceLauncher::ContinueWithNetworkReady() {
  observers_.NotifyAppInstalling();
  app_installer_->InstallApp(
      base::BindOnce(&WebKioskAppServiceLauncher::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::OnInstallComplete(
    const absl::optional<web_app::AppId>& app_id) {
  if (app_id.has_value()) {
    NotifyAppPrepared(app_id);
    return;
  }

  observers_.NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
}

void WebKioskAppServiceLauncher::NotifyAppPrepared(
    const absl::optional<web_app::AppId>& id) {
  CHECK(id.has_value());
  app_id_ = id.value();
  observers_.NotifyAppPrepared();
}

void WebKioskAppServiceLauncher::LaunchApp() {
  DCHECK(app_service_launcher_);
  app_service_launcher_->CheckAndMaybeLaunchApp(
      app_id_,
      base::BindOnce(&WebKioskAppServiceLauncher::OnAppLaunched,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebKioskAppServiceLauncher::OnAppBecomesVisible,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::OnAppLaunched(bool success) {
  if (!success) {
    observers_.NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
    return;
  }
  observers_.NotifyAppLaunched();
}

void WebKioskAppServiceLauncher::OnAppBecomesVisible() {
  // TODO(b/242023891): Make sure we send a absl::nullopt when Lacros is enabled
  observers_.NotifyAppWindowCreated(
      web_app::GenerateApplicationNameFromAppId(app_id_));
}

}  // namespace ash
