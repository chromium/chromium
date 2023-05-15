// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/volume_manager_factory.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/storage_monitor/storage_monitor.h"

namespace file_manager {

VolumeManager* VolumeManagerFactory::Get(content::BrowserContext* context) {
  return static_cast<VolumeManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

VolumeManagerFactory* VolumeManagerFactory::GetInstance() {
  return base::Singleton<VolumeManagerFactory>::get();
}

bool VolumeManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool VolumeManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* VolumeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  VolumeManager* instance = new VolumeManager(
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
          // Explicitly allow this manager in guest login mode.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(ash::file_system_provider::ServiceFactory::GetInstance());
}

VolumeManagerFactory::~VolumeManagerFactory() = default;

}  // namespace file_manager
