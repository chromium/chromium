// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_onboarding.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
// Use default store.
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
// Don't do anything. We're going to use the default store.
#elif defined(USE_X11)
#include "chrome/browser/password_manager/password_store_x.h"
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"
#endif

using password_manager::PasswordStore;

namespace {

#if defined(USE_X11)
constexpr PasswordStoreX::MigrationToLoginDBStep
    kMigrationToLoginDBNotAttempted = PasswordStoreX::NOT_ATTEMPTED;
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
std::string GetSyncUsername(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  return identity_manager ? identity_manager->GetPrimaryAccountInfo().email
                          : std::string();
}
#endif

}  // namespace

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
      ProfileSyncServiceFactory::GetForProfile(profile);

  password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
      password_store.get(), sync_service,
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess(),
      content::GetNetworkConnectionTracker(), profile->GetPath());
}

PasswordStoreFactory::PasswordStoreFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "PasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // TODO(crbug.com/715987). Remove when PasswordReuseDetector is decoupled
  // from PasswordStore.
  DependsOn(IdentityManagerFactory::GetInstance());
#endif
}

PasswordStoreFactory::~PasswordStoreFactory() {}

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
#if defined(OS_MACOSX)
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  login_db->InitPasswordRecoveryUtil(
      std::make_unique<password_manager::PasswordRecoveryUtilMac>(
          local_state, base::ThreadTaskRunnerHandle::Get()));
#endif

  scoped_refptr<PasswordStore> ps;
#if defined(OS_WIN)
  ps = new password_manager::PasswordStoreDefault(std::move(login_db));
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_MACOSX)
  ps = new password_manager::PasswordStoreDefault(std::move(login_db));
#elif defined(USE_X11)
  ps = new PasswordStoreX(std::move(login_db), profile->GetPrefs());
#elif defined(USE_OZONE)
  ps = new password_manager::PasswordStoreDefault(std::move(login_db));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);
  if (!ps->Init(sync_start_util::GetFlareForSyncableService(profile->GetPath()),
                profile->GetPrefs())) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Prepare password hash data for reuse detection.
  ps->PreparePasswordHashData(GetSyncUsername(profile));
#endif

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile))
          return nullptr;
        return content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetNetworkContext();
      },
      profile);
  password_manager_util::RemoveUselessCredentials(ps, profile->GetPrefs(), 60,
                                                  network_context_getter);

  // Update the |kPasswordManagerOnboardingState| pref in the background.
  UpdateOnboardingState(ps, profile->GetPrefs(),
                        base::TimeDelta::FromSeconds(20));

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  std::unique_ptr<password_manager::PasswordStoreSigninNotifier> notifier =
      std::make_unique<password_manager::PasswordStoreSigninNotifierImpl>(
          IdentityManagerFactory::GetForProfile(profile));
  ps->SetPasswordStoreSigninNotifier(std::move(notifier));
#endif

  return ps;
}

void PasswordStoreFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(USE_X11)
  // Notice that the preprocessor conditions above are exactly those that will
  // result in using PasswordStoreX in BuildServiceInstanceFor().
  registry->RegisterIntegerPref(
      password_manager::prefs::kMigrationToLoginDBStep,
      kMigrationToLoginDBNotAttempted);
#endif
}

content::BrowserContext* PasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
