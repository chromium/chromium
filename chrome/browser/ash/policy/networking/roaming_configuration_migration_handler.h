// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_NETWORKING_ROAMING_CONFIGURATION_MIGRATION_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_NETWORKING_ROAMING_CONFIGURATION_MIGRATION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/networking/network_roaming_state_migration_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

namespace ash {
class OwnerSettingsServiceAsh;
}  // namespace ash

namespace policy {

// This class is responsible for keeping the
// |chromeos::kSignedDataRoamingEnabled| setting consistent with how a user had
// configured cellular roaming for cellular networks on a device. This class
// works by checking each cellular network as it is available, and caching if at
// least one of the seen networks has roaming disabled. The cached value is
// propagated if the profile active at the time of finding a cellular network
// with roaming disabled belongs to the device owner, or if the first time the
// user logs in it is with the device owner account. The value will fail to be
// propagated if the user first logs in to any account other than the device
// owner account.
// TODO(crbug.com/1232818): Remove when per-network cellular roaming is fully
// launched.
class RoamingConfigurationMigrationHandler
    : public NetworkRoamingStateMigrationHandler::Observer,
      public ProfileManagerObserver {
 public:
  explicit RoamingConfigurationMigrationHandler(
      NetworkRoamingStateMigrationHandler*
          network_roaming_state_migration_handler);
  ~RoamingConfigurationMigrationHandler() override;

 private:
  // NetworkRoamingStateMigrationHandler::Observer
  void OnFoundCellularNetwork(bool roaming_enabled) override;

  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // The cellular roaming setting must be set by the device owner. This
  // registers a callback for the owner settings service of |profile| that will
  // be invoked when the profile is confirmed to be the device owner or not.
  void PropagateRoamingEnabledAsync(Profile* profile);

  // This uses |found_network_with_disabled_roaming_| to determine a new value
  // for the cellular roaming setting of |service| when |is_owner| is |true|.
  void PropagateRoamingEnabled(ash::OwnerSettingsServiceAsh* service,
                               bool is_owner);

  // Whether at least one cellular network has been found with roaming disabled.
  bool found_network_with_disabled_roaming_ = false;

  NetworkRoamingStateMigrationHandler* network_roaming_state_migration_handler_;

  base::ScopedObservation<NetworkRoamingStateMigrationHandler,
                          NetworkRoamingStateMigrationHandler::Observer>
      migration_handler_observer_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

  base::WeakPtrFactory<RoamingConfigurationMigrationHandler> weak_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_NETWORKING_ROAMING_CONFIGURATION_MIGRATION_HANDLER_H_
