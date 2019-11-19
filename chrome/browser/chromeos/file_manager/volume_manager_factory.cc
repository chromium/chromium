// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/storage_monitor/storage_monitor.h"

namespace file_manager {

VolumeManager* VolumeManagerFactory::Get(content::BrowserContext* context) {
  return static_cast<VolumeManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

VolumeManagerFactory* VolumeManagerFactory::GetInstance() {
  return base::Singleton<VolumeManagerFactory>::get();
}

content::BrowserContext* VolumeManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Explicitly allow this manager in guest login mode.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
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
      chromeos::disks::DiskMountManager::GetInstance(),
      chromeos::file_system_provider::ServiceFactory::Get(context),
      VolumeManager::GetMtpStorageInfoCallback());
  instance->Initialize();
  return instance;
}

VolumeManagerFactory::VolumeManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "VolumeManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(chromeos::file_system_provider::ServiceFactory::GetInstance());
}

VolumeManagerFactory::~VolumeManagerFactory() = default;

}  // namespace file_manager
