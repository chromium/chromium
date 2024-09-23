// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_ACTIVITY_WATCHER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_ACTIVITY_WATCHER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace ash::on_device_controls {

class BlockedAppRegistry;

// A class to watch running apps and prevent blocked ones from running.
class AppActivityWatcher : public apps::InstanceRegistry::Observer {
 public:
  AppActivityWatcher(BlockedAppRegistry* blocked_app_registry,
                     apps::AppServiceProxy* app_service_proxy);
  AppActivityWatcher(const AppActivityWatcher&) = delete;
  AppActivityWatcher& operator=(const AppActivityWatcher&) = delete;
  ~AppActivityWatcher() override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

 private:
  // Owns the instance of this class.
  const raw_ptr<BlockedAppRegistry> blocked_app_registry_;

  // Owned externally and outlives this class.
  const raw_ptr<apps::AppServiceProxy> app_service_proxy_;

  // Instance IDs of running apps that should be blocked.
  std::set<base::UnguessableToken> blocking_instance_ids_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_ACTIVITY_WATCHER_H_
