// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_FAKE_SUPERVISED_USER_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_FAKE_SUPERVISED_USER_MANAGER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/users/supervised_user_manager.h"

namespace ash {

// Fake supervised user manager with a barebones implementation.
class FakeSupervisedUserManager : public SupervisedUserManager {
 public:
  FakeSupervisedUserManager();
  ~FakeSupervisedUserManager() override;

  std::string GetUserSyncId(const std::string& user_id) const override;
  std::u16string GetManagerDisplayName(
      const std::string& user_id) const override;
  std::string GetManagerUserId(const std::string& user_id) const override;
  std::string GetManagerDisplayEmail(const std::string& user_id) const override;
  void GetPasswordInformation(const std::string& user_id,
                              base::DictionaryValue* result) override {}
  void SetPasswordInformation(
      const std::string& user_id,
      const base::DictionaryValue* password_info) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSupervisedUserManager);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_FAKE_SUPERVISED_USER_MANAGER_H_
