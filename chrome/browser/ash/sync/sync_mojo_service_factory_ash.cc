// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_mojo_service_factory_ash.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/sync/sync_mojo_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace ash {

// static
SyncMojoServiceAsh* SyncMojoServiceFactoryAsh::GetForProfile(Profile* profile) {
  return static_cast<SyncMojoServiceAsh*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SyncMojoServiceFactoryAsh* SyncMojoServiceFactoryAsh::GetInstance() {
  return base::Singleton<SyncMojoServiceFactoryAsh>::get();
}

SyncMojoServiceFactoryAsh::SyncMojoServiceFactoryAsh()
    : ProfileKeyedServiceFactory(
          "SyncMojoServiceAsh",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

SyncMojoServiceFactoryAsh::~SyncMojoServiceFactoryAsh() = default;

std::unique_ptr<KeyedService>
SyncMojoServiceFactoryAsh::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    // Something prevented SyncService from being instantiated (e.g. sync is
    // disabled by command line flag).
    return nullptr;
  }

  return std::make_unique<SyncMojoServiceAsh>(sync_service);
}

}  // namespace ash
