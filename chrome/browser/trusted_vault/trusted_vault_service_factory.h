// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

class Profile;

class TrustedVaultServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static trusted_vault::TrustedVaultService* GetForProfile(Profile* profile);

  static TrustedVaultServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<TrustedVaultServiceFactory>;

  TrustedVaultServiceFactory();
  ~TrustedVaultServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_SERVICE_FACTORY_H_
