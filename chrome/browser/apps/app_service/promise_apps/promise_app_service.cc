// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"

namespace apps {
PromiseAppService::PromiseAppService(Profile* profile)
    : promise_app_registry_cache_(
          std::make_unique<apps::PromiseAppRegistryCache>()),
      promise_app_almanac_connector_(
          std::make_unique<PromiseAppAlmanacConnector>(profile)) {}

PromiseAppService::~PromiseAppService() = default;

PromiseAppRegistryCache* PromiseAppService::PromiseAppRegistryCache() {
  return promise_app_registry_cache_.get();
}

void PromiseAppService::OnPromiseApp(PromiseAppPtr delta) {
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

void PromiseAppService::OnGetPromiseAppInfoCompleted(
    const PackageId& package_id,
    absl::optional<PromiseAppWrapper> promise_app_info) {
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
  promise_app->should_show = true;
  OnPromiseApp(std::move(promise_app));
}
}  // namespace apps
