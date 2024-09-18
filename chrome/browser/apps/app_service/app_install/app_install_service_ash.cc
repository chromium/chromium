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
#include "chrome/browser/apps/app_service/app_install/app_install_discovery_metrics.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/borealis/borealis_game_install_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
// TODO(crbug.com/40283709): Remove circular dependency.
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"  // nogncheck
#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user_manager.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/native_widget_types.h"

namespace apps {

namespace {

std::optional<QueryError::Type> VerifyAppInstallData(
    const base::expected<AppInstallData, QueryError>& data,
    const PackageId& expected_package_id) {
  if (data.has_value()) {
    if (data->package_id != expected_package_id) {
      return QueryError::kBadResponse;
    }
    if (!data->IsValidForInstallation()) {
      return QueryError::kBadResponse;
    }
    return std::nullopt;
  }

  return data.error().type;
}

AppInstallResult AppInstallResultFromQueryError(
    QueryError::Type query_error_type) {
  switch (query_error_type) {
    case QueryError::kConnectionError:
      return AppInstallResult::kAlmanacFetchFailed;
    case QueryError::kBadRequest:
      return AppInstallResult::kBadAppRequest;
    case QueryError::kBadResponse:
      return AppInstallResult::kAppDataCorrupted;
  }
}

void RecordInstallResult(base::OnceClosure callback,
                         AppInstallSurface surface,
                         AppInstallResult result) {
  base::UmaHistogramEnumeration("Apps.AppInstallService.AppInstallResult",
                                result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Apps.AppInstallService.AppInstallResult.",
                    base::ToString(surface)}),
      result);
  std::move(callback).Run();
}

}  // namespace

base::OnceCallback<void(PackageId)>&
AppInstallServiceAsh::InstallAppCallbackForTesting() {
  static base::NoDestructor<base::OnceCallback<void(PackageId)>> callback;
  return *callback;
}

AppInstallServiceAsh::AppInstallServiceAsh(Profile& profile)
    : profile_(profile),
      arc_app_installer_(&*profile_),
      web_app_installer_(&*profile_) {}

AppInstallServiceAsh::~AppInstallServiceAsh() = default;

void AppInstallServiceAsh::InstallAppWithFallback(
    AppInstallSurface surface,
    std::string serialized_package_id,
    std::optional<WindowIdentifier> anchor_window,
    base::OnceClosure callback) {
  if (std::optional<PackageId> package_id =
          PackageId::FromString(serialized_package_id)) {
    InstallApp(surface, std::move(package_id).value(), anchor_window,
               std::move(callback));
    return;
  }

  base::OnceCallback<void(AppInstallResult)> result_callback =
      base::BindOnce(&RecordInstallResult, std::move(callback), surface);

  FetchAppInstallUrl(
      std::move(serialized_package_id),
      base::BindOnce(&AppInstallServiceAsh::MaybeLaunchAppInstallUrl,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(result_callback)));
}

void AppInstallServiceAsh::InstallApp(
    AppInstallSurface surface,
    PackageId package_id,
    std::optional<gfx::NativeWindow> anchor_window,
    base::OnceClosure callback) {
  if (InstallAppCallbackForTesting()) {
    std::move(InstallAppCallbackForTesting()).Run(package_id);
  }

  RecordAppDiscoveryMetricForInstallRequest(&profile_.get(), surface,
                                            package_id);

  base::OnceCallback<void(AppInstallResult)> result_callback =
      base::BindOnce(&RecordInstallResult, std::move(callback), surface);

  if (!CanUserInstall()) {
    std::move(result_callback).Run(AppInstallResult::kUserTypeNotPermitted);
    return;
  }

  switch (package_id.package_type()) {
    case PackageType::kArc:
    case PackageType::kGeForceNow:
    case PackageType::kWeb:
    case PackageType::kWebsite: {
      // Observe for `anchor_window` being destroyed during async work.
      std::unique_ptr<views::NativeWindowTracker> anchor_window_tracker;
      if (anchor_window) {
        anchor_window_tracker =
            views::NativeWindowTracker::Create(*anchor_window);
      }

      FetchAppInstallData(
          package_id,
          base::BindOnce(&AppInstallServiceAsh::ShowDialogAndInstall,
                         weak_ptr_factory_.GetWeakPtr(), surface, package_id,
                         anchor_window, std::move(anchor_window_tracker),
                         std::move(result_callback)));
      return;
    }
    case PackageType::kBorealis:
    case PackageType::kChromeApp:
    case PackageType::kSystem:
    case PackageType::kUnknown:
      std::move(result_callback).Run(AppInstallResult::kAppTypeNotSupported);
      return;
  }
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

bool AppInstallServiceAsh::CanUserInstall() const {
  if (profile_->IsSystemProfile()) {
    return false;
  }

  if (!ash::IsUserBrowserContext(&*profile_)) {
    return false;
  }

  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    return false;
  }

  if (user_manager->IsLoggedInAsManagedGuestSession() ||
      user_manager->IsLoggedInAsGuest() ||
      user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  return true;
}

void AppInstallServiceAsh::FetchAppInstallData(
    PackageId package_id,
    app_install_almanac_endpoint::GetAppInstallInfoCallback data_callback) {
  app_install_almanac_endpoint::GetAppInstallInfo(&profile_.get(), package_id,
                                                  std::move(data_callback));
}

void AppInstallServiceAsh::PerformInstallHeadless(
    AppInstallSurface surface,
    PackageId expected_package_id,
    base::OnceCallback<void(bool success)> callback,
    base::expected<AppInstallData, QueryError> data) {
  // TODO(b/327535848): Record metrics for headless installs.
  if (!data.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  RecordAppDiscoveryMetricForInstallRequest(&profile_.get(), surface,
                                            expected_package_id);

  PerformInstall(surface, *data, std::move(callback));
}

void AppInstallServiceAsh::ShowDialogAndInstall(
    AppInstallSurface surface,
    PackageId expected_package_id,
    std::optional<gfx::NativeWindow> anchor_window,
    std::unique_ptr<views::NativeWindowTracker> anchor_window_tracker,
    base::OnceCallback<void(AppInstallResult)> callback,
    base::expected<AppInstallData, QueryError> data) {
  gfx::NativeWindow parent =
      anchor_window.has_value() &&
              !anchor_window_tracker->WasNativeWindowDestroyed()
          ? anchor_window.value()
          : nullptr;

  if (std::optional<QueryError::Type> query_error =
          VerifyAppInstallData(data, expected_package_id)) {
    base::WeakPtr<ash::app_install::AppInstallDialog> dialog =
        ash::app_install::AppInstallDialog::CreateDialog();
    switch (query_error.value()) {
      case QueryError::kConnectionError:
        dialog->ShowConnectionError(
            parent, base::BindOnce(&AppInstallServiceAsh::InstallApp,
                                   weak_ptr_factory_.GetWeakPtr(), surface,
                                   expected_package_id, anchor_window,
                                   base::DoNothing()));
        break;
      case QueryError::kBadRequest:
      case QueryError::kBadResponse:
        dialog->ShowNoAppError(parent);
        break;
    }

    std::move(callback).Run(
        AppInstallResultFromQueryError(query_error.value()));
    return;
  }

  bool show_install_dialog =
      expected_package_id.package_type() == PackageType::kWeb ||
      expected_package_id.package_type() == PackageType::kWebsite;

  // If we can't show the install dialog, we must have an install URL to open.
  if (!show_install_dialog) {
    // This is checked by VerifyAppInstallData:
    CHECK(data->install_url.is_valid());
    LaunchUrlInInstalledAppOrBrowser(&*profile_, data->install_url,
                                     LaunchSource::kFromInstaller);
    std::move(callback).Run(AppInstallResult::kUnknown);
    return;
  }

  // The install dialog is only used for web apps currently.
  CHECK(absl::holds_alternative<WebAppInstallData>(data->app_type_data));
  const WebAppInstallData& web_app_data =
      absl::get<WebAppInstallData>(data->app_type_data);

  if (expected_package_id.package_type() == PackageType::kWebsite) {
    // kWebsite packages will end up installed as a regular kWeb app. Pass a
    // kWeb package ID to the Install Dialog so that it can look for the correct
    // installed app.
    // An alternative would be to set the installer_package_id for shortcut web
    // apps as kWebsite in App Service. However, this is difficult to manage
    // correctly, as the user could already have a non-shortcut web app
    // installed with the same identifier.
    expected_package_id =
        PackageId(PackageType::kWeb, expected_package_id.identifier());
  }

  std::vector<ash::app_install::mojom::ScreenshotPtr> screenshots;
  for (auto& screenshot : data->screenshots) {
    auto dialog_screenshot = ash::app_install::mojom::Screenshot::New();
    dialog_screenshot->url = screenshot.url;
    dialog_screenshot->size =
        gfx::Size(screenshot.width_in_pixels, screenshot.height_in_pixels);
    screenshots.push_back(std::move(dialog_screenshot));
  }

  base::WeakPtr<ash::app_install::AppInstallDialog> dialog =
      ash::app_install::AppInstallDialog::CreateDialog();
  dialog->ShowApp(&*profile_, parent, expected_package_id, data->name,
                  web_app_data.document_url, data->description, data->icon,
                  std::move(screenshots),
                  base::BindOnce(&AppInstallServiceAsh::InstallIfDialogAccepted,
                                 weak_ptr_factory_.GetWeakPtr(), surface,
                                 data.value(), dialog, std::move(callback)));
}

void AppInstallServiceAsh::InstallIfDialogAccepted(
    AppInstallSurface surface,
    AppInstallData data,
    base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
    base::OnceCallback<void(AppInstallResult)> callback,
    bool dialog_accepted) {
  if (!dialog_accepted) {
    std::move(callback).Run(AppInstallResult::kInstallDialogNotAccepted);
    return;
  }

  PerformInstall(surface, data,
                 base::BindOnce(&AppInstallServiceAsh::ProcessInstallResult,
                                weak_ptr_factory_.GetWeakPtr(), surface, data,
                                dialog, std::move(callback)));
}

void AppInstallServiceAsh::ProcessInstallResult(
    AppInstallSurface surface,
    AppInstallData data,
    base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
    base::OnceCallback<void(AppInstallResult)> callback,
    bool install_success) {
  if (!dialog) {
    std::move(callback).Run(install_success
                                ? AppInstallResult::kSuccess
                                : AppInstallResult::kAppTypeInstallFailed);
    return;
  }

  if (install_success) {
    dialog->SetInstallSucceeded();
    std::move(callback).Run(AppInstallResult::kSuccess);
    return;
  }

  dialog->SetInstallFailed(
      base::BindOnce(&AppInstallServiceAsh::InstallIfDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr(), surface, std::move(data),
                     dialog, std::move(callback)));
}

void AppInstallServiceAsh::PerformInstall(
    AppInstallSurface surface,
    AppInstallData data,
    base::OnceCallback<void(bool)> install_callback) {
  if (absl::holds_alternative<AndroidAppInstallData>(data.app_type_data)) {
    arc_app_installer_.InstallApp(surface, std::move(data),
                                  std::move(install_callback));
  } else if (absl::holds_alternative<WebAppInstallData>(data.app_type_data)) {
    web_app_installer_.InstallApp(surface, std::move(data),
                                  std::move(install_callback));
  } else {
    LOG(ERROR) << "Unsupported AppInstallData type";
    std::move(install_callback).Run(false);
  }
}

void AppInstallServiceAsh::FetchAppInstallUrl(
    std::string serialized_package_id,
    base::OnceCallback<void(base::expected<GURL, QueryError>)> callback) {
  app_install_almanac_endpoint::GetAppInstallUrl(
      &profile_.get(), serialized_package_id, std::move(callback));
}

void AppInstallServiceAsh::MaybeLaunchAppInstallUrl(
    base::OnceCallback<void(AppInstallResult)> callback,
    base::expected<GURL, QueryError> install_url) {
  if (install_url.has_value()) {
    LaunchUrlInInstalledAppOrBrowser(&*profile_, install_url.value(),
                                     LaunchSource::kFromInstaller);
    std::move(callback).Run(AppInstallResult::kInstallUrlFallback);
    return;
  }

  base::WeakPtr<ash::app_install::AppInstallDialog> dialog =
      ash::app_install::AppInstallDialog::CreateDialog();
  switch (install_url.error().type) {
    case QueryError::kConnectionError:
      // TODO(b/339548810): Show connection error dialog instead, this needs
      // the parameters necessary for a retry_callback to be plumbed through
      // to here.
    case QueryError::kBadRequest:
    case QueryError::kBadResponse:
      // TODO(b/339548810): Plumb the parent window through to here.
      dialog->ShowNoAppError(/*parent=*/nullptr);
      break;
  }
  std::move(callback).Run(
      AppInstallResultFromQueryError(install_url.error().type));
}

}  // namespace apps
