// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_backend_factory.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/password_manager/android/password_store_empty_backend.h"
#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_store_android_account_backend.h"
#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"
#include "chrome/browser/password_manager/android/password_store_backend_migration_decorator.h"
#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/policy/policy_path_parser.h"  // nogncheck
#endif

namespace {

using ::password_manager::PasswordStoreBackend;
using ::password_manager::PasswordStoreBuiltInBackend;
#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;
using password_manager_android_util::PasswordManagerUtilBridge;
#endif

std::unique_ptr<PasswordStoreBackend> CreateProfilePasswordStoreBuiltInBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(login_db_directory,
                                                             prefs));
  password_manager::LoginDatabase* login_db_ptr = login_db.get();
  std::unique_ptr<PasswordStoreBackend> backend =
      std::make_unique<PasswordStoreBuiltInBackend>(
          std::move(login_db),
          syncer::WipeModelUponSyncDisabledBehavior::kNever, prefs,
          os_crypt_async);

  auto is_profile_db_empty_cb =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &password_manager::IntermediateCallbackForSettingPrefs,
          backend->AsWeakPtr(),
#if BUILDFLAG(IS_ANDROID)
          base::BindRepeating(
              &password_manager::SetEmptyStorePref, prefs,
              password_manager::prefs::kEmptyProfileStoreLoginDatabase)));
#else
          base::BindRepeating(
              &password_manager::SetAutofillableCredentialsStorePref, prefs,
              password_manager::prefs::
                  kAutofillableCredentialsProfileStoreLoginDatabase)));
#endif
  login_db_ptr->SetIsEmptyCb(std::move(is_profile_db_empty_cb));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  base::FilePath user_data_dir;
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
  // If `user_data_dir` is empty it means that policy did not set it.
  login_db_ptr->SetIsUserDataDirPolicySet(!user_data_dir.empty());
#endif
  return backend;
}

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

// Creates the backend for the profile `PasswordStore` on Android, after
// login db deprecation.
std::unique_ptr<PasswordStoreBackend> CreateProfilePasswordStoreBackendAndroid(
    PrefService* prefs,
    const base::FilePath& login_db_directory,
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  CHECK(base::FeatureList::IsEnabled(
      password_manager::features::kLoginDbDeprecationAndroid));
  if (!password_manager_android_util::LoginDbDeprecationReady(prefs)) {
    // There are still passwords that need exporting, so instantiate the
    // backend that connects to the login DB.
    return CreateProfilePasswordStoreBuiltInBackend(login_db_directory, prefs,
                                                    os_crypt_async);
  }
  // Once the login DB is deprecated, there are only 2 options for
  // the backend: an empty one if the Android backend isn't supported,
  // or the Android backend.
  if (password_manager_android_util::IsPasswordManagerAvailable(
          prefs, std::make_unique<PasswordManagerUtilBridge>())) {
    return std::make_unique<password_manager::PasswordStoreAndroidLocalBackend>(
        prefs);
  }

  return std::make_unique<password_manager::PasswordStoreEmptyBackend>();
}

// Creates the backend for the account `PasswordStore` on Android, after
// login db deprecation.
std::unique_ptr<PasswordStoreBackend> CreateAccountPasswordStoreBackendAndroid(
    PrefService* prefs) {
  CHECK(base::FeatureList::IsEnabled(
      password_manager::features::kLoginDbDeprecationAndroid));
  // The account store shouldn't have an associated login DB with existing
  // passwords, so no pre-export step is required.
  if (password_manager_android_util::IsPasswordManagerAvailable(
          prefs, std::make_unique<PasswordManagerUtilBridge>())) {
    return std::make_unique<
        password_manager::PasswordStoreAndroidAccountBackend>(
        prefs, password_manager::kAccountStore);
  }
  return std::make_unique<password_manager::PasswordStoreEmptyBackend>();
}

// TODO(crbug.com/378653024): Remove this function after login db deprecation.
std::unique_ptr<PasswordStoreBackend>
CreateProfilePasswordStoreBackendForUpmAndroid(
    PrefService* prefs,
    const base::FilePath& login_db_directory,
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.WasEnrolledInUPMWhenBackendWasCreated",
      !prefs->GetBoolean(password_manager::prefs::
                             kUnenrolledFromGoogleMobileServicesDueToErrors));
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
          CreateProfilePasswordStoreBuiltInBackend(login_db_directory, prefs,
                                                   os_crypt_async),
          std::make_unique<password_manager::PasswordStoreAndroidLocalBackend>(
              prefs),
          prefs);
    // UPM M2: The password store proxy backend is created. No migrations are
    // needed.
    case UseUpmLocalAndSeparateStoresState::kOn:
      return std::make_unique<
          password_manager::PasswordStoreAndroidLocalBackend>(prefs);
    // Old UPM: support for local passwords in GMSCore is unavailable for some
    // reason.
    case UseUpmLocalAndSeparateStoresState::kOff: {
      // Even though this is a backend for a ProfilePasswordStore it has to
      // talk to the account. Before the store split, the `ProfilePasswordStore`
      // only supports talking to the account storage in GMS Core. All local
      // storage requests go to the built-in backend instead.
      auto android_account_backend = std::make_unique<
          password_manager::PasswordStoreAndroidAccountBackend>(
          prefs, password_manager::kProfileStore);
      // Chrome stopped trying to migrate passwords to the account GMSCore
      // storage. Only PasswordStoreProxyBackend is created.
      return std::make_unique<password_manager::PasswordStoreProxyBackend>(
          CreateProfilePasswordStoreBuiltInBackend(login_db_directory, prefs,
                                                   os_crypt_async),
          std::move(android_account_backend), prefs);
    }
  }
}
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}  // namespace

std::unique_ptr<PasswordStoreBackend> CreateProfilePasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  TRACE_EVENT0("passwords", "PasswordStoreBackendCreation");

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kLoginDbDeprecationAndroid)) {
    // During the login DB deprecation, only the built-in backend should be
    // instantiated. After the unmigrated passwords are exported, the login DB
    // is considered deprecated. There will be only 2 options for
    // the backend: an empty one if the Android backend isn't supported,
    // or the Android backend.
    return CreateProfilePasswordStoreBackendAndroid(prefs, login_db_directory,
                                                    os_crypt_async);
  }

  // This are the absolute minimum requirements to have any version of UPM.
  if (password_manager_android_util::AreMinUpmRequirementsMet()) {
    return CreateProfilePasswordStoreBackendForUpmAndroid(
        prefs, login_db_directory, os_crypt_async);
  }
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  return CreateProfilePasswordStoreBuiltInBackend(login_db_directory, prefs,
                                                  os_crypt_async);
}

std::unique_ptr<PasswordStoreBackend> CreateAccountPasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    password_manager::UnsyncedCredentialsDeletionNotifier
        unsynced_deletions_notifier,
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(login_db_directory,
                                                             prefs));
  std::unique_ptr<PasswordStoreBackend> backend;
#if !BUILDFLAG(IS_ANDROID)
  password_manager::LoginDatabase* login_db_ptr = login_db.get();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  backend = std::make_unique<PasswordStoreBuiltInBackend>(
      std::move(login_db), syncer::WipeModelUponSyncDisabledBehavior::kAlways,
      prefs, os_crypt_async, std::move(unsynced_deletions_notifier));
#else  // BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

  if (base::FeatureList::IsEnabled(
          password_manager::features::kLoginDbDeprecationAndroid)) {
    // Once the login DB is deprecated, there will be only 2 options for
    // the backend: an empty one if the Android backend isn't supported,
    // or the Android backend.
    return CreateAccountPasswordStoreBackendAndroid(prefs);
  }

  // On Android, if there is no internal backend, the account store isn't
  // created, so this function isn't called.
  // If the GMS Core version is not high enough for the account-only upm,
  // it will not be high enough for split stores, so the account store
  // won't be created in that case either.
  CHECK(password_manager_android_util::AreMinUpmRequirementsMet());
  backend =
      std::make_unique<password_manager::PasswordStoreAndroidAccountBackend>(
          prefs, password_manager::kAccountStore);
#endif

#if !BUILDFLAG(IS_ANDROID)
  auto is_account_db_empty_cb =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &password_manager::IntermediateCallbackForSettingPrefs,
          backend->AsWeakPtr(),
          base::BindRepeating(
              &password_manager::SetAutofillableCredentialsStorePref, prefs,
              password_manager::prefs::
                  kAutofillableCredentialsAccountStoreLoginDatabase)));
  login_db_ptr->SetIsEmptyCb(std::move(is_account_db_empty_cb));
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  base::FilePath user_data_dir;
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
  // If `user_data_dir` is empty it means that policy did not set it.
  login_db_ptr->SetIsUserDataDirPolicySet(!user_data_dir.empty());
#endif

  return backend;
}
