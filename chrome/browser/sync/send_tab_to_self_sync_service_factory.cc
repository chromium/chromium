// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"

// static
send_tab_to_self::SendTabToSelfSyncService*
SendTabToSelfSyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<send_tab_to_self::SendTabToSelfSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SendTabToSelfSyncServiceFactory*
SendTabToSelfSyncServiceFactory::GetInstance() {
  static base::NoDestructor<SendTabToSelfSyncServiceFactory> instance;
  return instance.get();
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "SendTabToSelfSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

SendTabToSelfSyncServiceFactory::~SendTabToSelfSyncServiceFactory() = default;

KeyedService* SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();

  return new send_tab_to_self::SendTabToSelfSyncService(
      chrome::GetChannel(), std::move(store_factory), history_service,
      profile->GetPrefs(), device_info_tracker);
}
