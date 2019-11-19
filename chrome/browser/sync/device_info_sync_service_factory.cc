// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/device_info_sync_service_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/features.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/sync_device_info/device_info_sync_service_impl.h"
#include "components/sync_device_info/local_device_info_provider_impl.h"

namespace {

class DeviceInfoSyncClient : public syncer::DeviceInfoSyncClient {
 public:
  explicit DeviceInfoSyncClient(Profile* profile) : profile_(profile) {}
  ~DeviceInfoSyncClient() override = default;

  // syncer::DeviceInfoSyncClient:
  std::string GetSigninScopedDeviceId() const override {
// Since the local sync backend is currently only supported on Windows don't
// even check the pref on other os-es.
#if defined(OS_WIN)
    syncer::SyncPrefs prefs(profile_->GetPrefs());
    if (prefs.IsLocalSyncEnabled()) {
      return "local_device";
    }
#endif  // defined(OS_WIN)

    return GetSigninScopedDeviceIdForProfile(profile_);
  }

  // syncer::DeviceInfoSyncClient:
  bool GetSendTabToSelfReceivingEnabled() const override {
    return send_tab_to_self::IsReceivingEnabledByUserOnThisDevice(
        profile_->GetPrefs());
  }

  // syncer::DeviceInfoSyncClient:
  base::Optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override {
    return SharingSyncPreference::GetLocalSharingInfoForSync(
        profile_->GetPrefs());
  }

 private:
  Profile* const profile_;
};

}  // namespace

// static
syncer::DeviceInfoSyncService* DeviceInfoSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::DeviceInfoSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DeviceInfoSyncServiceFactory* DeviceInfoSyncServiceFactory::GetInstance() {
  return base::Singleton<DeviceInfoSyncServiceFactory>::get();
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
    : BrowserContextKeyedServiceFactory(
          "DeviceInfoSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

DeviceInfoSyncServiceFactory::~DeviceInfoSyncServiceFactory() {}

KeyedService* DeviceInfoSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto device_info_sync_client =
      std::make_unique<DeviceInfoSyncClient>(profile);
  auto local_device_info_provider =
      std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
          chrome::GetChannel(), chrome::GetVersionString(),
          device_info_sync_client.get());
  auto device_prefs = std::make_unique<syncer::DeviceInfoPrefs>(
      profile->GetPrefs(), base::DefaultClock::GetInstance());

  return new syncer::DeviceInfoSyncServiceImpl(
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs),
      std::move(device_info_sync_client));
}
