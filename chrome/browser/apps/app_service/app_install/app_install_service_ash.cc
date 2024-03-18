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
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
// TODO(crbug.com/1488697): Remove circular dependency.
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"  // nogncheck
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace apps {

namespace {

// These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Additions to this enum must be added to the corresponding enum XML in:
// tools/metrics/histograms/metadata/apps/enums.xml
enum class AppInstallResult {
  kUnknown = 0,
  kSuccess = 1,
  kAlmanacFetchFailed = 2,
  kAppDataCorrupted = 3,
  kAppProviderNotAvailable = 4,
  kAppTypeNotSupported = 5,
  kInstallParametersInvalid = 6,
  kAppAlreadyInstalled = 7,
  kMaxValue = kAppAlreadyInstalled,
};

AppInstallResult InstallWebAppWithBrowserInstallDialog(
    Profile& profile,
    const GURL& install_url) {
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

// Gets the first icon larger than `kIconSize` from `icons` and returns
// the url. If none exist, returns the url of the largest icon. Returns empty
// GURL if vector is empty.
// TODO(crbug.com/1488697): This function assumes icons is sorted, which it may
// not be. Icon purpose also needs to be considered.
const GURL& GetIconUrl(const std::vector<AppInstallIcon>& icons) {
  if (icons.empty()) {
    return GURL::EmptyGURL();
  }

  const GURL* icon_url = &GURL::EmptyGURL();
  for (const auto& icon : icons) {
    icon_url = &icon.url;
    if (icon.width_in_pixels > ash::app_install::kIconSize) {
      break;
    }
  }

  return *icon_url;
}

void RecordInstallResult(AppInstallSurface surface, AppInstallResult result) {
  base::UmaHistogramEnumeration("Apps.AppInstallService.AppInstallResult",
                                result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Apps.AppInstallService.AppInstallResult.",
                    base::ToString(surface)}),
      result);
}

}  // namespace

base::OnceCallback<void(PackageId)>&
AppInstallServiceAsh::InstallAppCallbackForTesting() {
  static base::NoDestructor<base::OnceCallback<void(PackageId)>> callback;
  return *callback;
}

AppInstallServiceAsh::AppInstallServiceAsh(Profile& profile)
    : profile_(profile),
      device_info_manager_(&*profile_),
      arc_app_installer_(&*profile_),
      web_app_installer_(&*profile_) {}

AppInstallServiceAsh::~AppInstallServiceAsh() = default;

void AppInstallServiceAsh::InstallApp(AppInstallSurface surface,
                                      PackageId package_id,
                                      base::OnceClosure callback) {
  if (InstallAppCallbackForTesting()) {
    std::move(InstallAppCallbackForTesting()).Run(package_id);
  }

  if (MaybeLaunchApp(package_id)) {
    RecordInstallResult(surface, AppInstallResult::kAppAlreadyInstalled);
    std::move(callback).Run();
    return;
  }

  // TODO(b/303350800): Generalize to work with all app types.
  CHECK_EQ(package_id.app_type(), AppType::kWeb);

  FetchAppInstallData(
      package_id, base::BindOnce(&AppInstallServiceAsh::ShowDialogAndInstall,
                                 weak_ptr_factory_.GetWeakPtr(), surface,
                                 package_id, std::move(callback)));
}

void AppInstallServiceAsh::InstallAppHeadless(
    AppInstallSurface surface,
    PackageId package_id,
    base::OnceCallback<void(bool success)> callback) {
  FetchAppInstallData(
      package_id, base::BindOnce(&AppInstallServiceAsh::PerformInstallHeadless,
                                 weak_ptr_factory_.GetWeakPtr(), surface,
                                 package_id, std::move(callback)));
}

void AppInstallServiceAsh::InstallAppHeadless(
    AppInstallSurface surface,
    AppInstallData data,
    base::OnceCallback<void(bool success)> callback) {
  PerformInstallHeadless(surface, data.package_id, std::move(callback), data);
}

bool AppInstallServiceAsh::MaybeLaunchApp(const PackageId& package_id) {
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(&*profile_);
  if (!proxy) {
    return false;
  }
  std::optional<std::string> app_id;
  proxy->AppRegistryCache().ForEachApp(
      [&app_id, package_id](const apps::AppUpdate& update) {
        if (!app_id.has_value() && apps_util::IsInstalled(update.Readiness()) &&
            update.InstallerPackageId() == package_id) {
          app_id = update.AppId();
        }
      });

  if (!app_id.has_value()) {
    return false;
  }

  proxy->Launch(app_id.value(), /*event_flags=*/0,
                LaunchSource::kFromInstaller);
  return true;
}

void AppInstallServiceAsh::FetchAppInstallData(
    PackageId package_id,
    base::OnceCallback<void(std::optional<AppInstallData>)> data_callback) {
  device_info_manager_.GetDeviceInfo(
      base::BindOnce(&AppInstallServiceAsh::FetchAppInstallDataWithDeviceInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(package_id),
                     std::move(data_callback)));
}

void AppInstallServiceAsh::FetchAppInstallDataWithDeviceInfo(
    PackageId package_id,
    base::OnceCallback<void(std::optional<AppInstallData>)> data_callback,
    DeviceInfo device_info) {
  connector_.GetAppInstallInfo(package_id, std::move(device_info),
                               *profile_->GetURLLoaderFactory(),
                               std::move(data_callback));
}

void AppInstallServiceAsh::PerformInstallHeadless(
    AppInstallSurface surface,
    PackageId expected_package_id,
    base::OnceCallback<void(bool success)> callback,
    std::optional<AppInstallData> data) {
  // TODO(b/327535848): Record metrics for headless installs.
  if (!data) {
    std::move(callback).Run(false);
    return;
  }

  if (absl::holds_alternative<AndroidAppInstallData>(data->app_type_data)) {
    arc_app_installer_.InstallApp(surface, std::move(*data),
                                  std::move(callback));
  } else if (absl::holds_alternative<WebAppInstallData>(data->app_type_data)) {
    web_app_installer_.InstallApp(surface, std::move(*data),
                                  std::move(callback));
  } else {
    LOG(ERROR) << "Unsupported AppInstallData type";
    std::move(callback).Run(false);
  }
}

void AppInstallServiceAsh::ShowDialogAndInstall(
    AppInstallSurface surface,
    PackageId expected_package_id,
    base::OnceClosure callback,
    std::optional<AppInstallData> data) {
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
          if (base::FeatureList::IsEnabled(
                  chromeos::features::kCrosWebAppInstallDialog)) {
            ash::app_install::mojom::DialogArgsPtr args =
                ash::app_install::mojom::DialogArgs::New();
            args->url = web_app_data->document_url;
            args->name = data->name;
            args->description = data->description;
            args->icon_url = GetIconUrl(data->icons);

            base::WeakPtr<ash::app_install::AppInstallDialog> dialog =
                ash::app_install::AppInstallDialog::CreateDialog();
            // TODO(crbug.com/1488697): Install the app.
            dialog->Show(
                nullptr, std::move(args),
                web_app::GenerateAppIdFromManifestId(
                    // expected_package_id.identifier() is the manifest ID for
                    // web apps.
                    GURL(expected_package_id.identifier())),
                base::BindOnce(
                    [](base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
                       bool dialog_accepted) {
                      dialog->SetInstallComplete(nullptr);
                    },
                    dialog));
            return AppInstallResult::kUnknown;
          }
          // TODO(b/303350800): Delegate to a generic AppPublisher method
          // instead of harboring app type specific logic here.
          return InstallWebAppWithBrowserInstallDialog(
              *profile_, web_app_data->document_url);
        }
        return AppInstallResult::kAppDataCorrupted;
      case AppType::kArc:
      case AppType::kBorealis:
      case AppType::kBruschetta:
      case AppType::kBuiltIn:
      case AppType::kChromeApp:
      case AppType::kCrostini:
      case AppType::kExtension:
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

  RecordInstallResult(surface, result);

  std::move(callback).Run();
}

}  // namespace apps
