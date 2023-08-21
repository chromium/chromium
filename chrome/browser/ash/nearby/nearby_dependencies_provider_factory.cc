// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace ash::nearby {

namespace {

// This needs to be overridden because the default implementation returns
// nullptr for "Ash internal" profiles (i.e. the signin profile), which would
// prevent using this with Quick Start. We allow this service to be created for
// the OTR signin profile for use with Quick Start, and for the regular user
// profile with all other features. See ProfileSelections and
// NearbyProcessManagerFactory documentation for more detail.
ProfileSelections BuildNearbyDependenciesProviderProfileSelections() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithAshInternals(ProfileSelection::kOffTheRecordOnly)
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
  static base::NoDestructor<NearbyDependenciesProviderFactory> instance;
  return instance.get();
}

NearbyDependenciesProviderFactory::NearbyDependenciesProviderFactory()
    : ProfileKeyedServiceFactory(
          "NearbyDependenciesProvider",
          BuildNearbyDependenciesProviderProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

NearbyDependenciesProviderFactory::~NearbyDependenciesProviderFactory() =
    default;

std::unique_ptr<KeyedService>
NearbyDependenciesProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<NearbyDependenciesProvider>(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

bool NearbyDependenciesProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash::nearby
