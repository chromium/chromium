// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_FACTORIES_PASSWORD_STORE_BACKEND_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_FACTORIES_PASSWORD_STORE_BACKEND_FACTORY_H_

#include <memory>

#include "components/password_manager/core/browser/password_store/password_store_interface.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base

namespace password_manager {
class PasswordStoreBackend;
}  // namespace password_manager

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

// Depending on the platform, this can be backed by the login database, or by
// the android backend.
std::unique_ptr<password_manager::PasswordStoreBackend>
CreatePasswordStoreBackend(password_manager::IsAccountStore is_account_store,
                           const base::FilePath& login_db_directory,
                           PrefService* prefs,
                           os_crypt_async::OSCryptAsync* os_crypt_async);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_FACTORIES_PASSWORD_STORE_BACKEND_FACTORY_H_
