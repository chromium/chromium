// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/profile_password_store_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"
#include "chrome/browser/password_manager/password_store_backend_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStore;
using password_manager::PasswordStoreInterface;

namespace {

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  if (!password_manager_android_util::IsInternalBackendPresent()) {
    LOG(ERROR)
        << "Password store is not supported: use_login_database_as_backend is "
           "false when Chrome's internal backend is not present. Please, set "
           "use_login_database_as_backend=true in the args.gn file to enable "
           "Chrome password store.";
    return nullptr;
  }
#endif

  Profile* profile = Profile::FromBrowserContext(context);

  DCHECK(!profile->IsOffTheRecord());

  std::unique_ptr<password_manager::PasswordAffiliationSourceAdapter>
      password_affiliation_adapter = std::make_unique<
          password_manager::PasswordAffiliationSourceAdapter>();

  scoped_refptr<PasswordStore> ps;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_OZONE)
  os_crypt_async::OSCryptAsync* os_crypt_async =
      base::FeatureList::IsEnabled(
          password_manager::features::kUseAsyncOsCryptInLoginDatabase)
          ? g_browser_process->os_crypt_async()
          : nullptr;

  ps = new password_manager::PasswordStore(CreateProfilePasswordStoreBackend(
      profile->GetPath(), profile->GetPrefs(), *password_affiliation_adapter,
      os_crypt_async));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);

  affiliations::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  ps->Init(profile->GetPrefs(), std::move(affiliated_match_helper));

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile)) {
          return nullptr;
        }
        return profile->GetDefaultStoragePartition()->GetNetworkContext();
      },
      profile);
  password_manager::SanitizeAndMigrateCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), ps,
      password_manager::kProfileStore, profile->GetPrefs(), base::Seconds(60),
      network_context_getter);

  password_affiliation_adapter->RegisterPasswordStore(ps.get());
  affiliation_service->RegisterSource(std::move(password_affiliation_adapter));

  DelayReportingPasswordStoreMetrics(profile);

  return ps;
}

}  // namespace

// static
scoped_refptr<PasswordStoreInterface>
ProfilePasswordStoreFactory::GetForProfile(Profile* profile,
                                           ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(static_cast<PasswordStoreInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
bool ProfilePasswordStoreFactory::HasStore(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(
             profile, /*create=*/false) != nullptr;
}

// static
ProfilePasswordStoreFactory* ProfilePasswordStoreFactory::GetInstance() {
  static base::NoDestructor<ProfilePasswordStoreFactory> instance;
  return instance.get();
}

ProfilePasswordStoreFactory::ProfilePasswordStoreFactory()
    : RefcountedProfileKeyedServiceFactory(
          "PasswordStore",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
}

ProfilePasswordStoreFactory::~ProfilePasswordStoreFactory() = default;

ProfilePasswordStoreFactory::TestingFactory
ProfilePasswordStoreFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildPasswordStore);
}

scoped_refptr<RefcountedKeyedService>
ProfilePasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildPasswordStore(context);
}

bool ProfilePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
