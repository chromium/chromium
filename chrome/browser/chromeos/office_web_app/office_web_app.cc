// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/office_web_app/office_web_app.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/office_web_app_resources.h"
#include "components/webapps/browser/install_result_code.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace chromeos {

const char kMicrosoft365WebAppUrl[] =
    "https://www.microsoft365.com/?from=Homescreen";

namespace {
constexpr char kMicrosoft365FallbackName[] = "Microsoft 365";

void OnOfficeWebAppInstalledOffline(
    Profile* profile,
    base::OnceCallback<void(webapps::InstallResultCode)> callback,
    const GURL& install_url,
    web_app::ExternallyManagedAppManager::InstallResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "FileBrowser.OfficeFiles.Setup.OfficeWebAppOfflineInstallation",
      result.code);
  if (webapps::IsSuccess(result.code)) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->SetSupportedLinksPreference(*result.app_id);
  } else {
    LOG(ERROR) << "Office web app offline installation failure: "
               << result.code;
  }

  std::move(callback).Run(result.code);
}

void InstallMicrosoft365Offline(
    Profile* profile,
    base::OnceCallback<void(webapps::InstallResultCode)> callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);

  // Manually install using the manifest at
  // https://www.microsoft365.com/webmanifest.json.
  web_app::ExternalInstallOptions options(
      GURL(kMicrosoft365WebAppUrl),
      web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kInternalMicrosoft365Setup);
  options.fallback_app_name = kMicrosoft365FallbackName;
  options.add_to_quick_launch_bar = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url = GURL(kMicrosoft365WebAppUrl);
    webapps::ManifestId manifest_id =
        web_app::GenerateManifestIdFromStartUrlOnly(start_url);
    auto info =
        std::make_unique<web_app::WebAppInstallInfo>(manifest_id, start_url);
    info->title =
        l10n_util::GetStringUTF16(IDS_OFFICE_FILE_HANDLER_APP_MICROSOFT);
    info->scope = GURL("/");
    info->display_mode = web_app::DisplayMode::kStandalone;

    auto image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_OFFICE_WEB_APP_ICONS_OFFICE_192_PNG);
    info->icon_bitmaps.any[192] = image.AsBitmap();
    info->background_color = 0xFFD53A00;
    info->theme_color = 0xFFD53A00;
    return info;
  });

  provider->externally_managed_app_manager().InstallNow(
      std::move(options), base::BindOnce(&OnOfficeWebAppInstalledOffline,
                                         profile, std::move(callback)));
}

void OnOfficeWebAppInstalled(
    Profile* profile,
    base::OnceCallback<void(webapps::InstallResultCode)> callback,
    const GURL& install_url,
    web_app::ExternallyManagedAppManager::InstallResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "FileBrowser.OfficeFiles.Setup.OfficeWebAppInstallation", result.code);
  if (webapps::IsSuccess(result.code)) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->SetSupportedLinksPreference(*result.app_id);
    std::move(callback).Run(result.code);
    return;
  }

  // Fallback to an offline install.
  LOG(ERROR) << "Office web app installation failure: " << result.code
             << " attempting offline installation";
  InstallMicrosoft365Offline(profile, std::move(callback));
}

}  // namespace

void InstallMicrosoft365(
    Profile* profile,
    base::OnceCallback<void(webapps::InstallResultCode)> callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);

  web_app::ExternalInstallOptions options(
      GURL(kMicrosoft365WebAppUrl),
      web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kInternalMicrosoft365Setup);
  options.fallback_app_name = kMicrosoft365FallbackName;
  options.add_to_quick_launch_bar = false;

  // Attempt an online install first.
  provider->externally_managed_app_manager().InstallNow(
      std::move(options),
      base::BindOnce(&OnOfficeWebAppInstalled, profile, std::move(callback)));
}

}  // namespace chromeos
