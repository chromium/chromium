// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/profile_account_manager.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

ProfileAccountManager::ProfileAccountManager(AccountProfileMapper* mapper,
                                             const base::FilePath& profile_path)
    : mapper_(mapper), profile_path_(profile_path) {
  DCHECK(mapper_);
  mapper_observation_.Observe(mapper_.get());
}

ProfileAccountManager::~ProfileAccountManager() = default;

void ProfileAccountManager::AddObserver(
    account_manager::AccountManagerFacade::Observer* observer) {
  observers_.AddObserver(observer);
}

void ProfileAccountManager::RemoveObserver(
    account_manager::AccountManagerFacade::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProfileAccountManager::Shutdown() {
  DCHECK(observers_.empty());
  mapper_observation_.Reset();
}

void ProfileAccountManager::OnAccountUpserted(
    const base::FilePath& profile_path,
    const account_manager::Account& account) {
  DCHECK_EQ(account.key.account_type(), account_manager::AccountType::kGaia);
  if (profile_path != profile_path_)
    return;
  for (auto& obs : observers_)
    obs.OnAccountUpserted(account);
}

void ProfileAccountManager::OnAccountRemoved(
    const base::FilePath& profile_path,
    const account_manager::Account& account) {
  if (profile_path != profile_path_)
    return;
  for (auto& obs : observers_)
    obs.OnAccountRemoved(account);
}

void ProfileAccountManager::OnAuthErrorChanged(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(account.account_type(), account_manager::AccountType::kGaia);
  if (profile_path != profile_path_)
    return;
  for (auto& obs : observers_)
    obs.OnAuthErrorChanged(account, error);
}

void ProfileAccountManager::GetAccounts(
    base::OnceCallback<void(const std::vector<account_manager::Account>&)>
        callback) {
  DCHECK(!callback.is_null());
  mapper_->GetAccounts(profile_path_, std::move(callback));
}

void ProfileAccountManager::GetPersistentErrorForAccount(
    const account_manager::AccountKey& account,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback) {
  DCHECK(!callback.is_null());
  return mapper_->GetPersistentErrorForAccount(profile_path_, account,
                                               std::move(callback));
}

void ProfileAccountManager::ShowAddAccountDialog(AccountAdditionSource source) {
  NOTREACHED_IN_MIGRATION();
}

void ProfileAccountManager::ShowAddAccountDialog(
    AccountAdditionSource source,
    base::OnceCallback<
        void(const account_manager::AccountUpsertionResult& result)> callback) {
  NOTREACHED_IN_MIGRATION();
}

void ProfileAccountManager::ShowReauthAccountDialog(
    AccountAdditionSource source,
    const std::string& email,
    base::OnceCallback<
        void(const account_manager::AccountUpsertionResult& result)> callback) {
  NOTREACHED_IN_MIGRATION();
}

void ProfileAccountManager::ShowManageAccountsSettings() {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileAccountManager::CreateAccessTokenFetcher(
    const account_manager::AccountKey& account,

    OAuth2AccessTokenConsumer* consumer) {
  return mapper_->CreateAccessTokenFetcher(profile_path_, account, consumer);
}

void ProfileAccountManager::ReportAuthError(
    const account_manager::AccountKey& account,
    const GoogleServiceAuthError& error) {
  mapper_->ReportAuthError(profile_path_, account, error);
}

void ProfileAccountManager::UpsertAccountForTesting(
    const account_manager::Account& account,
    const std::string& token_value) {
  mapper_->UpsertAccountForTesting(  // IN-TEST
      profile_path_, account, token_value);
}

void ProfileAccountManager::RemoveAccountForTesting(
    const account_manager::AccountKey& account) {
  mapper_->RemoveAccountForTesting(profile_path_, account);  // IN-TEST
}
