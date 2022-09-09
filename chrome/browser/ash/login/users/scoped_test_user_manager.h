// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_SCOPED_TEST_USER_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_SCOPED_TEST_USER_MANAGER_H_

#include <memory>

namespace ash {
class ChromeUserManager;

// Helper class for unit tests. Initializes the UserManager singleton on
// construction and tears it down again on destruction.
class ScopedTestUserManager {
 public:
  ScopedTestUserManager();

  ScopedTestUserManager(const ScopedTestUserManager&) = delete;
  ScopedTestUserManager& operator=(const ScopedTestUserManager&) = delete;

  ~ScopedTestUserManager();

 private:
  std::unique_ptr<ChromeUserManager> chrome_user_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_SCOPED_TEST_USER_MANAGER_H_
