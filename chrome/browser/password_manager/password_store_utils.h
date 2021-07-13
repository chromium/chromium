// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password store.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_

#include "base/memory/scoped_refptr.h"

namespace password_manager {
class PasswordStoreInterface;
}

class Profile;

// Returns the password store associated with the currently active profile.
password_manager::PasswordStoreInterface* GetPasswordStore(
    Profile* profile,
    bool use_account_store);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_
