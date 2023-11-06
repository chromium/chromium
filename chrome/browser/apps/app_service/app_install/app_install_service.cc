// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace apps {

namespace {

void InstallWebApp(Profile& profile, const GURL& install_url) {
  const GURL& origin_url = install_url;
  constexpr bool is_renderer_initiated = false;

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(&profile);
  if (provider) {
    provider->scheduler().ScheduleNavigateAndTriggerInstallDialog(
        install_url, origin_url, is_renderer_initiated, base::DoNothing());
    return;
  }

  // No WebAppProvider means web apps are hosted in Lacros (because this code
  // runs in Ash).
  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  if (!web_app_provider_bridge) {
    // Bridge not ready.
    return;
  }
  web_app_provider_bridge->ScheduleNavigateAndTriggerInstallDialog(
      install_url, origin_url, is_renderer_initiated);
}

}  // namespace

AppInstallService::AppInstallService(Profile& profile)
    : profile_(profile), device_info_manager_(&*profile_) {}
AppInstallService::~AppInstallService() = default;

void AppInstallService::InstallApp(PackageId package_id) {
  // TODO(b/303350800): Generalize to work with all app types.
  CHECK_EQ(package_id.app_type(), AppType::kWeb);

  device_info_manager_.GetDeviceInfo(
      base::BindOnce(&AppInstallService::InstallAppWithDeviceInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(package_id)));
}

void AppInstallService::InstallAppWithDeviceInfo(PackageId package_id,
                                                 DeviceInfo device_info) {
  connector_.GetAppInstallInfo(
      package_id, std::move(device_info), *profile_->GetURLLoaderFactory(),
      base::BindOnce(&AppInstallService::InstallFromFetchedData,
                     weak_ptr_factory_.GetWeakPtr(), package_id));
}

void AppInstallService::InstallFromFetchedData(
    const PackageId& expected_package_id,
    absl::optional<AppInstallData> data) {
  if (!data || data->package_id != expected_package_id) {
    return;
  }

  if (const auto* web_app_data =
          absl::get_if<WebAppInstallData>(&data->app_type_data)) {
    // TODO(crbug.com/1488697): Show an install dialog.
    // TODO(b/303350800): Delegate to a generic AppPublisher method instead of
    // harboring app type specific logic here.
    InstallWebApp(*profile_, web_app_data->document_url);
  }
}

}  // namespace apps
