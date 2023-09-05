// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ip_protection/ip_protection_config_provider.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class IpProtectionConfigProvider;

// Responsible for managing IP Protection auth token fetching.
class IpProtectionConfigProviderFactory : public ProfileKeyedServiceFactory {
 public:
  static IpProtectionConfigProvider* GetForProfile(Profile* profile);

  static IpProtectionConfigProviderFactory* GetInstance();

  static ProfileSelections CreateProfileSelectionsForTesting() {
    return CreateProfileSelections();
  }

  IpProtectionConfigProviderFactory(const IpProtectionConfigProviderFactory&) =
      delete;
  IpProtectionConfigProviderFactory& operator=(
      const IpProtectionConfigProviderFactory&) = delete;

 private:
  friend base::NoDestructor<IpProtectionConfigProviderFactory>;

  static ProfileSelections CreateProfileSelections();

  IpProtectionConfigProviderFactory();
  ~IpProtectionConfigProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_FACTORY_H_
