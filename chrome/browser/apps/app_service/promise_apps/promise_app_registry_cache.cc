// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"

namespace apps {

PromiseAppRegistryCache::PromiseAppRegistryCache() = default;

PromiseAppRegistryCache::~PromiseAppRegistryCache() = default;

void PromiseAppRegistryCache::OnPromiseApp(PromiseAppPtr delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check that there isn't an update currently being processed. We do not allow
  // an update to trigger an observer to send another update.
  DCHECK(!update_in_progress_);
  update_in_progress_ = true;

  // Retrieve the current promise app state.
  apps::PromiseApp* state = FindPromiseApp(delta->package_id);

  // Update the promise app with the delta or add the new promise app instance
  // to the registry.
  if (state) {
    PromiseAppUpdate::Merge(state, delta.get());
  } else {
    promise_app_map_[delta->package_id] = delta->Clone();
  }

  // TODO(b/261907495): Notify observers.

  update_in_progress_ = false;
}

// Retrieve a copy of all the registered promise apps.
std::vector<PromiseAppPtr> PromiseAppRegistryCache::GetAllPromiseApps() const {
  std::vector<PromiseAppPtr> promise_apps;
  for (const auto& promise_pair : promise_app_map_) {
    promise_apps.push_back(promise_pair.second.get()->Clone());
  }
  return promise_apps;
}

const PromiseApp* PromiseAppRegistryCache::GetPromiseAppForTesting(
    const PackageId& package_id) const {
  return FindPromiseApp(package_id);
}

PromiseApp* PromiseAppRegistryCache::FindPromiseApp(
    const PackageId& package_id) const {
  auto promise_iter = promise_app_map_.find(package_id);
  return (promise_iter != promise_app_map_.end()) ? promise_iter->second.get()
                                                  : nullptr;
}

}  // namespace apps
