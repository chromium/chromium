// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_

#include "base/memory/scoped_refptr.h"

class Profile;
namespace password_manager {
class TestPasswordStore;
}

scoped_refptr<password_manager::TestPasswordStore>
CreateAndUseTestPasswordStore(Profile* profile);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_UTIL_H_
