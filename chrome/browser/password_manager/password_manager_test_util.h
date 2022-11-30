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

scoped_refptr<password_manager::TestPasswordStore>
CreateAndUseTestPasswordStore(content::BrowserContext* context);

scoped_refptr<password_manager::TestPasswordStore>
CreateAndUseTestAccountPasswordStore(content::BrowserContext* context);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_
