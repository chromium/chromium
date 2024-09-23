// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_invalidations_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/sync/invalidations/sync_invalidations_service_impl.h"

syncer::SyncInvalidationsService*
SyncInvalidationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<syncer::SyncInvalidationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

SyncInvalidationsServiceFactory*
SyncInvalidationsServiceFactory::GetInstance() {
  static base::NoDestructor<SyncInvalidationsServiceFactory> instance;
  return instance.get();
}

SyncInvalidationsServiceFactory::SyncInvalidationsServiceFactory()
    : ProfileKeyedServiceFactory(
          "SyncInvalidationsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
}

SyncInvalidationsServiceFactory::~SyncInvalidationsServiceFactory() = default;

KeyedService* SyncInvalidationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  return new syncer::SyncInvalidationsServiceImpl(gcm_driver,
                                                  instance_id_driver);
}
