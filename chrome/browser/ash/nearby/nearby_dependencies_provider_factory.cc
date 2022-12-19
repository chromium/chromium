// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash::nearby {

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
    : BrowserContextKeyedServiceFactory(
          "NearbyDependenciesProvider",
          BrowserContextDependencyManager::GetInstance()) {
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

// This needs to be overridden because the default implementation returns
// nullptr for OTR profiles, which would prevent using this with Quick Start.
content::BrowserContext*
NearbyDependenciesProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (features::IsOobeQuickStartEnabled()) {
    return context;
  } else {
    return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
  }
}

}  // namespace ash::nearby
