// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CORE_HOST_FACTORY_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CORE_HOST_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class IpProtectionCoreHost;

// Responsible for managing IP Protection auth token fetching.
class IpProtectionCoreHostFactory : public ProfileKeyedServiceFactory {
 public:
  static IpProtectionCoreHost* GetForProfile(Profile* profile);

  static IpProtectionCoreHostFactory* GetInstance();

  static ProfileSelections CreateProfileSelectionsForTesting() {
    return CreateProfileSelections();
  }

  IpProtectionCoreHostFactory(const IpProtectionCoreHostFactory&) = delete;
  IpProtectionCoreHostFactory& operator=(const IpProtectionCoreHostFactory&) =
      delete;

 private:
  friend base::NoDestructor<IpProtectionCoreHostFactory>;

  static ProfileSelections CreateProfileSelections();

  IpProtectionCoreHostFactory();
  ~IpProtectionCoreHostFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CORE_HOST_FACTORY_H_
