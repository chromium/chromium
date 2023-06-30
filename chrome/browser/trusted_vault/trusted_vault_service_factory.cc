// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/trusted_vault/trusted_vault_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/trusted_vault/trusted_vault_client_android.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#else
#include "base/files/file_path.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "content/public/browser/storage_partition.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<trusted_vault::TrustedVaultClient> CreateTrustedVaultClient(
    Profile* profile) {
  return std::make_unique<
      TrustedVaultClientAndroid>(/*gaia_account_info_by_gaia_id_cb=*/
                                 base::BindRepeating(
                                     [](signin::IdentityManager*
                                            identity_manager,
                                        const std::string& gaia_id)
                                         -> CoreAccountInfo {
                                       return identity_manager
                                           ->FindExtendedAccountInfoByGaiaId(
                                               gaia_id);
                                     },
                                     IdentityManagerFactory::GetForProfile(
                                         profile)));
}
#else   // !BUILDFLAG(IS_ANDROID)
constexpr base::FilePath::CharType kTrustedVaultFilename[] =
    FILE_PATH_LITERAL("trusted_vault.pb");
constexpr base::FilePath::CharType kDeprecatedTrustedVaultFilename[] =
    FILE_PATH_LITERAL("Trusted Vault");

std::unique_ptr<trusted_vault::TrustedVaultClient> CreateTrustedVaultClient(
    Profile* profile) {
  const base::FilePath profile_path = profile->GetPath();
  return std::make_unique<trusted_vault::StandaloneTrustedVaultClient>(
      profile_path.Append(kTrustedVaultFilename),
      profile_path.Append(kDeprecatedTrustedVaultFilename),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::unique_ptr<KeyedService> BuildTrustedVaultService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(!profile->IsOffTheRecord());

  return std::make_unique<trusted_vault::TrustedVaultService>(
      CreateTrustedVaultClient(profile));
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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode. Currently it is required due to dependant services
              // (e.g. SyncService) that have similar TODO, if they stop being
              // used in Guest mode, this service could stop to be used as well.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

TrustedVaultServiceFactory::~TrustedVaultServiceFactory() = default;

KeyedService* TrustedVaultServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildTrustedVaultService(context).release();
}

bool TrustedVaultServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
