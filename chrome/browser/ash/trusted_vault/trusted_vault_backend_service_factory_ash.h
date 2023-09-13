// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_SERVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_SERVICE_FACTORY_ASH_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ash {

class TrustedVaultBackendServiceAsh;

class TrustedVaultBackendServiceFactoryAsh : public ProfileKeyedServiceFactory {
 public:
  static TrustedVaultBackendServiceAsh* GetForProfile(Profile* profile);
  static TrustedVaultBackendServiceFactoryAsh* GetInstance();

  TrustedVaultBackendServiceFactoryAsh(
      const TrustedVaultBackendServiceFactoryAsh&) = delete;
  TrustedVaultBackendServiceFactoryAsh& operator=(
      const TrustedVaultBackendServiceFactoryAsh&) = delete;

 private:
  friend class base::NoDestructor<TrustedVaultBackendServiceFactoryAsh>;

  TrustedVaultBackendServiceFactoryAsh();
  ~TrustedVaultBackendServiceFactoryAsh() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_SERVICE_FACTORY_ASH_H_
