// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/affiliations_prefetcher_factory.h"
#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStore;
using password_manager::PasswordStoreInterface;

// static
scoped_refptr<PasswordStoreInterface> PasswordStoreFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord())
    return nullptr;
  return base::WrapRefCounted(static_cast<PasswordStoreInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
PasswordStoreFactory* PasswordStoreFactory::GetInstance() {
  return base::Singleton<PasswordStoreFactory>::get();
}

PasswordStoreFactory::PasswordStoreFactory()
    : RefcountedProfileKeyedServiceFactory(
          "PasswordStore",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(AffiliationsPrefetcherFactory::GetInstance());
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
}

PasswordStoreFactory::~PasswordStoreFactory() = default;

scoped_refptr<RefcountedKeyedService>
PasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  DCHECK(!profile->IsOffTheRecord());

  // Incognito profiles don't have their own password stores. Guest, or system
  // profiles aren't relevant for Password Manager, and no PasswordStore should
  // even be created for those types of profiles.
  if (!profile->IsRegularProfile())
    return nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, there are additional non-interesting profile types (sign-in
  // profile and lockscreen profile).
  if (!ash::ProfileHelper::IsUserProfile(profile))
    return nullptr;
#endif

  scoped_refptr<PasswordStore> ps;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_OZONE)
  // Since SyncService has dependency on PasswordStore keyed service, there
  // are no guarantees that during the construction of the password store
  // about the sync service existence. And hence we cannot directly query the
  // status of password syncing. However, status of password syncing is
  // relevant for migrating passwords from the built-in backend to the Android
  // backend. Since migration does *not* start immediately after start up,
  // SyncService will be propagated to PasswordStoreBackend after the backend
  // creation once SyncService is initialized. Assumption is by the time the
  // migration starts, the sync service will have been created. As a safety
  // mechanism, if the sync service isn't created yet, we proceed as if the
  // user isn't syncing which forces moving the passwords to the Android backend
  // to avoid data loss.
  ps = new password_manager::PasswordStore(
      password_manager::PasswordStoreBackend::Create(profile->GetPath(),
                                                     profile->GetPrefs()));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);

  password_manager::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  ps->Init(profile->GetPrefs(), std::move(affiliated_match_helper));

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile))
          return nullptr;
        return profile->GetDefaultStoragePartition()->GetNetworkContext();
      },
      profile);
  password_manager_util::RemoveUselessCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), ps,
      profile->GetPrefs(), base::Seconds(60), network_context_getter);

  AffiliationsPrefetcherFactory::GetForProfile(profile)->RegisterPasswordStore(
      ps.get());

  DelayReportingPasswordStoreMetrics(profile);

  return ps;
}

bool PasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
