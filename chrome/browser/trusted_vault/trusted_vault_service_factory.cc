// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "device/fido/public/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/android/trusted_vault_client_android.h"
#else
#include "base/files/file_path.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "components/trusted_vault/standalone_trusted_vault_frontend.h"
#include "content/public/browser/storage_partition.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_version.h"
#endif

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kICloudKeychainAccessGroupPrefix[] = MAC_TEAM_IDENTIFIER_STRING;
#endif

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<trusted_vault::TrustedVaultService> CreateTrustedVaultService(
    Profile* profile) {
  TrustedVaultClientAndroid::GetAccountInfoByGaiaIdCallback
      account_info_callback = base::BindRepeating(
          [](signin::IdentityManager* identity_manager,
             const GaiaId& gaia_id) -> CoreAccountInfo {
            return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id)
                .GetCoreAccountInfo();
          },
          IdentityManagerFactory::GetForProfile(profile));

  return std::make_unique<trusted_vault::TrustedVaultService>(
      std::make_unique<TrustedVaultClientAndroid>(
          trusted_vault::SecurityDomainId::kChromeSync, account_info_callback),
      std::make_unique<TrustedVaultClientAndroid>(
          trusted_vault::SecurityDomainId::kPasskeys, account_info_callback));
}
#else
std::unique_ptr<trusted_vault::TrustedVaultService> CreateTrustedVaultService(
    Profile* profile) {
  auto frontend =
      base::MakeRefCounted<trusted_vault::StandaloneTrustedVaultFrontend>(
#if BUILDFLAG(IS_MAC)
          kICloudKeychainAccessGroupPrefix,
#endif
          /*base_dir=*/profile->GetPath(),
          IdentityManagerFactory::GetForProfile(profile),
          profile->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess());

  auto chrome_sync_client =
      std::make_unique<trusted_vault::StandaloneTrustedVaultClient>(
          trusted_vault::SecurityDomainId::kChromeSync, frontend);

  return std::make_unique<trusted_vault::TrustedVaultService>(
      std::move(chrome_sync_client),
      /*passkeys_security_domain_client=*/nullptr);
}
#endif

std::unique_ptr<KeyedService> BuildTrustedVaultService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(!profile->IsOffTheRecord());
  return CreateTrustedVaultService(profile);
}

}  // namespace

// static
trusted_vault::TrustedVaultService* TrustedVaultServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<trusted_vault::TrustedVaultService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
TrustedVaultServiceFactory* TrustedVaultServiceFactory::GetInstance() {
  static base::NoDestructor<TrustedVaultServiceFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
TrustedVaultServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTrustedVaultService);
}

TrustedVaultServiceFactory::TrustedVaultServiceFactory()
    : ProfileKeyedServiceFactory(
          "TrustedVaultService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode. Currently it is required due to dependent services
              // (e.g. SyncService) that have similar TODO, if they stop being
              // used in Guest mode, this service could stop to be used as well.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

TrustedVaultServiceFactory::~TrustedVaultServiceFactory() = default;

std::unique_ptr<KeyedService>
TrustedVaultServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildTrustedVaultService(context);
}

bool TrustedVaultServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
