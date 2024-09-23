// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_REGISTRY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_REGISTRY_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_activity_watcher.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_store.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_types.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class PrefService;

namespace ash::on_device_controls {

// Keeps track of blocked apps and persists blocked apps on the disk.
// TODO(b/338246850): Handle app uninstall/reinstall.
// TODO(b/338247185): Persist blocked apps in a pref.
class BlockedAppRegistry : public apps::AppRegistryCache::Observer {
 public:
  BlockedAppRegistry(apps::AppServiceProxy* app_service,
                     PrefService* pref_service);
  BlockedAppRegistry(const BlockedAppRegistry&) = delete;
  BlockedAppRegistry& operator=(const BlockedAppRegistry&) = delete;
  ~BlockedAppRegistry() override;

  // Adds an app identified with `app_id` to the registry.
  // Mark app as blocked. It will have no effect if the app is already blocked.
  void AddApp(const std::string& app_id);

  // Removes an app identified with an `app_id` from the registry.
  // Marks app as unblocked. Will have no effect if the app is not blocked.
  void RemoveApp(const std::string& app_id);

  // Removes all apps from the registry and marks all blocked apps as unblocked.
  // No effect if the registry is empty.
  void RemoveAllApps();

  // Returns the set with ids for all blocked apps.
  std::set<std::string> GetBlockedApps();

  // Returns the local state of the app based on its presences in the registry
  LocalAppState GetAppState(const std::string& app_id) const;

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  apps::AppRegistryCache& GetAppCache();

  // Called when app is installed and at the beginning of each
  // user session. Restores the blocked state of the apps that are loaded or
  // reinstalled.
  void OnAppReady(const std::string& app_id);
  // Called when app is uninstalled.
  void OnAppUninstalled(const std::string& app_id);
  // Returns the number of blocked apps that are uninstalled.
  int GetUninstalledBlockedAppCount() const;
  // Removes the oldest uninstalled blocked app from the registry.
  void RemoveOldestUninstalledApp();

  // The in-memory registry of the locked apps.
  // Maps blocked app id to blocked ap metadata.
  BlockedAppMap registry_;

  // Manages persisting and restoring blocked apps.
  BlockedAppStore store_;

  const raw_ptr<apps::AppServiceProxy> app_service_;

  // Watches for instances of blocked apps launched from play.
  AppActivityWatcher app_activity_watcher_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_REGISTRY_H_
