// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/keyed_service/core/keyed_service.h"

// This is a profile-scoped implementation of AccountManagerFacade, intended to
// be used by the identity manager. Accounts update generally follow the path:
//
//                       AccountManagerFacadeImpl
//                                  |
//                                  V
//                         AccountProfileMapper
//                                  |
//                                  V
//                         ProfileAccountManager
//                                  |
//                                  V
//                            IdentityManager
//
// The ProfileAccountManager is not intended to have much logic and mostly
// forwards calls to the AccountProfileMapper.
class ProfileAccountManager : public KeyedService,
                              public account_manager::AccountManagerFacade {
 public:
  explicit ProfileAccountManager(const base::FilePath& profile_path);
  ~ProfileAccountManager() override;

  ProfileAccountManager(const ProfileAccountManager&) = delete;
  ProfileAccountManager& operator=(const ProfileAccountManager&) = delete;

  // KeyedService:
  void Shutdown() override;

  // account_manager::AccountManagerFacade:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetAccounts(
      base::OnceCallback<void(const std::vector<account_manager::Account>&)>
          callback) override;
  void GetPersistentErrorForAccount(
      const account_manager::AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback)
      override;
  void ShowAddAccountDialog(const AccountAdditionSource& source) override;
  void ShowAddAccountDialog(
      const AccountAdditionSource& source,
      base::OnceCallback<void(const account_manager::AccountAdditionResult&
                                  result)> callback) override;
  void ShowReauthAccountDialog(const AccountAdditionSource& source,
                               const std::string& email) override;
  void ShowManageAccountsSettings() override;
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const account_manager::AccountKey& account,
      const std::string& oauth_consumer_name,
      OAuth2AccessTokenConsumer* consumer) override;

 private:
  // Returns the `AccountManagerFacade` for the whole browser, managing accounts
  // across all profiles.
  account_manager::AccountManagerFacade* GetSystemAccountManager() const;

  const base::FilePath profile_path_;
  base::ObserverList<account_manager::AccountManagerFacade::Observer>
      observers_;
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_
