// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.usersPrivate.getUsers method.
class UsersPrivateGetUsersFunction : public ExtensionFunction {
 public:
  UsersPrivateGetUsersFunction();
  DECLARE_EXTENSION_FUNCTION("usersPrivate.getUsers", USERSPRIVATE_GETUSERS)

 protected:
  ~UsersPrivateGetUsersFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateGetUsersFunction);
};

// Implements the chrome.usersPrivate.isUserInList method.
class UsersPrivateIsUserInListFunction : public ExtensionFunction {
 public:
  UsersPrivateIsUserInListFunction();
  DECLARE_EXTENSION_FUNCTION("usersPrivate.isUserInList",
                             USERSPRIVATE_ISUSERINLIST)

 protected:
  ~UsersPrivateIsUserInListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateIsUserInListFunction);
};

// Implements the chrome.usersPrivate.addUser method.
class UsersPrivateAddUserFunction : public ExtensionFunction {
 public:
  UsersPrivateAddUserFunction();
  DECLARE_EXTENSION_FUNCTION("usersPrivate.addUser", USERSPRIVATE_ADDUSER)

 protected:
  ~UsersPrivateAddUserFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateAddUserFunction);
};

// Implements the chrome.usersPrivate.removeUser method.
class UsersPrivateRemoveUserFunction : public ExtensionFunction {
 public:
  UsersPrivateRemoveUserFunction();
  DECLARE_EXTENSION_FUNCTION("usersPrivate.removeUser", USERSPRIVATE_REMOVEUSER)

 protected:
  ~UsersPrivateRemoveUserFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateRemoveUserFunction);
};

// Implements the chrome.usersPrivate.isUserListManaged method.
class UsersPrivateIsUserListManagedFunction : public ExtensionFunction {
 public:
  UsersPrivateIsUserListManagedFunction();
  DECLARE_EXTENSION_FUNCTION("usersPrivate.isUserListManaged",
                             USERSPRIVATE_ISUSERLISTMANAGED)

 protected:
  ~UsersPrivateIsUserListManagedFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateIsUserListManagedFunction);
};

// Implements the chrome.usersPrivate.getCurrentUser method.
class UsersPrivateGetCurrentUserFunction : public ExtensionFunction {
 public:
  UsersPrivateGetCurrentUserFunction();
  DECLARE_EXTENSION_FUNCTION("usersPrivate.getCurrentUser",
                             USERSPRIVATE_GETCURRENTUSER)

 protected:
  ~UsersPrivateGetCurrentUserFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateGetCurrentUserFunction);
};

class UsersPrivateGetLoginStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usersPrivate.getLoginStatus",
                             USERSPRIVATE_GETLOGINSTATUS)
  UsersPrivateGetLoginStatusFunction();

 private:
  ~UsersPrivateGetLoginStatusFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsersPrivateGetLoginStatusFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_API_H_
