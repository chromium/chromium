// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_

#include "base/memory/scoped_refptr.h"

namespace content {
class BrowserContext;
}

namespace password_manager {
class TestPasswordStore;
}

class Profile;

scoped_refptr<password_manager::TestPasswordStore>
CreateAndUseTestPasswordStore(content::BrowserContext* context);

scoped_refptr<password_manager::TestPasswordStore>
CreateAndUseTestAccountPasswordStore(content::BrowserContext* context);

// Get the active test password store, which is the account storage if signed in
// without sync-the-feature and account storage for passwords enabled, and the
// local one otherwise.
password_manager::TestPasswordStore* GetDefaultPasswordStore(Profile* profile);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_
