// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AppInstallResult {
  kUnknown = 0,
  kSuccess = 1,
  kAlmanacFetchFailed = 2,
  kAppDataCorrupted = 3,
  kAppProviderNotAvailable = 4,
  kAppTypeNotSupported = 5,
  kInstallParametersInvalid = 6,
  kMaxValue = kInstallParametersInvalid,
};

AppInstallResult InstallWebApp(Profile& profile, const GURL& install_url) {
  const GURL& origin_url = install_url;
  constexpr bool is_renderer_initiated = false;

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(&profile);
  if (provider) {
    provider->scheduler().ScheduleNavigateAndTriggerInstallDialog(
        install_url, origin_url, is_renderer_initiated, base::DoNothing());
    return AppInstallResult::kUnknown;
  }

  // No WebAppProvider means web apps are hosted in Lacros (because this code
  // runs in Ash).
  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  if (!web_app_provider_bridge) {
    return AppInstallResult::kAppProviderNotAvailable;
  }
  web_app_provider_bridge->ScheduleNavigateAndTriggerInstallDialog(
      install_url, origin_url, is_renderer_initiated);
  return AppInstallResult::kUnknown;
}

}  // namespace

AppInstallServiceAsh::AppInstallServiceAsh(Profile& profile)
    : profile_(profile), device_info_manager_(&*profile_) {}
AppInstallServiceAsh::~AppInstallServiceAsh() = default;

void AppInstallServiceAsh::InstallApp(AppInstallSurface surface,
                                      PackageId package_id,
                                      base::OnceClosure callback) {
  // TODO(b/303350800): Generalize to work with all app types.
  CHECK_EQ(package_id.app_type(), AppType::kWeb);

  device_info_manager_.GetDeviceInfo(
      base::BindOnce(&AppInstallServiceAsh::InstallAppWithDeviceInfo,
                     weak_ptr_factory_.GetWeakPtr(), surface,
                     std::move(package_id), std::move(callback)));
}

void AppInstallServiceAsh::InstallAppWithDeviceInfo(AppInstallSurface surface,
                                                    PackageId package_id,
                                                    base::OnceClosure callback,
                                                    DeviceInfo device_info) {
  connector_.GetAppInstallInfo(
      package_id, std::move(device_info), *profile_->GetURLLoaderFactory(),
      base::BindOnce(&AppInstallServiceAsh::InstallFromFetchedData,
                     weak_ptr_factory_.GetWeakPtr(), surface, package_id,
                     std::move(callback)));
}

void AppInstallServiceAsh::InstallFromFetchedData(
    AppInstallSurface surface,
    PackageId expected_package_id,
    base::OnceClosure callback,
    absl::optional<AppInstallData> data) {
  AppInstallResult result = [&] {
    if (!data) {
      return AppInstallResult::kAlmanacFetchFailed;
    }

    if (data->package_id != expected_package_id) {
      return AppInstallResult::kAppDataCorrupted;
    }

    switch (expected_package_id.app_type()) {
      case AppType::kWeb:
        if (const auto* web_app_data =
                absl::get_if<WebAppInstallData>(&data->app_type_data)) {
          // TODO(crbug.com/1488697): Show an install dialog.
          // TODO(b/303350800): Delegate to a generic AppPublisher method
          // instead of harboring app type specific logic here.
          return InstallWebApp(*profile_, web_app_data->document_url);
        }
        return AppInstallResult::kAppDataCorrupted;
      case AppType::kArc:
      case AppType::kBorealis:
      case AppType::kBruschetta:
      case AppType::kBuiltIn:
      case AppType::kChromeApp:
      case AppType::kCrostini:
      case AppType::kExtension:
      case AppType::kMacOs:
      case AppType::kPluginVm:
      case AppType::kRemote:
      case AppType::kStandaloneBrowser:
      case AppType::kStandaloneBrowserChromeApp:
      case AppType::kStandaloneBrowserExtension:
      case AppType::kSystemWeb:
      case AppType::kUnknown:
        return AppInstallResult::kAppTypeNotSupported;
    }
  }();

  base::UmaHistogramEnumeration("Apps.AppInstallService.AppInstallResult",
                                result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Apps.AppInstallService.AppInstallResult.",
                    base::ToString(surface)}),
      result);

  std::move(callback).Run();
}

}  // namespace apps
