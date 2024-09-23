// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/data_migration/data_migration_factory.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/components/data_migration/data_migration.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"

namespace data_migration {

// static
DataMigrationFactory* DataMigrationFactory::GetInstance() {
  static base::NoDestructor<DataMigrationFactory> instance;
  return instance.get();
}

DataMigrationFactory::DataMigrationFactory()
    : ProfileKeyedServiceFactory(
          "DataMigration",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ash::nearby::NearbyProcessManagerFactory::GetInstance());
}

DataMigrationFactory::~DataMigrationFactory() = default;

std::unique_ptr<KeyedService>
DataMigrationFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!chromeos::features::IsDataMigrationEnabled()) {
    VLOG(4) << "DataMigration feature disabled";
    return nullptr;
  }

  // For now, only a simple normal logged in user can run data migration. Guest
  // mode, incognito, OOBE, and other "special" profiles are not supported yet.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!ash::IsUserBrowserContext(context) || !profile ||
      !profile->IsRegularProfile()) {
    VLOG(4) << "DataMigration does not apply to requested profile";
    return nullptr;
  }

  ash::nearby::NearbyProcessManager* process_manager =
      ash::nearby::NearbyProcessManagerFactory::GetForProfile(profile);
  if (!process_manager) {
    // Can legitimately happen for secondary-user profiles. See
    // `NearbyProcessManagerFactory::CanBeLaunchedForProfile()`.
    VLOG(4) << "Nearby process manager not available for secondary profile";
    return nullptr;
  }

  VLOG(1) << "Creating DataMigration service";
  auto data_migration = std::make_unique<DataMigration>(
      std::make_unique<NearbyConnectionsManagerImpl>(process_manager,
                                                     kServiceId));
  data_migration->StartAdvertising();
  return data_migration;
}

// TODO(esum): Wait for user to launch the data migration UI and remove this
// method. This causes
// `DataMigrationFactory::BuildServiceInstanceForBrowserContext()`
// to be called as soon as the user logs in. (i.e. data migration starts
// immediately at log-in).
bool DataMigrationFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace data_migration
