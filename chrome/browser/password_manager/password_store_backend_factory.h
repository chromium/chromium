// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "components/password_manager/core/browser/password_store/password_store.h"

class PrefService;

namespace password_manager {
class PasswordAffiliationSourceAdapter;
class PasswordStoreBackend;
}  // namespace password_manager

namespace os_crypt_async {
class OSCryptAsync;
}

// Creates the password store backend for the profile store. Depending on
// the platform, this can be backed by the login database, or by
// the android backend. The `password_affiliation_adapter` is used
// to cancel prefetching for affiliations in case the android backend
// is ready to provide logins for affiliations directly.
std::unique_ptr<password_manager::PasswordStoreBackend>
CreateProfilePasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    password_manager::PasswordAffiliationSourceAdapter&
        password_affiliation_adapter,
    os_crypt_async::OSCryptAsync* os_crypt_async);

// Creates the password store backend for the account store. Depending on
// the platform, this can be backed by the login database, or by
// the android backend.
std::unique_ptr<password_manager::PasswordStoreBackend>
CreateAccountPasswordStoreBackend(
    const base::FilePath& login_db_directory,
    PrefService* prefs,
    password_manager::UnsyncedCredentialsDeletionNotifier
        unsynced_deletions_notifier,
    os_crypt_async::OSCryptAsync* os_crypt_async);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_FACTORY_H_
