// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace ash::nearby {

namespace {

ProfileSelections BuildNearbyDependenciesProviderProfileSelections() {
  // This needs to be overridden because the default implementation returns
  // nullptr for OTR profiles, which would prevent using this with Quick Start.
  if (features::IsOobeQuickStartEnabled()) {
    return ProfileSelections::Builder()
        .WithRegular(ProfileSelection::kOwnInstance)
        // TODO(crbug.com/1418376): Check if this service is needed in
        // Guest mode.
        .WithGuest(ProfileSelection::kOwnInstance)
        .Build();
  }

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      // TODO(crbug.com/1418376): Check if this service is needed in Guest mode.
      .WithGuest(ProfileSelection::kOriginalOnly)
      .Build();
}

}  // namespace

// static
NearbyDependenciesProvider* NearbyDependenciesProviderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NearbyDependenciesProvider*>(
      NearbyDependenciesProviderFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
NearbyDependenciesProviderFactory*
NearbyDependenciesProviderFactory::GetInstance() {
  return base::Singleton<NearbyDependenciesProviderFactory>::get();
}

NearbyDependenciesProviderFactory::NearbyDependenciesProviderFactory()
    : ProfileKeyedServiceFactory(
          "NearbyDependenciesProvider",
          BuildNearbyDependenciesProviderProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

NearbyDependenciesProviderFactory::~NearbyDependenciesProviderFactory() =
    default;

KeyedService* NearbyDependenciesProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new NearbyDependenciesProvider(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

bool NearbyDependenciesProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash::nearby
