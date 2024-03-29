// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"

#include "ash/constants/ash_features.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/ash/borealis/borealis_game_install_flow.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
// TODO(crbug.com/1488697): Remove circular dependency.
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"  // nogncheck
#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"  // nogncheck
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/native_widget_types.h"

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
  kInstallDialogNotAccepted = 8,
  kAppTypeInstallFailed = 9,
  kMaxValue = kAppTypeInstallFailed,
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

// TODO(b/330414871): AppInstallService shouldn't know about publisher specific
// logic, remove the generation of app_ids.
std::string GetAppId(const PackageId& package_id) {
  CHECK_EQ(package_id.app_type(), AppType::kWeb);
  // data->package_id.identifier() is the manifest ID for web apps.
  return web_app::GenerateAppIdFromManifestId(GURL(package_id.identifier()));
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

void AppInstallServiceAsh::InstallApp(
    AppInstallSurface surface,
    PackageId package_id,
    std::optional<gfx::NativeWindow> anchor_window,
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
  CHECK(package_id.app_type() == AppType::kWeb ||
        package_id.app_type() == AppType::kBorealis);

  // Observe for `anchor_window` being destroyed during async work.
  std::unique_ptr<views::NativeWindowTracker> anchor_window_tracker;
  if (anchor_window) {
    anchor_window_tracker = views::NativeWindowTracker::Create(*anchor_window);
  }

  FetchAppInstallData(
      package_id,
      base::BindOnce(&AppInstallServiceAsh::ShowDialogAndInstall,
                     weak_ptr_factory_.GetWeakPtr(), surface, package_id,
                     anchor_window, std::move(anchor_window_tracker),
                     std::move(callback)));
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
    std::optional<gfx::NativeWindow> anchor_window,
    std::unique_ptr<views::NativeWindowTracker> anchor_window_tracker,
    base::OnceClosure callback,
    std::optional<AppInstallData> data) {
  std::optional<AppInstallResult> result =
      [&]() -> std::optional<AppInstallResult> {
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
                  chromeos::features::kCrosWebAppInstallDialog) ||
              ash::app_install::AppInstallPageHandler::
                  GetAutoAcceptForTesting()) {
            ash::app_install::mojom::DialogArgsPtr args =
                ash::app_install::mojom::DialogArgs::New();
            args->url = web_app_data->document_url;
            args->name = data->name;
            args->description = data->description;
            args->icon_url = data->icon ? data->icon->url : GURL::EmptyGURL();
            for (auto& screenshot : data->screenshots) {
              auto dialog_screenshot =
                  ash::app_install::mojom::Screenshot::New();
              dialog_screenshot->url = screenshot.url;
              dialog_screenshot->size = gfx::Size(screenshot.width_in_pixels,
                                                  screenshot.height_in_pixels);
              args->screenshots.push_back(std::move(dialog_screenshot));
            }

            webapps::AppId expected_app_id = GetAppId(data->package_id);
            base::WeakPtr<ash::app_install::AppInstallDialog> dialog =
                ash::app_install::AppInstallDialog::CreateDialog();
            gfx::NativeWindow parent =
                anchor_window.has_value() &&
                        !anchor_window_tracker->WasNativeWindowDestroyed()
                    ? anchor_window.value()
                    : nullptr;
            dialog->Show(
                parent, std::move(args), expected_app_id,
                base::BindOnce(&AppInstallServiceAsh::InstallIfDialogAccepted,
                               weak_ptr_factory_.GetWeakPtr(), surface,
                               expected_package_id, std::move(data).value(),
                               dialog, std::move(callback)));
            return std::nullopt;
          }
          // TODO(b/303350800): Delegate to a generic AppPublisher method
          // instead of harboring app type specific logic here.
          return InstallWebAppWithBrowserInstallDialog(
              *profile_, web_app_data->document_url);
        }
        return AppInstallResult::kAppDataCorrupted;
      case AppType::kBorealis:
        if (!base::FeatureList::IsEnabled(
                ash::features::kAppInstallServiceUriBorealis)) {
          return AppInstallResult::kAppProviderNotAvailable;
        }

        // Parse the Steam Game ID from the PackageId.
        uint64_t steam_game_id;
        if (!base::StringToUint64(expected_package_id.identifier(),
                                  &steam_game_id)) {
          return AppInstallResult::kAppDataCorrupted;
        }

        borealis::UserRequestedSteamGameInstall(&*profile_, steam_game_id);

        // We've now launched the Borealis installer or the Steam Store
        // website. We don't yet know whether that flow will result in a
        // successfully installed game.
        return AppInstallResult::kUnknown;
      case AppType::kArc:
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

  if (result.has_value()) {
    RecordInstallResult(surface, result.value());
    std::move(callback).Run();
  }
}

void AppInstallServiceAsh::InstallIfDialogAccepted(
    AppInstallSurface surface,
    PackageId expected_package_id,
    AppInstallData data,
    base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
    base::OnceClosure callback,
    bool dialog_accepted) {
  if (!dialog_accepted) {
    RecordInstallResult(surface, AppInstallResult::kInstallDialogNotAccepted);
    std::move(callback).Run();
    return;
  }
  web_app_installer_.InstallApp(
      surface, std::move(data),
      base::BindOnce(&AppInstallServiceAsh::ProcessInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), surface,
                     expected_package_id, dialog, std::move(callback)));
}

void AppInstallServiceAsh::ProcessInstallResult(
    AppInstallSurface surface,
    PackageId expected_package_id,
    base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
    base::OnceClosure callback,
    bool install_success) {
  if (dialog) {
    std::string app_id = GetAppId(expected_package_id);
    dialog->SetInstallComplete(install_success ? &app_id : nullptr);
  }
  RecordInstallResult(surface, install_success
                                   ? AppInstallResult::kSuccess
                                   : AppInstallResult::kAppTypeInstallFailed);
  std::move(callback).Run();
}

}  // namespace apps
