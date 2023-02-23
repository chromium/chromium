// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/package_id.h"

namespace apps {

struct PromiseApp;
using PromiseAppPtr = std::unique_ptr<PromiseApp>;
using PromiseAppCacheMap = std::map<PackageId, PromiseAppPtr>;

// A cache that manages and keeps track of all promise apps on the
// system.
class PromiseAppRegistryCache {
 public:
  PromiseAppRegistryCache();

  PromiseAppRegistryCache(const PromiseAppRegistryCache&) = delete;
  PromiseAppRegistryCache& operator=(const PromiseAppRegistryCache&) = delete;

  ~PromiseAppRegistryCache();

  // Add a promise app to the PromiseAppRegistry.
  void AddPromiseApp(PromiseAppPtr promise_app);

  // Update the installation progress of a registered promise app.
  void UpdatePromiseAppProgress(PackageId& package_id, float progress);

 private:
  friend class PromiseAppRegistryCacheTest;
  friend class PublisherTest;

  apps::PromiseAppCacheMap promise_app_map_;

  // Flag to check whether an update to a promise app is already in progress. We
  // shouldn't have more than one concurrent update to a package_id,
  // e.g. if UpdatePromiseAppProgress notifies observers and triggers them to
  // call UpdatePromiseAppProgress again (before the first call to
  // UpdatePromiseAppProgress completes), we want to prevent overwriting fields.
  bool update_in_progress_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_
