// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/quick_start_connectivity_service_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"

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
  static base::NoDestructor<QuickStartConnectivityServiceFactory> instance;
  return instance.get();
}

QuickStartConnectivityServiceFactory::QuickStartConnectivityServiceFactory()
    : ProfileKeyedServiceFactory(
          "QuickStartConnectivityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(nearby::NearbyProcessManagerFactory::GetInstance());
}

QuickStartConnectivityServiceFactory::~QuickStartConnectivityServiceFactory() =
    default;

std::unique_ptr<KeyedService>
QuickStartConnectivityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!features::IsCrossDeviceFeatureSuiteAllowed()) {
    return nullptr;
  }

  if (!ash::IsSigninBrowserContext(context)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  // The "signin profile" is actually a pair of profiles: a "regular" profile
  // and a "primary OTR" profile tied to the regular profile. The primary OTR
  // profile is the one that is used in OOBE.
  if (!profile->IsPrimaryOTRProfile()) {
    return nullptr;
  }

  // The NearbyProcessManager* fetched here is bound to the lifetime of the
  // profile and is guaranteed to outlive QuickStartConnectivityServiceImpl.
  return std::make_unique<QuickStartConnectivityServiceImpl>(
      nearby::NearbyProcessManagerFactory::GetForProfile(profile));
}

bool QuickStartConnectivityServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash::quick_start
