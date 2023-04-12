// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_appsync_service_factory.h"

#include "base/check_is_test.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/sync/sync_appsync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// static
SyncAppsyncService* SyncAppsyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SyncAppsyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SyncAppsyncServiceFactory* SyncAppsyncServiceFactory::GetInstance() {
  return base::Singleton<SyncAppsyncServiceFactory>::get();
}

SyncAppsyncServiceFactory::SyncAppsyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "SyncAppsyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

SyncAppsyncServiceFactory::~SyncAppsyncServiceFactory() = default;

KeyedService* SyncAppsyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    // Something prevented SyncService from being instantiated (e.g. sync is
    // disabled by command line flag)
    return nullptr;
  }

  if (!user_manager::UserManager::IsInitialized()) {
    // UserManager is not initialized for some tests. Normally, UserManager will
    // be initialized before |this| is created.
    CHECK_IS_TEST();
    return nullptr;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  return new SyncAppsyncService(sync_service, user_manager);
}

}  // namespace ash
