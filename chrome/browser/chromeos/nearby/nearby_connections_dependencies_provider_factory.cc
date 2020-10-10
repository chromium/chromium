// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider_factory.h"

#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace nearby {

// static
NearbyConnectionsDependenciesProvider*
NearbyConnectionsDependenciesProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<NearbyConnectionsDependenciesProvider*>(
      NearbyConnectionsDependenciesProviderFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
NearbyConnectionsDependenciesProviderFactory*
NearbyConnectionsDependenciesProviderFactory::GetInstance() {
  return base::Singleton<NearbyConnectionsDependenciesProviderFactory>::get();
}

NearbyConnectionsDependenciesProviderFactory::
    NearbyConnectionsDependenciesProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "NearbyConnectionsDependenciesProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

NearbyConnectionsDependenciesProviderFactory::
    ~NearbyConnectionsDependenciesProviderFactory() = default;

KeyedService*
NearbyConnectionsDependenciesProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new NearbyConnectionsDependenciesProvider(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

bool NearbyConnectionsDependenciesProviderFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace nearby
}  // namespace chromeos
