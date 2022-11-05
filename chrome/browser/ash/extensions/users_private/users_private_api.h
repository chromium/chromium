// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_API_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_API_H_

#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.usersPrivate.getUsers method.
class UsersPrivateGetUsersFunction : public ExtensionFunction {
 public:
  UsersPrivateGetUsersFunction();

  UsersPrivateGetUsersFunction(const UsersPrivateGetUsersFunction&) = delete;
  UsersPrivateGetUsersFunction& operator=(const UsersPrivateGetUsersFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("usersPrivate.getUsers", USERSPRIVATE_GETUSERS)

 protected:
  ~UsersPrivateGetUsersFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.usersPrivate.isUserInList method.
class UsersPrivateIsUserInListFunction : public ExtensionFunction {
 public:
  UsersPrivateIsUserInListFunction();

  UsersPrivateIsUserInListFunction(const UsersPrivateIsUserInListFunction&) =
      delete;
  UsersPrivateIsUserInListFunction& operator=(
      const UsersPrivateIsUserInListFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("usersPrivate.isUserInList",
                             USERSPRIVATE_ISUSERINLIST)

 protected:
  ~UsersPrivateIsUserInListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.usersPrivate.addUser method.
class UsersPrivateAddUserFunction : public ExtensionFunction {
 public:
  UsersPrivateAddUserFunction();

  UsersPrivateAddUserFunction(const UsersPrivateAddUserFunction&) = delete;
  UsersPrivateAddUserFunction& operator=(const UsersPrivateAddUserFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("usersPrivate.addUser", USERSPRIVATE_ADDUSER)

 protected:
  ~UsersPrivateAddUserFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.usersPrivate.removeUser method.
class UsersPrivateRemoveUserFunction : public ExtensionFunction {
 public:
  UsersPrivateRemoveUserFunction();

  UsersPrivateRemoveUserFunction(const UsersPrivateRemoveUserFunction&) =
      delete;
  UsersPrivateRemoveUserFunction& operator=(
      const UsersPrivateRemoveUserFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("usersPrivate.removeUser", USERSPRIVATE_REMOVEUSER)

 protected:
  ~UsersPrivateRemoveUserFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.usersPrivate.isUserListManaged method.
class UsersPrivateIsUserListManagedFunction : public ExtensionFunction {
 public:
  UsersPrivateIsUserListManagedFunction();

  UsersPrivateIsUserListManagedFunction(
      const UsersPrivateIsUserListManagedFunction&) = delete;
  UsersPrivateIsUserListManagedFunction& operator=(
      const UsersPrivateIsUserListManagedFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("usersPrivate.isUserListManaged",
                             USERSPRIVATE_ISUSERLISTMANAGED)

 protected:
  ~UsersPrivateIsUserListManagedFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.usersPrivate.getCurrentUser method.
class UsersPrivateGetCurrentUserFunction : public ExtensionFunction {
 public:
  UsersPrivateGetCurrentUserFunction();

  UsersPrivateGetCurrentUserFunction(
      const UsersPrivateGetCurrentUserFunction&) = delete;
  UsersPrivateGetCurrentUserFunction& operator=(
      const UsersPrivateGetCurrentUserFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("usersPrivate.getCurrentUser",
                             USERSPRIVATE_GETCURRENTUSER)

 protected:
  ~UsersPrivateGetCurrentUserFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class UsersPrivateGetLoginStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usersPrivate.getLoginStatus",
                             USERSPRIVATE_GETLOGINSTATUS)
  UsersPrivateGetLoginStatusFunction();

  UsersPrivateGetLoginStatusFunction(
      const UsersPrivateGetLoginStatusFunction&) = delete;
  UsersPrivateGetLoginStatusFunction& operator=(
      const UsersPrivateGetLoginStatusFunction&) = delete;

 private:
  ~UsersPrivateGetLoginStatusFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_API_H_
