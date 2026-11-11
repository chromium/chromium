// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_MODEL_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_MODEL_BUILDER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_model_builder.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class AppListControllerDelegate;

class AppServiceAppModelBuilder : public AppListModelBuilder,
                                  public apps::AppRegistryCache::Observer {
 public:
  // `app_registry_cache` must not be null, and must outlive `this`.
  AppServiceAppModelBuilder(AppListControllerDelegate* controller,
                            apps::AppRegistryCache* app_registry_cache);

  AppServiceAppModelBuilder(const AppServiceAppModelBuilder&) = delete;
  AppServiceAppModelBuilder& operator=(const AppServiceAppModelBuilder&) =
      delete;

  ~AppServiceAppModelBuilder() override;

 private:
  // AppListModelBuilder overrides:
  void BuildModel() override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const raw_ref<apps::AppRegistryCache> app_registry_cache_;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  std::unique_ptr<AppListModelUpdaterObserver> crostini_folder_observer_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_MODEL_BUILDER_H_
