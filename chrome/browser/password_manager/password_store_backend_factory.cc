// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/browser/password_store_backend.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_manager_buildflags.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_eviction_util.h"
#include "components/password_manager/core/browser/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "components/password_manager/core/browser/password_store_backend_migration_decorator.h"
#include "components/password_manager/core/common/password_manager_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace password_manager {

std::unique_ptr<PasswordStoreBackend> PasswordStoreBackend::Create(
    const base::FilePath& login_db_path,
    PrefService* prefs) {
  TRACE_EVENT0("passwords", "PasswordStoreBackendCreation");
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(USE_LEGACY_PASSWORD_STORE_BACKEND)
  return std::make_unique<PasswordStoreBuiltInBackend>(
      CreateLoginDatabaseForProfileStorage(login_db_path),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
#else  // BUILDFLAG(IS_ANDROID) && !USE_LEGACY_PASSWORD_STORE_BACKEND
  if (PasswordStoreAndroidBackendBridgeHelper::CanCreateBackend() &&
      base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerAndroid)) {
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
    return std::make_unique<PasswordStoreBackendMigrationDecorator>(
        std::make_unique<PasswordStoreBuiltInBackend>(
            CreateLoginDatabaseForProfileStorage(login_db_path),
            syncer::WipeModelUponSyncDisabledBehavior::kNever),
        std::make_unique<PasswordStoreAndroidBackend>(prefs), prefs);
  }
  return std::make_unique<PasswordStoreBuiltInBackend>(
      CreateLoginDatabaseForProfileStorage(login_db_path),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
#endif
}

}  // namespace password_manager
