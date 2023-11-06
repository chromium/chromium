// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_factory_ash.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"

namespace ash {

// static
TrustedVaultBackendServiceAsh*
TrustedVaultBackendServiceFactoryAsh::GetForProfile(Profile* profile) {
  return static_cast<TrustedVaultBackendServiceAsh*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
TrustedVaultBackendServiceFactoryAsh*
TrustedVaultBackendServiceFactoryAsh::GetInstance() {
  static base::NoDestructor<TrustedVaultBackendServiceFactoryAsh> instance;
  return instance.get();
}

TrustedVaultBackendServiceFactoryAsh::TrustedVaultBackendServiceFactoryAsh()
    : ProfileKeyedServiceFactory(
          "TrustedVaultBackendServiceAsh",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TrustedVaultServiceFactory::GetInstance());
}

TrustedVaultBackendServiceFactoryAsh::~TrustedVaultBackendServiceFactoryAsh() =
    default;

std::unique_ptr<KeyedService>
TrustedVaultBackendServiceFactoryAsh::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TrustedVaultBackendServiceAsh>(
      IdentityManagerFactory::GetForProfile(profile),
      TrustedVaultServiceFactory::GetForProfile(profile)->GetTrustedVaultClient(
          trusted_vault::SecurityDomainId::kChromeSync));
}

}  // namespace ash
