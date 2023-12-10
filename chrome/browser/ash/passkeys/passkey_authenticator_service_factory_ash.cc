// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/passkeys/passkey_authenticator_service_factory_ash.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/passkeys/passkey_authenticator_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/storage_partition.h"
#include "device/fido/features.h"

namespace ash {

PasskeyAuthenticatorServiceFactoryAsh*
PasskeyAuthenticatorServiceFactoryAsh::GetInstance() {
  static base::NoDestructor<PasskeyAuthenticatorServiceFactoryAsh> instance;
  return instance.get();
}

PasskeyAuthenticatorServiceAsh*
PasskeyAuthenticatorServiceFactoryAsh::GetForProfile(Profile* profile) {
  return static_cast<PasskeyAuthenticatorServiceAsh*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasskeyAuthenticatorServiceFactoryAsh::PasskeyAuthenticatorServiceFactoryAsh()
    : ProfileKeyedServiceFactory(
          "PasskeyAuthenticatorServiceAsh",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(PasskeyModelFactory::GetInstance());
  DependsOn(TrustedVaultServiceFactory::GetInstance());
}

PasskeyAuthenticatorServiceFactoryAsh::
    ~PasskeyAuthenticatorServiceFactoryAsh() = default;

std::unique_ptr<KeyedService>
PasskeyAuthenticatorServiceFactoryAsh::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(device::kChromeOsPasskeys)) {
    return nullptr;
  }
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PasskeyAuthenticatorServiceAsh>(
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      PasskeyModelFactory::GetForProfile(profile),
      TrustedVaultServiceFactory::GetForProfile(profile)->GetTrustedVaultClient(
          trusted_vault::SecurityDomainId::kPasskeys));
}

}  // namespace ash
