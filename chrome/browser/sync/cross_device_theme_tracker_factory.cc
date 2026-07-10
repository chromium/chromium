// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/cross_device_theme_tracker_factory.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/themes/cross_device/cross_device_theme_sync_bridge.h"
#include "components/themes/cross_device/theme_translation.h"

namespace {

template <typename RemoteSpecifics, typename LocalSpecifics>
void RegisterBridgeHelper(
    syncer::DataType type,
    base::RepeatingCallback<themes::DeviceThemeInfo<LocalSpecifics>(
        const RemoteSpecifics&)> translate_cb,
    themes::CrossDeviceThemeTracker<LocalSpecifics>* tracker,
    syncer::DataTypeStoreService* store_service,
    version_info::Channel channel) {
  syncer::RepeatingDataTypeStoreFactory store_factory =
      store_service->GetStoreFactory();
  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      type, base::BindRepeating(&syncer::ReportUnrecoverableError, channel));

  auto bridge = std::make_unique<
      themes::CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>>(
      type, std::move(translate_cb),
      base::BindRepeating(
          &themes::CrossDeviceThemeTracker<LocalSpecifics>::UpdateThemeInfo,
          base::Unretained(tracker)),
      base::BindRepeating(
          &themes::CrossDeviceThemeTracker<LocalSpecifics>::RemoveThemeInfo,
          base::Unretained(tracker)),
      base::BindRepeating(
          &themes::CrossDeviceThemeTracker<LocalSpecifics>::OnBridgeSyncStarted,
          base::Unretained(tracker), type),
      base::BindRepeating(&themes::CrossDeviceThemeTracker<
                              LocalSpecifics>::OnBridgeSyncDisabled,
                          base::Unretained(tracker), type),
      std::move(processor), store_factory);

  tracker->RegisterBridge(type, std::move(bridge));
}

}  // namespace

// static
themes::CrossDeviceThemeTracker<
    CrossDeviceThemeTrackerFactory::LocalThemeSpecifics>*
CrossDeviceThemeTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<themes::CrossDeviceThemeTracker<LocalThemeSpecifics>*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrossDeviceThemeTrackerFactory* CrossDeviceThemeTrackerFactory::GetInstance() {
  static base::NoDestructor<CrossDeviceThemeTrackerFactory> instance;
  return instance.get();
}

CrossDeviceThemeTrackerFactory::CrossDeviceThemeTrackerFactory()
    : ProfileKeyedServiceFactory(
          "CrossDeviceThemeTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

CrossDeviceThemeTrackerFactory::~CrossDeviceThemeTrackerFactory() = default;

std::unique_ptr<KeyedService>
CrossDeviceThemeTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service
          ? device_info_sync_service->GetDeviceInfoTracker()
          : nullptr;

  syncer::DataTypeStoreService* store_service =
      DataTypeStoreServiceFactory::GetForProfile(profile);
  version_info::Channel channel = chrome::GetChannel();

  auto tracker =
      std::make_unique<themes::CrossDeviceThemeTracker<LocalThemeSpecifics>>(
          device_info_tracker);

  // Construct Android bridge.
  RegisterBridgeHelper<sync_pb::ThemeAndroidSpecifics, LocalThemeSpecifics>(
      syncer::THEMES_ANDROID, base::BindRepeating(&themes::TranslateAndroid),
      tracker.get(), store_service, channel);

  // Construct iOS bridge.
  RegisterBridgeHelper<sync_pb::ThemeIosSpecifics, LocalThemeSpecifics>(
      syncer::THEMES_IOS, base::BindRepeating(&themes::TranslateIos),
      tracker.get(), store_service, channel);

  return tracker;
}
