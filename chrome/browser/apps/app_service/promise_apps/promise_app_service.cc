// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
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

}  // namespace

namespace apps {
PromiseAppService::PromiseAppService(Profile* profile)
    : promise_app_registry_cache_(
          std::make_unique<apps::PromiseAppRegistryCache>()),
      promise_app_almanac_connector_(
          std::make_unique<PromiseAppAlmanacConnector>(profile)),
      promise_app_icon_cache_(std::make_unique<apps::PromiseAppIconCache>()),
      image_fetcher_(std::make_unique<image_fetcher::ImageFetcherImpl>(
          std::make_unique<ImageDecoderImpl>(),
          profile->GetURLLoaderFactory())) {}

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
  bool is_existing_registration =
      promise_app_registry_cache_->HasPromiseApp(package_id);
  promise_app_registry_cache_->OnPromiseApp(std::move(delta));

  if (is_existing_registration) {
    return;
  }

  // Exit early to simplify unit tests that don't care about Almanac.
  if (skip_almanac_for_testing_) {
    return;
  }

  // If this is a new promise app, send an Almanac request to fetch more
  // details.
  promise_app_almanac_connector_->GetPromiseAppInfo(
      package_id,
      base::BindOnce(&PromiseAppService::OnGetPromiseAppInfoCompleted,
                     weak_ptr_factory_.GetWeakPtr(), package_id));
}

void PromiseAppService::SetSkipAlmanacForTesting(bool skip_almanac) {
  skip_almanac_for_testing_ = skip_almanac;
}

void PromiseAppService::SetImageFetcherForTesting(
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher) {
  image_fetcher_ = std::move(image_fetcher);
}

void PromiseAppService::OnGetPromiseAppInfoCompleted(
    const PackageId& package_id,
    absl::optional<PromiseAppWrapper> promise_app_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!promise_app_info.has_value()) {
    LOG(ERROR) << "Request for app details from the Almanac Promise App API "
                  "failed for package "
               << package_id.ToString();

    // TODO(b/276841106): Remove promise app from the cache and its observers.
    return;
  }
  if (!promise_app_info->GetPackageId().has_value() ||
      !promise_app_info->GetName().has_value() ||
      promise_app_info->GetIcons().size() == 0) {
    LOG(ERROR) << "Cannot update promise app " << package_id.ToString()
               << " due to incomplete Almanac Promise App API response.";
    return;
  }

  // The response's Package ID should match with our original request.
  if (package_id != promise_app_info->GetPackageId().value()) {
    LOG(ERROR) << "Cannot update promise app due to mismatching package IDs "
                  "between the request ("
               << package_id.ToString() << ") and response ("
               << promise_app_info->GetPackageId().value().ToString() << ")";
    return;
  }

  // If the promise app doesn't exist in the registry, drop the update. The app
  // installation may have completed before the Almanac returned a response.
  if (!promise_app_registry_cache_->HasPromiseApp(package_id)) {
    LOG(ERROR) << "Cannot update promise app " << package_id.ToString()
               << " as it does not exist in PromiseAppRegistry";
    return;
  }

  PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(promise_app_info->GetPackageId().value());
  promise_app->name = promise_app_info->GetName().value();
  OnPromiseApp(std::move(promise_app));

  pending_download_count_[package_id] = promise_app_info->GetIcons().size();

  for (auto icon : promise_app_info->GetIcons()) {
    PromiseAppIconPtr promise_app_icon = std::make_unique<PromiseAppIcon>();
    promise_app_icon->width_in_pixels = icon.GetWidthInPixels();
    promise_app_icon->is_masking_allowed = icon.IsMaskingAllowed();

    image_fetcher_->FetchImage(
        icon.GetUrl(),
        base::BindOnce(&PromiseAppService::OnIconDownloaded,
                       weak_ptr_factory_.GetWeakPtr(), package_id,
                       std::move(promise_app_icon)),
        image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                          "Promise App Service Icon Fetcher"));
  }
}

void PromiseAppService::OnIconDownloaded(
    const PackageId& package_id,
    PromiseAppIconPtr promise_app_icon,
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
    promise_app_icon->icon = image.AsImageSkia();
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

  // If there are no successfully downloaded icons, we don't want to update or
  // show the promise icon at all.
  if (!promise_app_icon_cache_->DoesPackageIdHaveIcons(package_id)) {
    return;
  }

  // Update the promise app so it can show to the user.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->should_show = true;
  promise_app_registry_cache_->OnPromiseApp(std::move(promise_app));
}
}  // namespace apps
