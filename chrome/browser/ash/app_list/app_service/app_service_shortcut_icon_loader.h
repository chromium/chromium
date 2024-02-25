// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_ICON_LOADER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_ICON_LOADER_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

class Profile;

// An AppIconLoader that loads shortcut icons for app service shortcuts.
class AppServiceShortcutIconLoader
    : public AppIconLoader,
      private apps::ShortcutRegistryCache::Observer {
 public:
  AppServiceShortcutIconLoader(Profile* profile,
                               int resource_size_in_dip,
                               int badge_size_in_dip,
                               AppIconLoaderDelegate* delegate);

  AppServiceShortcutIconLoader(const AppServiceShortcutIconLoader&) = delete;
  AppServiceShortcutIconLoader& operator=(const AppServiceShortcutIconLoader&) =
      delete;

  ~AppServiceShortcutIconLoader() override;

  // AppIconLoader overrides:
  bool CanLoadImageForApp(const std::string& id) override;
  void FetchImage(const std::string& id) override;
  void ClearImage(const std::string& id) override;
  void UpdateImage(const std::string& id) override;

  // apps::ShortcutRegistryCache::Observer overrides:
  void OnShortcutUpdated(const apps::ShortcutUpdate& update) override;
  void OnShortcutRegistryCacheWillBeDestroyed(
      apps::ShortcutRegistryCache* cache) override;

  static bool CanLoadImage(Profile* profile, const std::string& id);

 protected:
  // Callback invoked when the icon is loaded.
  virtual void OnLoadIcon(const apps::ShortcutId& shortcut_id,
                          apps::IconValuePtr icon_value,
                          apps::IconValuePtr badge_icon_value);

 private:
  struct IconInfo {
    gfx::ImageSkia image;
    gfx::ImageSkia badge;
  };

  using ShortcutIDToIconMap = std::map<std::string, IconInfo>;

  base::ScopedObservation<apps::ShortcutRegistryCache,
                          apps::ShortcutRegistryCache::Observer>
      shortcut_registry_cache_observation_{this};

  // Calls AppService LoadShortcutIconWithBadge to load icons.
  void CallLoadIcon(const apps::ShortcutId& shortcut_id);

  int badge_size_in_dip_;

  // Maps from a shortcut id to the icon to track the icons added via
  // FetchImage.
  ShortcutIDToIconMap icon_map_;

  base::WeakPtrFactory<AppServiceShortcutIconLoader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_ICON_LOADER_H_
