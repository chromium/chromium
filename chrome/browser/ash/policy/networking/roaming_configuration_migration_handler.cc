// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/roaming_configuration_migration_handler.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/device_event_log/device_event_log.h"

namespace policy {

RoamingConfigurationMigrationHandler::RoamingConfigurationMigrationHandler(
    NetworkRoamingStateMigrationHandler*
        network_roaming_state_migration_handler)
    : network_roaming_state_migration_handler_(
          network_roaming_state_migration_handler) {
  DCHECK(!base::FeatureList::IsEnabled(
      ash::features::kCellularAllowPerNetworkRoaming));
  DCHECK(!chromeos::InstallAttributes::IsInitialized() ||
         !chromeos::InstallAttributes::Get()->IsEnterpriseManaged());
  DCHECK(network_roaming_state_migration_handler_);

  migration_handler_observer_.Observe(network_roaming_state_migration_handler_);
  if (g_browser_process->profile_manager()) {
    profile_manager_observer_.Observe(g_browser_process->profile_manager());
  }
}

RoamingConfigurationMigrationHandler::~RoamingConfigurationMigrationHandler() {}

void RoamingConfigurationMigrationHandler::OnFoundCellularNetwork(
    bool roaming_enabled) {
  if (roaming_enabled || found_network_with_disabled_roaming_) {
    return;
  }
  found_network_with_disabled_roaming_ = true;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile) {
    PropagateRoamingEnabledAsync(profile);
  }
  migration_handler_observer_.Reset();
}

void RoamingConfigurationMigrationHandler::OnProfileAdded(Profile* profile) {
  PropagateRoamingEnabledAsync(profile);
}

void RoamingConfigurationMigrationHandler::OnProfileManagerDestroying() {
  profile_manager_observer_.Reset();
}

void RoamingConfigurationMigrationHandler::PropagateRoamingEnabledAsync(
    Profile* profile) {
  DCHECK(profile);

  ash::OwnerSettingsServiceAsh* service =
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
  if (service) {
    service->IsOwnerAsync(base::BindOnce(
        &RoamingConfigurationMigrationHandler::PropagateRoamingEnabled,
        weak_factory_.GetWeakPtr(), service));
  } else {
    NET_LOG(EVENT) << "Owner settings service unavailable for profile, will "
                      "not migrate cellular roaming configuration.";
  }
}

void RoamingConfigurationMigrationHandler::PropagateRoamingEnabled(
    ash::OwnerSettingsServiceAsh* service,
    bool is_owner) {
  if (!is_owner) {
    NET_LOG(EVENT) << "Cellular roaming configuration can only be migrated "
                      "on the device owner account.";
    return;
  }
  if (!service->SetBoolean(chromeos::kSignedDataRoamingEnabled,
                           !found_network_with_disabled_roaming_)) {
    NET_LOG(ERROR) << "Failed to migrate cellular roaming configuration.";
    return;
  }
  if (found_network_with_disabled_roaming_) {
    profile_manager_observer_.Reset();
  }
}

}  // namespace policy
