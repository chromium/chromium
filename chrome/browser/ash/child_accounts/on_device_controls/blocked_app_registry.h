// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_REGISTRY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_REGISTRY_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::on_device_controls {

// State of the app
enum class LocalAppState {
  // App is not blocked by on device controls.
  kAvailable = 0,
  // App installed and blocked by on device controls.
  kBlocked = 1,
  // App uninstalled and blocked by on device controls.
  // Used to block the app upon reinstallation.
  kBlockedUninstalled = 2,
};

// Keeps track of blocked apps and persists blocked apps on the disk.
// TODO(b/338246850): Handle app uninstall/reinstall.
// TODO(b/338247185): Persist blocked apps in a pref.
class BlockedAppRegistry : public apps::AppRegistryCache::Observer {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

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

  // Returns the set with ids for all blocked apps.
  std::set<std::string> GetBlockedApps();

  // Returns the local state of the app based on its presences in the registry
  LocalAppState GetAppState(const std::string& app_id) const;

 private:
  struct AppDetails {
    AppDetails();
    explicit AppDetails(base::TimeTicks block_timestamp);
    ~AppDetails();

    // The timestamp when the app was uninstalled..
    // If not populated app is currently installed.
    std::optional<base::TimeTicks> uninstall_timestamp;

    // The timestamp when the app was blocked.
    base::TimeTicks block_timestamp;
  };

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

  // The in-memory registry of the locked apps.
  // Maps blocked app id to blocked ap metadata.
  std::map<std::string, AppDetails> registry_;

  const raw_ptr<apps::AppServiceProxy> app_service_;

  const raw_ptr<PrefService> pref_service_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_REGISTRY_H_
