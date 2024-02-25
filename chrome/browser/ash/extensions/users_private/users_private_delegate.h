// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/common/extensions/api/users_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

// Provides prefs access for managing users.
// Use UsersPrivateDelegateFactory to create a UsersPrivateDelegate object.
class UsersPrivateDelegate : public KeyedService {
 public:
  explicit UsersPrivateDelegate(Profile* profile);

  UsersPrivateDelegate(const UsersPrivateDelegate&) = delete;
  UsersPrivateDelegate& operator=(const UsersPrivateDelegate&) = delete;

  ~UsersPrivateDelegate() override;

  // Gets a PrefsUtil object used for persisting settings.
  // The caller does not own the returned object.
  virtual PrefsUtil* GetPrefsUtil();

 protected:
  raw_ptr<Profile, LeakedDanglingUntriaged> profile_;  // weak; not owned by us
  std::unique_ptr<PrefsUtil> prefs_util_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_H_
