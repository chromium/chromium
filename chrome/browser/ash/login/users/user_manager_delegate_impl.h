// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_USER_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_USER_MANAGER_DELEGATE_IMPL_H_

#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "components/user_manager/user_manager_base.h"

namespace ash {

// Implementation of UserManagerBase::Delegate to inject //chrome/* behavior.
class UserManagerDelegateImpl : public user_manager::UserManagerBase::Delegate {
 public:
  UserManagerDelegateImpl();
  UserManagerDelegateImpl(const UserManagerDelegateImpl&) = delete;
  UserManagerDelegateImpl& operator=(const UserManagerDelegateImpl&) = delete;
  ~UserManagerDelegateImpl() override;

  // UserManagerBase::Delegate:
  const std::string& GetApplicationLocale() override;
  void OverrideDirHome(const user_manager::User& primary_user) override;
  bool IsUserSessionRestoreInProgress() override;
  void CheckProfileOnLogin(const user_manager::User& user) override;
  void RemoveProfileByAccountId(const AccountId& account_id) override;
  void RemoveCryptohomeAsync(const AccountId& account_id) override;

 private:
  MountPerformer mount_performer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_USER_MANAGER_DELEGATE_IMPL_H_
