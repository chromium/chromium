// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/users/supervised_user_manager.h"

namespace ash {
class ChromeUserManagerImpl;
class CrosSettings;
class SupervisedUserTestBase;

// TODO(crbug.com/1155729): Check this entire class is not used anymore for
// deprecated supervised users and remove it with all dependencies.
// Implementation of the UserManager.
class SupervisedUserManagerImpl : public SupervisedUserManager {
 public:
  ~SupervisedUserManagerImpl() override;

  std::string GetUserSyncId(const std::string& user_id) const override;
  std::u16string GetManagerDisplayName(
      const std::string& user_id) const override;
  std::string GetManagerUserId(const std::string& user_id) const override;
  std::string GetManagerDisplayEmail(const std::string& user_id) const override;
  void GetPasswordInformation(const std::string& user_id,
                              base::DictionaryValue* result) override;
  void SetPasswordInformation(
      const std::string& user_id,
      const base::DictionaryValue* password_info) override;

 private:
  friend class ChromeUserManagerImpl;
  friend class UserManager;
  friend class SupervisedUserTestBase;

  explicit SupervisedUserManagerImpl(ChromeUserManagerImpl* owner);

  void RemoveNonCryptohomeData(const std::string& user_id);

  bool CheckForFirstRun(const std::string& user_id);

  bool GetUserStringValue(const std::string& user_id,
                          const char* key,
                          std::string* out_value) const;

  void SetUserStringValue(const std::string& user_id,
                          const char* key,
                          const std::string& value);

  bool GetUserIntegerValue(const std::string& user_id,
                           const char* key,
                           int* out_value) const;

  void SetUserIntegerValue(const std::string& user_id,
                           const char* key,
                           const int value);

  bool GetUserBooleanValue(const std::string& user_id,
                           const char* key,
                           bool* out_value) const;

  void SetUserBooleanValue(const std::string& user_id,
                           const char* key,
                           const bool value);

  void CleanPref(const std::string& user_id, const char* key);

  ChromeUserManagerImpl* owner_;

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserManagerImpl);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_IMPL_H_
