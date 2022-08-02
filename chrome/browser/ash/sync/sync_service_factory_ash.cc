// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_service_factory_ash.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/sync/sync_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace ash {

// static
SyncServiceAsh* SyncServiceFactoryAsh::GetForProfile(Profile* profile) {
  return static_cast<SyncServiceAsh*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SyncServiceFactoryAsh* SyncServiceFactoryAsh::GetInstance() {
  return base::Singleton<SyncServiceFactoryAsh>::get();
}

SyncServiceFactoryAsh::SyncServiceFactoryAsh()
    : ProfileKeyedServiceFactory("SyncServiceAsh") {
  DependsOn(SyncServiceFactory::GetInstance());
}

SyncServiceFactoryAsh::~SyncServiceFactoryAsh() = default;

KeyedService* SyncServiceFactoryAsh::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    // Something prevented SyncService from being instantiated (e.g. sync is
    // disabled by command line flag).
    return nullptr;
  }

  return new SyncServiceAsh(sync_service);
}

}  // namespace ash
