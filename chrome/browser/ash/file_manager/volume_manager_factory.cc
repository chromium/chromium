// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/volume_manager_factory.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/storage_monitor/storage_monitor.h"

namespace file_manager {

VolumeManager* VolumeManagerFactory::Get(content::BrowserContext* context) {
  return static_cast<VolumeManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

VolumeManagerFactory* VolumeManagerFactory::GetInstance() {
  static base::NoDestructor<VolumeManagerFactory> instance;
  return instance.get();
}

bool VolumeManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool VolumeManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
VolumeManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  std::unique_ptr<VolumeManager> instance = std::make_unique<VolumeManager>(
      profile, drive::DriveIntegrationServiceFactory::GetForProfile(profile),
      chromeos::PowerManagerClient::Get(),
      ash::disks::DiskMountManager::GetInstance(),
      ash::file_system_provider::ServiceFactory::Get(context),
      VolumeManager::GetMtpStorageInfoCallback());
  instance->Initialize();
  return instance;
}

VolumeManagerFactory::VolumeManagerFactory()
    : ProfileKeyedServiceFactory(
          "VolumeManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(ash::file_system_provider::ServiceFactory::GetInstance());
  if (base::FeatureList::IsEnabled(features::kSkyVaultV2)) {
    DependsOn(policy::local_user_files::LocalFilesMigrationManagerFactory::
                  GetInstance());
  }
}

VolumeManagerFactory::~VolumeManagerFactory() = default;

}  // namespace file_manager
