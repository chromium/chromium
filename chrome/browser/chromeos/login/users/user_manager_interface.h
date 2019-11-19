// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_USER_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_USER_MANAGER_INTERFACE_H_

#include "base/macros.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

class AccountId;

namespace chromeos {

class MultiProfileUserController;
class SupervisedUserManager;
class UserFlow;
class UserImageManager;

// ChromeOS specific add-ons interface for the UserManager.
class UserManagerInterface {
 public:
  UserManagerInterface() {}
  virtual ~UserManagerInterface() {}

  virtual MultiProfileUserController* GetMultiProfileUserController() = 0;
  virtual UserImageManager* GetUserImageManager(
      const AccountId& account_id) = 0;
  virtual SupervisedUserManager* GetSupervisedUserManager() = 0;

  // Method that allows to set |flow| for user identified by |account_id|.
  // Flow should be set before login attempt.
  // Takes ownership of the |flow|, |flow| will be deleted in case of login
  // failure.
  virtual void SetUserFlow(const AccountId& account_id, UserFlow* flow) = 0;

  // Return user flow for current user. Returns instance of DefaultUserFlow if
  // no flow was defined for current user, or user is not logged in.
  // Returned value should not be cached.
  virtual UserFlow* GetCurrentUserFlow() const = 0;

  // Return user flow for user identified by |account_id|. Returns instance of
  // DefaultUserFlow if no flow was defined for user.
  // Returned value should not be cached.
  virtual UserFlow* GetUserFlow(const AccountId& account_id) const = 0;

  // Resets user flow for user identified by |account_id|.
  virtual void ResetUserFlow(const AccountId& account_id) = 0;

  DISALLOW_COPY_AND_ASSIGN(UserManagerInterface);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_USER_MANAGER_INTERFACE_H_
