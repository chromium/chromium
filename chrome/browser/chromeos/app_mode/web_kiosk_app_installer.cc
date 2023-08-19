// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/web_kiosk_app_installer.h"

#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using crosapi::mojom::WebKioskInstallState;

namespace {

// Histogram to log the installed web app is a placeholder.
static constexpr char kWebAppIsPlaceholderUMA[] =
    "Kiosk.AppService.WebApp.IsPlaceholder";

// Histogram to log the web app install result code.
static constexpr char kWebAppInstallResultUMA[] =
    "Kiosk.AppService.WebApp.InstallResult";

web_app::ExternalInstallOptions GetInstallOptions(GURL install_url) {
  web_app::ExternalInstallOptions options(
      install_url, web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kKiosk);
  // When the install URL redirects to another URL a placeholder will be
  // installed. This happens if a web app requires authentication.
  options.install_placeholder = true;
  return options;
}

}  // namespace

namespace chromeos {

WebKioskAppInstaller::WebKioskAppInstaller(Profile& profile,
                                           const GURL& install_url)
    : profile_(profile), install_url_(install_url) {}

WebKioskAppInstaller::~WebKioskAppInstaller() = default;

void WebKioskAppInstaller::GetInstallState(InstallStateCallback callback) {
  // If a web app `install_url` requires authentication, it will be assigned a
  // temporary `app_id` which will be changed to the correct `app_id` once the
  // authentication is done. The only key that is safe to be used as identifier
  // for Kiosk web apps is `install_url`.
  auto app_id = web_app_provider().registrar_unsafe().LookUpAppIdByInstallUrl(
      install_url_);
  if (!app_id || app_id->empty()) {
    std::move(callback).Run(WebKioskInstallState::kNotInstalled, absl::nullopt);
    return;
  }

  // If the installed app is a placeholder (similar to failed installation in
  // the old launcher), try to install again to replace it.
  bool is_placeholder_app =
      web_app_provider().registrar_unsafe().IsPlaceholderApp(
          app_id.value(), web_app::WebAppManagement::Type::kKiosk);
  base::UmaHistogramBoolean(kWebAppIsPlaceholderUMA, is_placeholder_app);
  if (is_placeholder_app) {
    SYSLOG(INFO) << "Placeholder app installed. Trying to reinstall...";
    std::move(callback).Run(WebKioskInstallState::kPlaceholderInstalled,
                            absl::nullopt);
    return;
  }

  std::move(callback).Run(WebKioskInstallState::kInstalled, app_id);
}

void WebKioskAppInstaller::InstallApp(InstallCallback callback) {
  web_app_provider().externally_managed_app_manager().Install(
      GetInstallOptions(install_url_),
      base::BindOnce(&WebKioskAppInstaller::OnExternalInstallCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebKioskAppInstaller::OnExternalInstallCompleted(
    InstallCallback callback,
    const GURL& app_url,
    web_app::ExternallyManagedAppManager::InstallResult result) {
  CHECK_EQ(app_url, install_url_);
  base::UmaHistogramEnumeration(kWebAppInstallResultUMA, result.code);

  if (!webapps::IsSuccess(result.code)) {
    SYSLOG(ERROR) << "Failed to install Kiosk web app, code " << result.code;
    std::move(callback).Run(absl::nullopt);
    return;
  }

  SYSLOG(INFO) << "Successfully installed Kiosk web app.";
  std::move(callback).Run(result.app_id);
}

}  // namespace chromeos
