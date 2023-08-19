// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ICON_LOADER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ICON_LOADER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

// An AppIconLoader that loads icons for app service apps.
class AppServiceAppIconLoader : public AppIconLoader,
                                private apps::AppRegistryCache::Observer {
 public:
  static bool CanLoadImage(Profile* profile, const std::string& id);

  AppServiceAppIconLoader(Profile* profile,
                          int resource_size_in_dip,
                          AppIconLoaderDelegate* delegate);

  AppServiceAppIconLoader(const AppServiceAppIconLoader&) = delete;
  AppServiceAppIconLoader& operator=(const AppServiceAppIconLoader&) = delete;

  ~AppServiceAppIconLoader() override;

  // AppIconLoader overrides:
  bool CanLoadImageForApp(const std::string& id) override;
  void FetchImage(const std::string& id) override;
  void ClearImage(const std::string& id) override;
  void UpdateImage(const std::string& id) override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 protected:
  using AppIDToIconMap = std::map<std::string, gfx::ImageSkia>;
  using AppIDToShelfAppId = std::map<std::string, std::set<std::string>>;

  // Calls AppService LoadIcon to load icons.
  void CallLoadIcon(const std::string& app_id, bool allow_placeholder_icon);

  // Callback invoked when the icon is loaded.
  virtual void OnLoadIcon(const std::string& app_id,
                          apps::IconValuePtr icon_value);

  // Returns true if the app_id does exist in icon_map_.
  bool Exist(const std::string& app_id);

 private:
  // Maps from an app id to shelf app ids.
  AppIDToShelfAppId shelf_app_id_map_;

  // Maps from an app id to the icon to track the icons added via FetchImage.
  AppIDToIconMap icon_map_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<AppServiceAppIconLoader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ICON_LOADER_H_
