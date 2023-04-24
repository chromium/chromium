// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/users/supervised_user_manager.h"

namespace ash {
class ChromeUserManagerImpl;
class CrosSettings;

// TODO(b/231321563): Check this entire class is not used anymore for
// deprecated supervised users and remove it with all dependencies.
// Implementation of the UserManager.
class SupervisedUserManagerImpl : public SupervisedUserManager {
 public:
  SupervisedUserManagerImpl(const SupervisedUserManagerImpl&) = delete;
  SupervisedUserManagerImpl& operator=(const SupervisedUserManagerImpl&) =
      delete;

  ~SupervisedUserManagerImpl() override;

  std::string GetUserSyncId(const std::string& user_id) const override;
  std::u16string GetManagerDisplayName(
      const std::string& user_id) const override;
  std::string GetManagerUserId(const std::string& user_id) const override;
  std::string GetManagerDisplayEmail(const std::string& user_id) const override;
  void GetPasswordInformation(const std::string& user_id,
                              base::Value::Dict* result) override;
  void SetPasswordInformation(const std::string& user_id,
                              const base::Value::Dict* password_info) override;

 private:
  friend class ChromeUserManagerImpl;
  friend class UserManager;

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

  raw_ptr<ChromeUserManagerImpl, ExperimentalAsh> owner_;

  // Interface to the signed settings store.
  raw_ptr<CrosSettings, ExperimentalAsh> cros_settings_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_SUPERVISED_USER_MANAGER_IMPL_H_
