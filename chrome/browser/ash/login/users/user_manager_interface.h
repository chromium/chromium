// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_USER_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_USER_MANAGER_INTERFACE_H_

class AccountId;

namespace ash {

class MultiProfileUserController;
class SupervisedUserManager;
class UserImageManager;

// ChromeOS specific add-ons interface for the UserManager.
class UserManagerInterface {
 public:
  UserManagerInterface() = default;

  UserManagerInterface(const UserManagerInterface&) = delete;
  UserManagerInterface& operator=(const UserManagerInterface&) = delete;

  virtual ~UserManagerInterface() = default;

  virtual MultiProfileUserController* GetMultiProfileUserController() = 0;
  virtual UserImageManager* GetUserImageManager(
      const AccountId& account_id) = 0;
  virtual SupervisedUserManager* GetSupervisedUserManager() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_USER_MANAGER_INTERFACE_H_
