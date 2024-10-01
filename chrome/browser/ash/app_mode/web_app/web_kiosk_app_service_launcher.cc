// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/crosapi/web_kiosk_service_ash.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"
#include "url/origin.h"

using crosapi::mojom::WebKioskInstaller;
using crosapi::mojom::WebKioskInstallState;

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

  app_service_launcher_ =
      std::make_unique<chromeos::KioskAppServiceLauncher>(profile_);
  app_service_launcher_->EnsureAppTypeInitialized(
      apps::AppType::kWeb,
      base::BindOnce(&WebKioskAppServiceLauncher::OnWebAppInitialized,
                     weak_ptr_factory_.GetWeakPtr()));

  // By default the app service will try to launch the start_url as defined by
  // the web app's manifest. This is generally not what we want, so we need to
  // set the complete url as override url.
  app_service_launcher_->SetLaunchUrl(GetCurrentApp()->install_url());

  profile_->GetExtensionSpecialStoragePolicy()->AddOriginWithUnlimitedStorage(
      url::Origin::Create(GetCurrentApp()->install_url()));
}

void WebKioskAppServiceLauncher::OnWebAppInitialized() {
  GetInstallState(
      GetCurrentApp()->install_url(),
      base::BindOnce(&WebKioskAppServiceLauncher::CheckWhetherNetworkIsRequired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::GetInstallState(
    const GURL& install_url,
    WebKioskInstaller::GetWebKioskInstallStateCallback callback) {
  auto [state, app_id] = chromeos::GetKioskWebAppInstallState(
      CHECK_DEREF(profile_.get()), install_url);
  std::move(callback).Run(state, std::move(app_id));
}

void WebKioskAppServiceLauncher::CheckWhetherNetworkIsRequired(
    WebKioskInstallState state,
    const std::optional<webapps::AppId>& id) {
  if (state != WebKioskInstallState::kInstalled ||
      !profile_->GetPrefs()->GetBoolean(::prefs::kKioskWebAppOfflineEnabled)) {
    delegate_->InitializeNetwork();
    return;
  }

  NotifyAppPrepared(id);
}

void WebKioskAppServiceLauncher::ContinueWithNetworkReady() {
  observers_.NotifyAppInstalling();

  // Start observing app update as soon a web app system is ready so that app
  // updates being applied while launching can be handled.
  WebKioskAppManager::Get()->StartObservingAppUpdate(profile_, account_id_);

  chromeos::InstallKioskWebApp(
      CHECK_DEREF(profile_.get()), GetCurrentApp()->install_url(),
      base::BindOnce(&WebKioskAppServiceLauncher::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::OnInstallComplete(
    const std::optional<webapps::AppId>& app_id) {
  if (app_id.has_value()) {
    NotifyAppPrepared(app_id);
    return;
  }

  observers_.NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
}

void WebKioskAppServiceLauncher::NotifyAppPrepared(
    const std::optional<webapps::AppId>& id) {
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
  observers_.NotifyAppWindowCreated(
      web_app::GenerateApplicationNameFromAppId(app_id_));
}

}  // namespace ash
