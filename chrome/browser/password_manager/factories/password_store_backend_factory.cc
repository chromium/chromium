// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/factories/password_store_backend_factory.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_account_backend.h"
#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"
#include "chrome/browser/password_manager/android/password_store_empty_backend.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/policy/policy_path_parser.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace {

#if !BUILDFLAG(IS_ANDROID)
void SetIsUserDataDirPolicySet(
    password_manager::LoginDatabase* login_database) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  base::FilePath user_data_dir;
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
  // If `user_data_dir` is empty it means that policy did not set it.
  login_database->SetIsUserDataDirPolicySet(!user_data_dir.empty());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

std::unique_ptr<password_manager::PasswordStoreBackend>
CreatePasswordStoreBackend(password_manager::IsAccountStore is_account_store,
                           const base::FilePath& login_db_directory,
                           PrefService* prefs,
                           os_crypt_async::OSCryptAsync* os_crypt_async) {
  TRACE_EVENT0("passwords", is_account_store
                                ? "AccountPasswordStoreBackendCreation"
                                : "ProfilePasswordStoreBackendCreation");

#if BUILDFLAG(IS_ANDROID)
  using password_manager_android_util::PasswordManagerUtilBridge;
  if (!password_manager_android_util::IsPasswordManagerAvailable(
          std::make_unique<PasswordManagerUtilBridge>())) {
    return std::make_unique<password_manager::PasswordStoreEmptyBackend>();
  }
  if (is_account_store) {
    return std::make_unique<
        password_manager::PasswordStoreAndroidAccountBackend>();
  }
  return std::make_unique<password_manager::PasswordStoreAndroidLocalBackend>();
#else   //  BUILDFLAG(IS_ANDROID)
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabase(is_account_store,
                                            login_db_directory, prefs));
  SetIsUserDataDirPolicySet(login_db.get());
  auto behavior = is_account_store
                      ? syncer::WipeModelUponSyncDisabledBehavior::kAlways
                      : syncer::WipeModelUponSyncDisabledBehavior::kNever;
  return std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
      std::move(login_db), behavior, prefs, os_crypt_async);
#endif  // BUILDFLAG(IS_ANDROID)
}
