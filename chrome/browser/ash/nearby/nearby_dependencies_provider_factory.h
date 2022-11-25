// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_NEARBY_DEPENDENCIES_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_ASH_NEARBY_NEARBY_DEPENDENCIES_PROVIDER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace ash::nearby {

class NearbyDependenciesProvider;

class NearbyDependenciesProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NearbyDependenciesProvider* GetForProfile(Profile* profile);

  static NearbyDependenciesProviderFactory* GetInstance();

  NearbyDependenciesProviderFactory(const NearbyDependenciesProviderFactory&) =
      delete;
  NearbyDependenciesProviderFactory& operator=(
      const NearbyDependenciesProviderFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<NearbyDependenciesProviderFactory>;

  NearbyDependenciesProviderFactory();
  ~NearbyDependenciesProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash::nearby

#endif  // CHROME_BROWSER_ASH_NEARBY_NEARBY_DEPENDENCIES_PROVIDER_FACTORY_H_
