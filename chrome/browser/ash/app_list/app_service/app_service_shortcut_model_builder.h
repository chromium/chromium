// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_MODEL_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_MODEL_BUILDER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_model_builder.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

class AppListControllerDelegate;

// Model builder that creates and manages App Service Shortcut Items to track
// entries in the profile's Shortcut Registry Cache.
class AppServiceShortcutModelBuilder
    : public AppListModelBuilder,
      public apps::ShortcutRegistryCache::Observer {
 public:
  explicit AppServiceShortcutModelBuilder(
      AppListControllerDelegate* controller);

  AppServiceShortcutModelBuilder(const AppServiceShortcutModelBuilder&) =
      delete;
  AppServiceShortcutModelBuilder& operator=(
      const AppServiceShortcutModelBuilder&) = delete;

  ~AppServiceShortcutModelBuilder() override;

 private:
  // AppListModelBuilder overrides:
  void BuildModel() override;

  // apps::ShortcutRegistryCache::Observer overrides:
  void OnShortcutUpdated(const apps::ShortcutUpdate& update) override;
  void OnShortcutRegistryCacheWillBeDestroyed(
      apps::ShortcutRegistryCache* cache) override;

  base::ScopedObservation<apps::ShortcutRegistryCache,
                          apps::ShortcutRegistryCache::Observer>
      shortcut_registry_cache_observation_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_MODEL_BUILDER_H_
