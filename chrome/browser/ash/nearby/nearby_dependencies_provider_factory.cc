// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"

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

}  // namespace ash::nearby
