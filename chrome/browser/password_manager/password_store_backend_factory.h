// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"

class PrefService;

namespace password_manager {
class PasswordStoreBackend;
}

std::unique_ptr<password_manager::PasswordStoreBackend>
CreatePasswordStoreBackend(const base::FilePath& login_db_directory,
                           PrefService* prefs);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_FACTORY_H_
