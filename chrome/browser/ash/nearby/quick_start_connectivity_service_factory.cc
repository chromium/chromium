// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/quick_start_connectivity_service_factory.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/profiles/profile.h"

namespace ash::quick_start {

// static
QuickStartConnectivityService*
QuickStartConnectivityServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<QuickStartConnectivityService*>(
      QuickStartConnectivityServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
QuickStartConnectivityServiceFactory*
QuickStartConnectivityServiceFactory::GetInstance() {
  return base::Singleton<QuickStartConnectivityServiceFactory>::get();
}

QuickStartConnectivityServiceFactory::QuickStartConnectivityServiceFactory()
    : ProfileKeyedServiceFactory(
          "QuickStartConnectivityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(nearby::NearbyProcessManagerFactory::GetInstance());
}

QuickStartConnectivityServiceFactory::~QuickStartConnectivityServiceFactory() =
    default;

KeyedService* QuickStartConnectivityServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // The NearbyProcessManager* fetched here is bound to the lifetime of the
  // profile and is guaranteed to outlive QuickStartConnectivityService.
  return new QuickStartConnectivityService(
      nearby::NearbyProcessManagerFactory::GetForProfile(profile));
}

bool QuickStartConnectivityServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash::quick_start
