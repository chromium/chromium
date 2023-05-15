// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/keyed_service/core/keyed_service.h"

// This is a profile-scoped implementation of `AccountManagerFacade`, intended
// to be used by the identity manager. Account updates generally follow the
// path:
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
// The `ProfileAccountManager` is not intended to have much logic and mostly
// forwards calls to the `AccountProfileMapper`.
class ProfileAccountManager : public KeyedService,
                              public account_manager::AccountManagerFacade,
                              public AccountProfileMapper::Observer {
 public:
  // `mapper` must outlive this object.
  ProfileAccountManager(AccountProfileMapper* mapper,
                        const base::FilePath& profile_path);
  ~ProfileAccountManager() override;

  ProfileAccountManager(const ProfileAccountManager&) = delete;
  ProfileAccountManager& operator=(const ProfileAccountManager&) = delete;

  // KeyedService:
  void Shutdown() override;

  // AccountProfileMapper::Observer:
  void OnAccountUpserted(const base::FilePath& profile_path,
                         const account_manager::Account& account) override;
  void OnAccountRemoved(const base::FilePath& profile_path,
                        const account_manager::Account& account) override;
  void OnAuthErrorChanged(const base::FilePath& profile_path,
                          const account_manager::AccountKey& account,
                          const GoogleServiceAuthError& error) override;

  // account_manager::AccountManagerFacade:
  void AddObserver(
      account_manager::AccountManagerFacade::Observer* observer) override;
  void RemoveObserver(
      account_manager::AccountManagerFacade::Observer* observer) override;
  void GetAccounts(
      base::OnceCallback<void(const std::vector<account_manager::Account>&)>
          callback) override;
  void GetPersistentErrorForAccount(
      const account_manager::AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback)
      override;
  void ShowAddAccountDialog(AccountAdditionSource source) override;
  void ShowAddAccountDialog(
      AccountAdditionSource source,
      base::OnceCallback<void(const account_manager::AccountUpsertionResult&
                                  result)> callback) override;
  void ShowReauthAccountDialog(
      AccountAdditionSource source,
      const std::string& email,
      base::OnceCallback<void(const account_manager::AccountUpsertionResult&
                                  result)> callback) override;
  void ShowManageAccountsSettings() override;
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const account_manager::AccountKey& account,
      OAuth2AccessTokenConsumer* consumer) override;
  void ReportAuthError(const account_manager::AccountKey& account,
                       const GoogleServiceAuthError& error) override;
  void UpsertAccountForTesting(const account_manager::Account& account,
                               const std::string& token_value) override;
  void RemoveAccountForTesting(
      const account_manager::AccountKey& account) override;

 private:
  const raw_ptr<AccountProfileMapper> mapper_;
  const base::FilePath profile_path_;
  base::ObserverList<account_manager::AccountManagerFacade::Observer>
      observers_;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      mapper_observation_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_
