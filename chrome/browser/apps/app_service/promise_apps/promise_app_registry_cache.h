// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_H_

#include <map>
#include <memory>

namespace apps {

class PackageId;

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

  void AddPromiseApp(PromiseAppPtr promise_app);

 private:
  friend class PromiseAppRegistryCacheTest;
  friend class PublisherTest;

  apps::PromiseAppCacheMap promise_app_map_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_REGISTRY_CACHE_
