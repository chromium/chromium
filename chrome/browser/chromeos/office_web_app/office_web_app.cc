// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/office_web_app/office_web_app.h"

#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/install_result_code.h"
#include "url/gurl.h"

namespace chromeos {

namespace {
constexpr char kMicrosoft365WebAppUrl[] =
    "https://www.microsoft365.com/?from=Homescreen";
constexpr char kMicrosoft365FallbackName[] = "Microsoft 365";

void OnOfficeWebAppInstalled(
    Profile* profile,
    base::OnceCallback<void(webapps::InstallResultCode)> callback,
    const GURL& install_url,
    web_app::ExternallyManagedAppManager::InstallResult result) {
  if (webapps::IsSuccess(result.code)) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->SetSupportedLinksPreference(*result.app_id);
  }
  std::move(callback).Run(result.code);
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

  provider->externally_managed_app_manager().InstallNow(
      std::move(options),
      base::BindOnce(&OnOfficeWebAppInstalled, profile, std::move(callback)));
}

}  // namespace chromeos
