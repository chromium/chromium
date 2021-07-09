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
#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/browser/password_store_impl.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#endif

using password_manager::PasswordStore;

// static
scoped_refptr<PasswordStore> PasswordStoreFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord())
    return nullptr;
  return base::WrapRefCounted(static_cast<password_manager::PasswordStore*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
PasswordStoreFactory* PasswordStoreFactory::GetInstance() {
  return base::Singleton<PasswordStoreFactory>::get();
}

// static
void PasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged(
    Profile* profile) {
  scoped_refptr<PasswordStore> password_store =
      GetForProfile(profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!password_store)
    return;
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
      password_store.get(), sync_service,
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      content::GetNetworkConnectionTracker(), profile->GetPath());
}

PasswordStoreFactory::PasswordStoreFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "PasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

PasswordStoreFactory::~PasswordStoreFactory() = default;

scoped_refptr<RefcountedKeyedService>
PasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_WIN)
  password_manager_util_win::DelayReportOsPassword();
#endif
  Profile* profile = static_cast<Profile*>(context);

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          profile->GetPath()));

  scoped_refptr<PasswordStore> ps;
#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_ANDROID) || \
    defined(OS_MAC) || defined(USE_X11) || defined(USE_OZONE)
  ps = new password_manager::PasswordStoreImpl(std::move(login_db));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);
  if (!ps->Init(profile->GetPrefs())) {
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
      profile->GetPrefs(), base::TimeDelta::FromSeconds(60),
      network_context_getter);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kFillingAcrossAffiliatedWebsites)) {
    // Try to create affiliation service without awaiting synced state changes.
    // TODO(http://crbug.com/1202699): Remove sync service completely after
    // launching HashAffiliationLookup.
    password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
        ps.get(), /*sync_service=*/nullptr,
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        content::GetNetworkConnectionTracker(), profile->GetPath());
  }

  return ps;
}

content::BrowserContext* PasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
