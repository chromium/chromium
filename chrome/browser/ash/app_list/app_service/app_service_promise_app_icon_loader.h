// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_ICON_LOADER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_ICON_LOADER_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class Profile;

// An AppIconLoader that loads promise icons for app service promise apps.
class AppServicePromiseAppIconLoader
    : public AppIconLoader,
      private apps::PromiseAppRegistryCache::Observer {
 public:
  AppServicePromiseAppIconLoader(Profile* profile,
                                 int resource_size_in_dip,
                                 AppIconLoaderDelegate* delegate);

  AppServicePromiseAppIconLoader(const AppServicePromiseAppIconLoader&) =
      delete;
  AppServicePromiseAppIconLoader& operator=(
      const AppServicePromiseAppIconLoader&) = delete;

  ~AppServicePromiseAppIconLoader() override;

  // AppIconLoader overrides:
  bool CanLoadImageForApp(const std::string& id) override;
  void FetchImage(const std::string& id) override;
  void ClearImage(const std::string& id) override;
  void UpdateImage(const std::string& id) override;

  // apps::PromiseAppRegistryCache::Observer overrides:
  void OnPromiseAppUpdate(const apps::PromiseAppUpdate& update) override;
  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override;

  static bool CanLoadImage(Profile* profile, const std::string& id);

 private:
  base::ScopedObservation<apps::PromiseAppRegistryCache,
                          apps::PromiseAppRegistryCache::Observer>
      promise_app_registry_cache_observation_{this};

  // Calls AppService LoadPromiseIcon to load icons.
  void CallLoadIcon(const apps::PackageId& package_id,
                    apps::IconEffects icon_effects);

  // Callback invoked when the icon is loaded.
  void OnLoadIcon(const apps::PackageId& package_id,
                  apps::IconValuePtr icon_value);

  base::WeakPtrFactory<AppServicePromiseAppIconLoader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_ICON_LOADER_H_
