// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"
#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/install_result_code.h"

namespace ash {

WebKioskAppServiceLauncher::WebKioskAppServiceLauncher(
    Profile* profile,
    const AccountId& account_id,
    KioskAppLauncher::Delegate* delegate)
    : KioskAppLauncher(delegate), profile_(profile), account_id_(account_id) {}

WebKioskAppServiceLauncher::~WebKioskAppServiceLauncher() = default;

const WebKioskAppData* WebKioskAppServiceLauncher::GetCurrentApp() const {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  return app;
}

void WebKioskAppServiceLauncher::Initialize() {
  DCHECK(!app_service_launcher_);

  app_service_launcher_ = std::make_unique<KioskAppServiceLauncher>(profile_);
  app_service_launcher_->EnsureAppTypeInitialized(
      apps::AppType::kWeb,
      base::BindOnce(&WebKioskAppServiceLauncher::OnWebAppInitializled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::OnWebAppInitializled() {
  web_app_provider_ = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(web_app_provider_) << "WebAppProvider cannot be initialized.";

  // Start observing app update as soon a web app system is ready so that app
  // updates being applied while launching can be handled.
  WebKioskAppManager::Get()->StartObservingAppUpdate(profile_, account_id_);

  // If a web app |install_url| requires authentication, it will be assigned a
  // temporary |app_id| which will be changed to the correct |app_id| once the
  // authentication is done. The only key that is safe to be used as identifier
  // for Kiosk web apps is |install_url|.
  auto app_id = web_app_provider_->registrar_unsafe().LookUpAppIdByInstallUrl(
      GetCurrentApp()->install_url());
  if (!app_id || app_id->empty()) {
    delegate_->InitializeNetwork();
    return;
  }

  // If the installed app is a placeholder (similar to failed installation in
  // the old launcher), try to install again to replace it.
  bool is_placeholder_app =
      web_app_provider_->registrar_unsafe().IsPlaceholderApp(
          app_id.value(), web_app::WebAppManagement::Type::kKiosk);
  base::UmaHistogramBoolean(kWebAppIsPlaceholderUMA, is_placeholder_app);
  if (is_placeholder_app) {
    SYSLOG(INFO) << "Placeholder app installed. Trying to reinstall...";
    delegate_->InitializeNetwork();
    return;
  }

  app_id_ = app_id.value();

  // Don't enforce network status in web Kiosk if the app is already installed.
  delegate_->OnAppPrepared();
}

void WebKioskAppServiceLauncher::ContinueWithNetworkReady() {
  InstallApp();
}

void WebKioskAppServiceLauncher::InstallApp() {
  delegate_->OnAppInstalling();

  web_app::ExternalInstallOptions options(
      GetCurrentApp()->install_url(), web_app::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kKiosk);
  // When the install URL redirects to another URL a placeholder will be
  // installed. This happens if a web app requires authentication.
  options.install_placeholder = true;
  // If there is a placeholder app installed, try to reinstall it.
  options.reinstall_placeholder = true;
  web_app_provider_->externally_managed_app_manager().Install(
      options,
      base::BindOnce(&WebKioskAppServiceLauncher::OnExternalInstallCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppServiceLauncher::OnExternalInstallCompleted(
    const GURL& app_url,
    web_app::ExternallyManagedAppManager::InstallResult result) {
  base::UmaHistogramEnumeration(kWebAppInstallResultUMA, result.code);
  if (!webapps::IsSuccess(result.code)) {
    SYSLOG(ERROR) << "Failed to install Kiosk web app, code " << result.code;
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  DCHECK(result.app_id && !result.app_id->empty());
  SYSLOG(INFO) << "Successfully installed Kiosk web app.";
  app_id_ = result.app_id.value();

  delegate_->OnAppPrepared();
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
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
    return;
  }
  delegate_->OnAppLaunched();
}

void WebKioskAppServiceLauncher::OnAppBecomesVisible() {
  delegate_->OnAppWindowCreated();
}

void WebKioskAppServiceLauncher::RestartLauncher() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  app_service_launcher_.reset();

  Initialize();
}

}  // namespace ash
