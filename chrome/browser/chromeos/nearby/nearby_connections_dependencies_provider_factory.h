// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NEARBY_NEARBY_CONNECTIONS_DEPENDENCIES_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_NEARBY_NEARBY_CONNECTIONS_DEPENDENCIES_PROVIDER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {
namespace nearby {

class NearbyConnectionsDependenciesProvider;

class NearbyConnectionsDependenciesProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NearbyConnectionsDependenciesProvider* GetForProfile(Profile* profile);

  static NearbyConnectionsDependenciesProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      NearbyConnectionsDependenciesProviderFactory>;

  NearbyConnectionsDependenciesProviderFactory();
  NearbyConnectionsDependenciesProviderFactory(
      const NearbyConnectionsDependenciesProviderFactory&) = delete;
  NearbyConnectionsDependenciesProviderFactory& operator=(
      const NearbyConnectionsDependenciesProviderFactory&) = delete;
  ~NearbyConnectionsDependenciesProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace nearby
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NEARBY_NEARBY_CONNECTIONS_DEPENDENCIES_PROVIDER_FACTORY_H_
