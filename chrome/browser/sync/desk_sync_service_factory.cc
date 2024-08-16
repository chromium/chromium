// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/desk_sync_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/channel_info.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/sync/model/data_type_store_service.h"

// static
desks_storage::DeskSyncService* DeskSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<desks_storage::DeskSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DeskSyncServiceFactory* DeskSyncServiceFactory::GetInstance() {
  static base::NoDestructor<DeskSyncServiceFactory> instance;
  return instance.get();
}

DeskSyncServiceFactory::DeskSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "DeskSyncService",
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
}

std::unique_ptr<KeyedService>
DeskSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile);

  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  // This instance will be wrapped in a |std::unique_ptr|, owned by
  // |KeyedServiceFactory| and associated with the given browser context.
  return std::make_unique<desks_storage::DeskSyncService>(
      chrome::GetChannel(), std::move(store_factory), account_id);
}
