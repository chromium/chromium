// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"
#include "chrome/browser/password_manager/password_store_backend_sync_delegate_impl.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
    : RefcountedBrowserContextKeyedServiceFactory(
          "PasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

PasswordStoreFactory::~PasswordStoreFactory() = default;

scoped_refptr<RefcountedKeyedService>
PasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  scoped_refptr<PasswordStore> ps;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || \
    defined(USE_OZONE)
  // Since SyncService has dependency on PasswordStore keyed service, there
  // are no guarantees that during the construction of the password store
  // about the sync service existence. And hence we cannot directly query the
  // status of password syncing. However, status of password syncing is
  // relevant for migrating passwords from the built-in backend to the Android
  // backend. Since migration does *not* start immediately after start up, we
  // inject a repeating callback that queries the sync service. Assumption is
  // by the time the migration starts, the sync service will have been
  // created. As a safety mechanism, if the sync service isn't created yet, we
  // proceed as if the user isn't syncing which forces moving the passwords to
  // the Android backend to avoid data loss.
  ps = new password_manager::PasswordStore(
      password_manager::PasswordStoreBackend::Create(
          profile->GetPath(), profile->GetPrefs(),
          std::make_unique<PasswordStoreBackendSyncDelegateImpl>(profile)));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);

  password_manager::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  if (!ps->Init(profile->GetPrefs(), std::move(affiliated_match_helper))) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }

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

  if (profile->IsRegularProfile())
    DelayReportingPasswordStoreMetrics(profile);

  return ps;
}

content::BrowserContext* PasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
