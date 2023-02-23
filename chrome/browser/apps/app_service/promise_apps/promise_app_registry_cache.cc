// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"

namespace apps {

PromiseAppRegistryCache::PromiseAppRegistryCache() = default;

PromiseAppRegistryCache::~PromiseAppRegistryCache() = default;

void PromiseAppRegistryCache::AddPromiseApp(PromiseAppPtr promise_app) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const PackageId& package_id = promise_app->package_id;
  DCHECK(!promise_app_map_.contains(package_id));
  promise_app_map_[package_id] = std::move(promise_app);

  // TODO(b/261907495): Notify observers.
}

void PromiseAppRegistryCache::UpdatePromiseAppProgress(PackageId& package_id,
                                                       float progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check that there isn't an update currently being processed. We do not allow
  // an update to trigger an observer to send another update.
  DCHECK(!update_in_progress_);
  update_in_progress_ = true;

  auto promise_iter = promise_app_map_.find(package_id);
  DCHECK(promise_iter != promise_app_map_.end());
  PromiseApp* promise_app = (promise_iter != promise_app_map_.end())
                                ? promise_iter->second.get()
                                : nullptr;
  if (promise_app) {
    promise_app->progress = progress;
  }

  // TODO(b/261907495): Notify observers.

  update_in_progress_ = false;
}

}  // namespace apps
