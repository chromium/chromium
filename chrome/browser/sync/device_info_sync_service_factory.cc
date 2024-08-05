// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/device_info_sync_service_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_client_impl.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_sync_service_impl.h"
#include "components/sync_device_info/local_device_info_provider_impl.h"

// static
syncer::DeviceInfoSyncService* DeviceInfoSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::DeviceInfoSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
DeviceInfoSyncServiceFactory* DeviceInfoSyncServiceFactory::GetInstance() {
  static base::NoDestructor<DeviceInfoSyncServiceFactory> instance;
  return instance.get();
}

// static
void DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers(
    std::vector<const syncer::DeviceInfoTracker*>* trackers) {
  DCHECK(trackers);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
  for (Profile* profile : profile_list) {
    syncer::DeviceInfoSyncService* service =
        DeviceInfoSyncServiceFactory::GetForProfile(profile);
    if (service != nullptr) {
      const syncer::DeviceInfoTracker* tracker =
          service->GetDeviceInfoTracker();
      if (tracker != nullptr) {
        // Even when sync is disabled and/or user is signed out, a tracker will
        // still be present.
        trackers->push_back(tracker);
      }
    }
  }
}

DeviceInfoSyncServiceFactory::DeviceInfoSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "DeviceInfoSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
}

DeviceInfoSyncServiceFactory::~DeviceInfoSyncServiceFactory() = default;

KeyedService* DeviceInfoSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto device_info_sync_client =
      std::make_unique<browser_sync::DeviceInfoSyncClientImpl>(profile);
  auto local_device_info_provider =
      std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
          chrome::GetChannel(),
          chrome::GetVersionString(chrome::WithExtendedStable(false)),
          device_info_sync_client.get());

  auto device_prefs = std::make_unique<syncer::DeviceInfoPrefs>(
      profile->GetPrefs(), base::DefaultClock::GetInstance());

  return new syncer::DeviceInfoSyncServiceImpl(
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs),
      std::move(device_info_sync_client),
      SyncInvalidationsServiceFactory::GetForProfile(profile));
}
