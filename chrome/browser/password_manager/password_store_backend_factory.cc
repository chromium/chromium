// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_backend_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
#include "chrome/browser/password_manager/android/android_backend_with_double_deletion.h"
#include "chrome/browser/password_manager/android/legacy_password_store_backend_migration_decorator.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_store_android_account_backend.h"
#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"
#include "chrome/browser/password_manager/android/password_store_backend_migration_decorator.h"
#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#endif

namespace {

using ::password_manager::PasswordStoreBackend;
using ::password_manager::PasswordStoreBuiltInBackend;
#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

std::unique_ptr<PasswordStoreBackend>
CreateProfilePasswordStoreBackendForUpmAndroid(
    PrefService* prefs,
    std::unique_ptr<PasswordStoreBuiltInBackend> built_in_backend,
    password_manager::PasswordAffiliationSourceAdapter&
        password_affiliation_adapter) {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.WasEnrolledInUPMWhenBackendWasCreated",
      !prefs->GetBoolean(password_manager::prefs::
                             kUnenrolledFromGoogleMobileServicesDueToErrors));
  base::UmaHistogramCounts100(
      "PasswordManager.PasswordStore.TimesReenrolledInUPM",
      prefs->GetInteger(
          password_manager::prefs::kTimesReenrolledToGoogleMobileServices));
  base::UmaHistogramCounts100(
      "PasswordManager.PasswordStore.TimesAttemptedToReenrollInUPM",
      prefs->GetInteger(password_manager::prefs::
                            kTimesAttemptedToReenrollToGoogleMobileServices));
  auto useSplitStores =
      static_cast<UseUpmLocalAndSeparateStoresState>(prefs->GetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores));
  // Creates the password store backend for the profile store on Android
  // platform when UPM is enabled. There are 3 cases:
  switch (useSplitStores) {
    // UPM M3: The password store migration decorator is created as backend. It
    // is expected to migrate the passwords from the built in profile store to
    // the GMS core local store.
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      return std::make_unique<
          password_manager::PasswordStoreBackendMigrationDecorator>(
          std::move(built_in_backend),
          std::make_unique<password_manager::PasswordStoreAndroidLocalBackend>(
              prefs, password_affiliation_adapter),
          prefs);
    // UPM M2: The password store proxy backend is created. No migrations are
    // needed.
    case UseUpmLocalAndSeparateStoresState::kOn:
      return std::make_unique<AndroidBackendWithDoubleDeletion>(
          std::move(built_in_backend),
          std::make_unique<password_manager::PasswordStoreAndroidLocalBackend>(
              prefs, password_affiliation_adapter));
    // Old UPM: support for local passwords in GMSCore is unavailable for some
    // reason.
    case UseUpmLocalAndSeparateStoresState::kOff: {
      // Even though this is a backend for a ProfilePasswordStore it has to
      // talk to the account. Before the store split, the `ProfilePasswordStore`
      // only supports talking to the account storage in GMS Core. All local
      // storage requests go to the built-in backend instead.
      auto android_account_backend = std::make_unique<
          password_manager::PasswordStoreAndroidAccountBackend>(
          prefs, &password_affiliation_adapter,
          password_manager::kProfileStore);
      if (base::FeatureList::IsEnabled(
              password_manager::features::
                  kUnifiedPasswordManagerSyncOnlyInGMSCore)) {
        // M4 feature flag is enabled. Chrome stops trying to migrate passwords
        // to the account GMSCore storage. Only PasswordStoreProxyBackend is
        // created.
        return std::make_unique<password_manager::PasswordStoreProxyBackend>(
            std::move(built_in_backend), std::move(android_account_backend),
            prefs);
      }
      // The password store migration decorator is created as backend.
      // There are no split stores at this stage, and the decorator is expected
      // to migrate the passwords from the built in profile store to the GMS
      // core account store.
      return std::make_unique<
          password_manager::LegacyPasswordStoreBackendMigrationDecorator>(
          std::move(built_in_backend), std::move(android_account_backend),
          prefs);
    }
  }
}
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}  // namespace

std::unique_ptr<PasswordStoreBackend> CreateProfilePasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    password_manager::PasswordAffiliationSourceAdapter&
        password_affiliation_adapter) {
  TRACE_EVENT0("passwords", "PasswordStoreBackendCreation");
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          login_db_directory));
  password_manager::LoginDatabase* login_db_ptr = login_db.get();
  std::unique_ptr<PasswordStoreBackend> backend =
      std::make_unique<PasswordStoreBuiltInBackend>(
          std::move(login_db),
          syncer::WipeModelUponSyncDisabledBehavior::kNever, prefs);

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  // This are the absolute minimum requirements to have any version of UPM.
  if (password_manager_android_util::AreMinUpmRequirementsMet()) {
    std::unique_ptr<PasswordStoreBuiltInBackend> backend_as_built_in_backend(
        static_cast<PasswordStoreBuiltInBackend*>(backend.release()));
    backend = CreateProfilePasswordStoreBackendForUpmAndroid(
        prefs, std::move(backend_as_built_in_backend),
        password_affiliation_adapter);
  }
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

  auto is_profile_db_empty_cb =
#if BUILDFLAG(IS_ANDROID)
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &password_manager::SetEmptyStorePref, prefs, backend->AsWeakPtr(),
          password_manager::prefs::kEmptyProfileStoreLoginDatabase));
#else
      base::NullCallback();
#endif
  login_db_ptr->SetIsEmptyCb(std::move(is_profile_db_empty_cb));

  return backend;
}

std::unique_ptr<PasswordStoreBackend> CreateAccountPasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    std::unique_ptr<password_manager::UnsyncedCredentialsDeletionNotifier>
        unsynced_deletions_notifier) {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(
          login_db_directory));
  std::unique_ptr<PasswordStoreBackend> backend;

#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  backend = std::make_unique<PasswordStoreBuiltInBackend>(
      std::move(login_db), syncer::WipeModelUponSyncDisabledBehavior::kAlways,
      prefs, std::move(unsynced_deletions_notifier));
#else  // BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  if (!password_manager_android_util::AreMinUpmRequirementsMet()) {
    // Can happen if the downstream code is not available.
    backend = std::make_unique<PasswordStoreBuiltInBackend>(
        std::move(login_db), syncer::WipeModelUponSyncDisabledBehavior::kAlways,
        prefs);
  }

  // Note: The built-in backend is backed by the login database and Chrome
  // syncs it. As such, it expects local data to be cleared every time when
  // sync is permanently disabled and thus uses
  // WipeModelUponSyncDisabledBehavior::kAlways.
  backend = std::make_unique<AndroidBackendWithDoubleDeletion>(
      std::make_unique<PasswordStoreBuiltInBackend>(
          std::move(login_db),
          syncer::WipeModelUponSyncDisabledBehavior::kAlways, prefs),
      std::make_unique<password_manager::PasswordStoreAndroidAccountBackend>(
          prefs, /*password_affiliation_adapter=*/nullptr,
          password_manager::kAccountStore));
#endif
  return backend;
}
