// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

class PrefRegistrySimple;

namespace ash {

// Keys in dictionary with supervised password information.
extern const char kSchemaVersion[];
extern const char kPasswordRevision[];
extern const char kSalt[];
extern const char kRequirePasswordUpdate[];
extern const char kHasIncompleteKey[];

// Base class for SupervisedUserManagerImpl - provides a mechanism for getting
// and setting specific values for supervised users, as well as additional
// lookup methods that make sense only for supervised users.
// TODO(b/231321563): Check this entire class is not used anymore for
// deprecated supervised users and remove it with all dependencies.
class SupervisedUserManager {
 public:
  // Registers user manager preferences.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  SupervisedUserManager() {}

  SupervisedUserManager(const SupervisedUserManager&) = delete;
  SupervisedUserManager& operator=(const SupervisedUserManager&) = delete;

  virtual ~SupervisedUserManager() {}

  // Returns sync_user_id for supervised user with `user_id` or empty string if
  // such user is not found or it doesn't have user_id defined.
  virtual std::string GetUserSyncId(const std::string& user_id) const = 0;

  // Returns the display name for manager of user `user_id` if it is known
  // (was previously set by a `SaveUserDisplayName` call).
  // Otherwise, returns a manager id.
  virtual std::u16string GetManagerDisplayName(
      const std::string& user_id) const = 0;

  // Returns the user id for manager of user `user_id` if it is known (user is
  // actually a managed user).
  // Otherwise, returns an empty string.
  virtual std::string GetManagerUserId(const std::string& user_id) const = 0;

  // Returns the display email for manager of user `user_id` if it is known
  // (user is actually a managed user).
  // Otherwise, returns an empty string.
  virtual std::string GetManagerDisplayEmail(
      const std::string& user_id) const = 0;

  // Fill `result` with public password-specific data for `user_id` from Local
  // State.
  virtual void GetPasswordInformation(const std::string& user_id,
                                      base::Value::Dict* result) = 0;

  // Stores public password-specific data from `password_info` for `user_id` in
  // Local State.
  virtual void SetPasswordInformation(
      const std::string& user_id,
      const base::Value::Dict* password_info) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_H_
