// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/profile_password_store_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/factories/credentials_cleaner_runner_factory.h"
#include "chrome/browser/password_manager/factories/password_store_backend_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "content/public/browser/storage_partition.h"

using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStore;
using password_manager::PasswordStoreInterface;

namespace {

network::mojom::NetworkContext* GetNetworkContext(Profile* profile) {
  return g_browser_process->profile_manager()->IsValidProfile(profile)
             ? profile->GetDefaultStoragePartition()->GetNetworkContext()
             : nullptr;
}

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());
  scoped_refptr<PasswordStore> ps =
      new password_manager::PasswordStore(CreatePasswordStoreBackend(
          password_manager::kProfileStore, profile->GetPath(),
          profile->GetPrefs(), g_browser_process->os_crypt_async()));
  affiliations::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);
  ps->Init(std::make_unique<AffiliatedMatchHelper>(affiliation_service));
  password_manager::SanitizeAndMigrateCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), ps,
      password_manager::kProfileStore, profile->GetPrefs(), base::Seconds(60),
      base::BindRepeating(&GetNetworkContext, profile));
#if !BUILDFLAG(IS_ANDROID)
  // Android gets logins with affiliations directly from the backend.
  auto password_affiliation_adapter =
      std::make_unique<password_manager::PasswordAffiliationSourceAdapter>();
  password_affiliation_adapter->RegisterPasswordStore(ps.get());
  affiliation_service->RegisterSource(std::move(password_affiliation_adapter));
#endif
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
          "ProfilePasswordStore",
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
