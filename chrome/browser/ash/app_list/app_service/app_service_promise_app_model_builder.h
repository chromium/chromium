// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_MODEL_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_MODEL_BUILDER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/app_list/app_list_model_builder.h"

class AppListControllerDelegate;

// Model builder that creates and manages App Service Promise App Items to track
// entries in the profile's Promise App Registry Cache.
class AppServicePromiseAppModelBuilder
    : public AppListModelBuilder,
      public apps::PromiseAppRegistryCache::Observer {
 public:
  explicit AppServicePromiseAppModelBuilder(
      AppListControllerDelegate* controller);

  AppServicePromiseAppModelBuilder(const AppServicePromiseAppModelBuilder&) =
      delete;
  AppServicePromiseAppModelBuilder& operator=(
      const AppServicePromiseAppModelBuilder&) = delete;

  ~AppServicePromiseAppModelBuilder() override;

 private:
  // AppListModelBuilder overrides:
  void BuildModel() override;

  // apps::PromiseAppRegistryCache::Observer overrides:
  void OnPromiseAppUpdate(const apps::PromiseAppUpdate& update) override;
  void OnPromiseAppRemoved(const apps::PackageId& package_id) override;
  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override;

  base::ScopedObservation<apps::PromiseAppRegistryCache,
                          apps::PromiseAppRegistryCache::Observer>
      promise_app_registry_cache_observation_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_MODEL_BUILDER_H_
