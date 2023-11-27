// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_backend_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "chrome/browser/password_manager/android/password_store_backend_migration_decorator.h"
#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<password_manager::PasswordStoreBackend>
CreateProfilePasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    password_manager::AffiliationsPrefetcher* affiliations_prefetcher) {
  TRACE_EVENT0("passwords", "PasswordStoreBackendCreation");
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(USE_LEGACY_PASSWORD_STORE_BACKEND)
  return std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
      password_manager::CreateLoginDatabaseForProfileStorage(
          login_db_directory, /*is_empty_cb=*/base::NullCallback()),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
#else  // BUILDFLAG(IS_ANDROID) && !USE_LEGACY_PASSWORD_STORE_BACKEND
  // base::Unretained() is safe, `prefs` outlives all keyed services, including
  // the PasswordStore (LoginDatabase's owner).
  auto is_profile_db_empty_cb =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &PrefService::SetBoolean, base::Unretained(prefs),
          password_manager::prefs::kEmptyProfileStoreLoginDatabase));
  std::unique_ptr<password_manager::LoginDatabase> profile_login_db =
      password_manager::CreateLoginDatabaseForProfileStorage(
          login_db_directory, is_profile_db_empty_cb);

  if (password_manager::PasswordStoreAndroidBackendBridgeHelper::
          CanCreateBackend()) {
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
    return std::make_unique<
        password_manager::PasswordStoreBackendMigrationDecorator>(
        std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
            std::move(profile_login_db),
            syncer::WipeModelUponSyncDisabledBehavior::kNever),
        std::make_unique<password_manager::PasswordStoreAndroidBackend>(
            prefs, affiliations_prefetcher),
        prefs, password_manager::IsAccountStore(false));
  }
  return std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
      std::move(profile_login_db),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
#endif
}

std::unique_ptr<password_manager::PasswordStoreBackend>
CreateAccountPasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    std::unique_ptr<password_manager::UnsyncedCredentialsDeletionNotifier>
        unsynced_deletions_notifier,
    password_manager::AffiliationsPrefetcher* affiliations_prefetcher) {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(
          login_db_directory));
#if BUILDFLAG(IS_ANDROID)
  // The min GMS Core version required by the account backend is larger than
  // the one checked by `CanCreateBackend`. If an account backend is being
  // created, it means that the version check already passed before, so no
  // need to check `CanCreateBackend` here.
  CHECK(password_manager::PasswordStoreAndroidBackendBridgeHelper::
            CanCreateBackend());
  CHECK(password_manager_android_util::UsesSplitStoresAndUPMForLocal(prefs));

  // Note: The built-in backend is backed by the login database and Chrome
  // syncs it. As such, it expects local data to be cleared every time when
  // sync is permanently disabled and thus uses
  // WipeModelUponSyncDisabledBehavior::kAlways.
  return std::make_unique<
      password_manager::PasswordStoreBackendMigrationDecorator>(
      std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
          std::move(login_db),
          syncer::WipeModelUponSyncDisabledBehavior::kAlways),
      std::make_unique<password_manager::PasswordStoreAndroidBackend>(
          prefs, affiliations_prefetcher),
      prefs, password_manager::IsAccountStore(true));
#else
  return std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
      std::move(login_db), syncer::WipeModelUponSyncDisabledBehavior::kAlways,
      std::move(unsynced_deletions_notifier));
#endif
}
