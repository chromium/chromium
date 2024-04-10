// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include <memory>
#include <optional>

#include "ash/components/arc/arc_features.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_utils.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_package_install_priority_handler.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace {

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("promise_app_service_download_icon",
                                        R"(
    semantics {
      sender: "Promise App Service"
      description:
        "Queries a Google server to fetch the icon of an app that is being "
        "installed or is pending installation on the device."
      trigger:
        "A request can be sent when an app starts installing or is pending "
        "installation."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "chromeos-apps-foundation-team@google.com"
        }
      }
      user_data {
        type: SENSITIVE_URL
      }
      data: "URL of the image to be fetched."
      last_reviewed: "2023-05-16"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This request is enabled by app sync without passphrase. You can"
        "disable this request in the 'Sync and Google services' section"
        "in Settings by either: 1. Going into the 'Manage What You Sync'"
        "settings page and turning off Apps sync; OR 2. In the 'Encryption"
        "Options' settings page, select the option to use a sync passphrase."
      policy_exception_justification:
        "This feature is required to deliver core user experiences and "
        "cannot be disabled by policy."
    }
  )");

apps::PromiseAppType GetPromiseAppType(apps::PackageType promise_app_type,
                                       apps::AppType installed_app_type) {
  if (promise_app_type == apps::PackageType::kArc &&
      installed_app_type == apps::AppType::kArc) {
    return apps::PromiseAppType::kArc;
  }
  if (promise_app_type == apps::PackageType::kArc &&
      installed_app_type == apps::AppType::kWeb) {
    return apps::PromiseAppType::kTwa;
  }
  return apps::PromiseAppType::kUnknown;
}

}  // namespace

namespace apps {
PromiseAppService::PromiseAppService(Profile* profile,
                                     AppRegistryCache& app_registry_cache)
    : profile_(profile),
      promise_app_registry_cache_(
          std::make_unique<apps::PromiseAppRegistryCache>()),
      promise_app_almanac_connector_(
          std::make_unique<PromiseAppAlmanacConnector>(profile)),
      promise_app_icon_cache_(std::make_unique<apps::PromiseAppIconCache>()),
      image_fetcher_(std::make_unique<image_fetcher::ImageFetcherImpl>(
          std::make_unique<ImageDecoderImpl>(),
          profile->GetURLLoaderFactory())),
      app_registry_cache_(&app_registry_cache) {
  app_registry_cache_observation_.Observe(&app_registry_cache);
}

PromiseAppService::~PromiseAppService() = default;

PromiseAppRegistryCache* PromiseAppService::PromiseAppRegistryCache() {
  return promise_app_registry_cache_.get();
}

PromiseAppIconCache* PromiseAppService::PromiseAppIconCache() {
  return promise_app_icon_cache_.get();
}

void PromiseAppService::OnPromiseApp(PromiseAppPtr delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const PackageId package_id = delta->package_id;
  bool is_new_promise_app =
      !promise_app_registry_cache_->HasPromiseApp(package_id);

  // If the app is in the AppRegistryCache, then it already has an item in the
  // Launcher/ Shelf and we don't need to create a new promise app item to
  // represent it. This scenario happens when we start installing a default ARC
  // app (which is a stubbed app in App Registry Cache to show an icon in the
  // Launcher/ Shelf but uses legacy ARC default apps implementation).
  // TODO(b/286981938): Remove this check after refactoring to allow the Promise
  // App Service to manage ARC default app icons.
  if (is_new_promise_app && IsRegisteredInAppRegistryCache(package_id)) {
    return;
  }

  promise_app_registry_cache_->OnPromiseApp(std::move(delta));

  // If the promise app is newly removed, clear out the icons.
  if (!promise_app_registry_cache_->HasPromiseApp(package_id)) {
    promise_app_icon_cache_->RemoveIconsForPackageId(package_id);
  }

  // If this is a new promise app, send an Almanac request to fetch more
  // details.
  if (is_new_promise_app && !skip_almanac_for_testing_) {
    promise_app_almanac_connector_->GetPromiseAppInfo(
        package_id,
        base::BindOnce(&PromiseAppService::OnGetPromiseAppInfoCompleted,
                       weak_ptr_factory_.GetWeakPtr(), package_id));
  }
}

void PromiseAppService::LoadIcon(const PackageId& package_id,
                                 int32_t size_hint_in_dip,
                                 apps::IconEffects icon_effects,
                                 apps::LoadIconCallback callback) {
  promise_app_icon_cache_->GetIconAndApplyEffects(
      package_id, size_hint_in_dip, icon_effects, std::move(callback));
}

void PromiseAppService::OnAppUpdate(const apps::AppUpdate& update) {
  // Check that the update is for a new installation.
  if (!update.ReadinessChanged() ||
      update.Readiness() != apps::Readiness::kReady ||
      apps_util::IsInstalled(update.PriorReadiness())) {
    return;
  }

  std::optional<PackageId> package_id =
      apps_util::GetPackageIdForApp(profile_.get(), update);
  if (!package_id.has_value()) {
    return;
  }

  // Check that the update corresponds to a registered promise app.
  if (!promise_app_registry_cache_->HasPromiseApp(package_id.value())) {
    return;
  }

  // Record metrics for app type, noting that the app type may differ between
  // the promise app and the installed app.
  RecordPromiseAppType(
      GetPromiseAppType(package_id->package_type(), update.AppType()));

  // Delete the promise app.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id.value());
  promise_app->status = PromiseStatus::kSuccess;
  promise_app->installed_app_id = update.AppId();
  OnPromiseApp(std::move(promise_app));
}

void PromiseAppService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observation_.Reset();
  app_registry_cache_ = nullptr;
}

void PromiseAppService::SetSkipAlmanacForTesting(bool skip_almanac) {
  skip_almanac_for_testing_ = skip_almanac;
}

void PromiseAppService::SetSkipApiKeyCheckForTesting(bool skip_api_key_check) {
  promise_app_almanac_connector_->SetSkipApiKeyCheckForTesting(  // IN-TEST
      skip_api_key_check);
}

void PromiseAppService::UpdateInstallPriority(const std::string& id) {
  const auto* promise_app =
      promise_app_registry_cache_->GetPromiseAppForStringPackageId(id);
  CHECK(promise_app);

  // Currently, updating install priority is only supported for ARC promise
  // apps.
  if (promise_app->package_id.package_type() != PackageType::kArc) {
    return;
  }

  // Feature flag that enables interacing with promise icon.
  if (!base::FeatureList::IsEnabled(arc::kSyncInstallPriority)) {
    return;
  }

  // We can only increase install priority for packages that are
  // queued/ pending. Promise apps that are already actively installing are
  // already treated as the highest priority installation and their installation
  // progress cannot be accelerated any further.
  if (promise_app->status != PromiseStatus::kPending) {
    return;
  }

  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile_);
  CHECK(arc_app_list_prefs);

  arc_app_list_prefs->GetInstallPriorityHandler()->PromotePackageInstall(
      promise_app->package_id.identifier());
}

void PromiseAppService::OnGetPromiseAppInfoCompleted(
    const PackageId& package_id,
    std::optional<PromiseAppWrapper> promise_app_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the promise app doesn't exist in the registry, drop the update. The app
  // installation may have completed before the Almanac returned a response.
  if (!promise_app_registry_cache_->HasPromiseApp(package_id)) {
    LOG(ERROR) << "Cannot update promise app " << package_id.ToString()
               << " as it does not exist in PromiseAppRegistry";
    return;
  }

  // If Almanac doesn't provide any meaningful response, continue to show the
  // promise app item. When an icon is requested, the PromiseAppIconCache will
  // fallback to returning a placeholder icon.
  if (!promise_app_info.has_value() ||
      !promise_app_info->GetName().has_value() ||
      promise_app_info->GetIcons().size() == 0) {
    RecordPromiseAppIconType(PromiseAppIconType::kPlaceholderIcon);
    SetPromiseAppReadyToShow(package_id);
    return;
  }

  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->name = promise_app_info->GetName().value();
  promise_app_registry_cache_->OnPromiseApp(std::move(promise_app));

  pending_download_count_[package_id] = promise_app_info->GetIcons().size();

  for (auto icon : promise_app_info->GetIcons()) {
    image_fetcher_->FetchImage(
        icon.GetUrl(),
        base::BindOnce(&PromiseAppService::OnIconDownloaded,
                       weak_ptr_factory_.GetWeakPtr(), package_id),
        image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                          "Promise App Service Icon Fetcher"));
  }
}

void PromiseAppService::OnIconDownloaded(
    const PackageId& package_id,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we weren't expecting an icon to be downloaded for this package ID, don't
  // process the result.
  if (!pending_download_count_.contains(package_id)) {
    LOG(ERROR) << "Will not save icon for unexpected package ID: "
               << package_id.ToString();
    return;
  }
  if (pending_download_count_[package_id] == 0) {
    LOG(ERROR) << "Will not save icon for unexpected package ID: "
               << package_id.ToString();
    pending_download_count_.erase(package_id);
    return;
  }

  // Save valid icons to the icon cache.
  if (!image.IsEmpty()) {
    PromiseAppIconPtr promise_app_icon = std::make_unique<PromiseAppIcon>();
    promise_app_icon->icon = image.AsBitmap();
    promise_app_icon->width_in_pixels = promise_app_icon->icon.width();
    promise_app_icon_cache_->SaveIcon(package_id, std::move(promise_app_icon));
  }

  // If there are still icons to be downloaded, we should wait for those
  // downloads to finish before updating the promise app. Otherwise, stop
  // tracking pending downloads for this package ID.
  pending_download_count_[package_id] -= 1;
  if (pending_download_count_[package_id] > 0) {
    return;
  }
  pending_download_count_.erase(package_id);
  RecordPromiseAppIconType(
      promise_app_icon_cache_->DoesPackageIdHaveIcons(package_id)
          ? PromiseAppIconType::kRealIcon
          : PromiseAppIconType::kPlaceholderIcon);

  // Update the promise app so it can show to the user.
  SetPromiseAppReadyToShow(package_id);
}

bool PromiseAppService::IsRegisteredInAppRegistryCache(
    const PackageId& package_id) {
  if (!app_registry_cache_) {
    return false;
  }
  bool is_registered = false;
  app_registry_cache_->ForEachApp(
      [&package_id, &is_registered](const AppUpdate& update) {
        // TODO(b/297296711): Update check for TWAs, which can have differing
        // package IDs.
        if (ConvertPackageTypeToAppType(package_id.package_type()) !=
            update.AppType()) {
          return;
        }
        if (update.PublisherId() != package_id.identifier()) {
          return;
        }
        if (!apps_util::IsInstalled(update.Readiness())) {
          // It's possible for an app to be in the AppRegistryCache despite
          // being uninstalled. Do not consider this as a registered
          // installed app.
          return;
        }
        is_registered = true;
        return;
      });
  return is_registered;
}

void PromiseAppService::SetPromiseAppReadyToShow(const PackageId& package_id) {
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->should_show = true;
  promise_app_registry_cache_->OnPromiseApp(std::move(promise_app));
}

void PromiseAppService::OnApkWebAppInstallationFinished(
    const std::string& package_name) {
  PackageId package_id(PackageType::kArc, package_name);

  // Successful APK web app installations are already handled during a call to
  // observers via AppRegistryCache::OnAppUpdate which happens before this
  // method is called.
  if (!promise_app_registry_cache_->HasPromiseApp(package_id)) {
    return;
  }

  // We get to this point if the APK web installation failed. In this case, we
  // should remove the promise app and consider it a cancellation.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->status = PromiseStatus::kCancelled;
  OnPromiseApp(std::move(promise_app));
}

}  // namespace apps
