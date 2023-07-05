// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_FACTORY_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ip_protection/ip_protection_auth_token_getter.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class IpProtectionAuthTokenGetter;

// Responsible for managing IP Protection auth token fetching.
class IpProtectionAuthTokenGetterFactory : public ProfileKeyedServiceFactory {
 public:
  static IpProtectionAuthTokenGetter* GetForProfile(Profile* profile);

  static IpProtectionAuthTokenGetterFactory* GetInstance();

  static ProfileSelections CreateProfileSelectionsForTesting() {
    return CreateProfileSelections();
  }

  IpProtectionAuthTokenGetterFactory(
      const IpProtectionAuthTokenGetterFactory&) = delete;
  IpProtectionAuthTokenGetterFactory& operator=(
      const IpProtectionAuthTokenGetterFactory&) = delete;

 private:
  friend base::NoDestructor<IpProtectionAuthTokenGetterFactory>;

  static ProfileSelections CreateProfileSelections();

  IpProtectionAuthTokenGetterFactory();
  ~IpProtectionAuthTokenGetterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_FACTORY_H_
