// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "components/webapps/browser/install_result_code.h"
#include "url/gurl.h"

using crosapi::mojom::WebKioskInstallState;

namespace {

// Histogram to log the installed web app is a placeholder.
constexpr std::string_view kWebAppIsPlaceholderUMA =
    "Kiosk.AppService.WebApp.IsPlaceholder";

// Histogram to log the web app install result code.
constexpr std::string_view kWebAppInstallResultUMA =
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

web_app::WebAppProvider& WebAppProviderOf(Profile& profile) {
  return CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(&profile));
}

void OnExternalInstallCompleted(
    const GURL& requested_install_url,
    crosapi::mojom::WebKioskInstaller::InstallWebKioskCallback on_done,
    const GURL& installed_url,
    web_app::ExternallyManagedAppManager::InstallResult result) {
  CHECK_EQ(installed_url, requested_install_url);
  base::UmaHistogramEnumeration(kWebAppInstallResultUMA, result.code);

  if (!webapps::IsSuccess(result.code)) {
    SYSLOG(ERROR) << "Failed to install Kiosk web app, code " << result.code;
    return std::move(on_done).Run(std::nullopt);
  }

  SYSLOG(INFO) << "Successfully installed Kiosk web app.";
  std::move(on_done).Run(result.app_id);
}

}  // namespace

namespace chromeos {

KioskWebAppInstallState GetKioskWebAppInstallState(Profile& profile,
                                                   const GURL& install_url) {
  // If a web app `install_url` requires authentication, it will be assigned a
  // temporary `app_id` which will be changed to the correct `app_id` once the
  // authentication is done. The only key that is safe to be used as identifier
  // for Kiosk web apps is `install_url`.
  auto app_id =
      WebAppProviderOf(profile).registrar_unsafe().LookUpAppIdByInstallUrl(
          install_url);
  if (!app_id || app_id->empty()) {
    return std::make_tuple(WebKioskInstallState::kNotInstalled, std::nullopt);
  }

  // If the installed app is a placeholder (similar to failed installation in
  // the old launcher), try to install again to replace it.
  bool is_placeholder_app =
      WebAppProviderOf(profile).registrar_unsafe().IsPlaceholderApp(
          app_id.value(), web_app::WebAppManagement::Type::kKiosk);
  base::UmaHistogramBoolean(kWebAppIsPlaceholderUMA, is_placeholder_app);
  if (is_placeholder_app) {
    SYSLOG(INFO) << "Placeholder app installed. Trying to reinstall...";
    return std::make_tuple(WebKioskInstallState::kPlaceholderInstalled,
                           std::nullopt);
  }

  return std::make_tuple(WebKioskInstallState::kInstalled, app_id);
}

void InstallKioskWebApp(
    Profile& profile,
    const GURL& install_url,
    crosapi::mojom::WebKioskInstaller::InstallWebKioskCallback on_done) {
  WebAppProviderOf(profile).externally_managed_app_manager().Install(
      GetInstallOptions(install_url),
      base::BindOnce(&OnExternalInstallCompleted, install_url,
                     std::move(on_done)));
}

}  // namespace chromeos
