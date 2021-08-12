// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/profile_account_manager.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/account_manager_facade_factory.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

ProfileAccountManager::ProfileAccountManager(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

ProfileAccountManager::~ProfileAccountManager() = default;

void ProfileAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  // TODO(https://crbug.com/1226045): Go through AccountProfileMapper instead.
  GetSystemAccountManager()->AddObserver(observer);
}

void ProfileAccountManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  // TODO(https://crbug.com/1226045): Go through AccountProfileMapper instead.
  GetSystemAccountManager()->RemoveObserver(observer);
}

void ProfileAccountManager::Shutdown() {
  DCHECK(observers_.empty());
}

void ProfileAccountManager::GetAccounts(
    base::OnceCallback<void(const std::vector<account_manager::Account>&)>
        callback) {
  DCHECK(!callback.is_null());
  GetSystemAccountManager()->GetAccounts(std::move(callback));
}

void ProfileAccountManager::GetPersistentErrorForAccount(
    const account_manager::AccountKey& account,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback) {
  DCHECK(!callback.is_null());
  return GetSystemAccountManager()->GetPersistentErrorForAccount(
      account, std::move(callback));
}

void ProfileAccountManager::ShowAddAccountDialog(
    const AccountAdditionSource& source) {
  NOTREACHED();
}

void ProfileAccountManager::ShowAddAccountDialog(
    const AccountAdditionSource& source,
    base::OnceCallback<
        void(const account_manager::AccountAdditionResult& result)> callback) {
  NOTREACHED();
}

void ProfileAccountManager::ShowReauthAccountDialog(
    const AccountAdditionSource& source,
    const std::string& email) {
  NOTREACHED();
}

void ProfileAccountManager::ShowManageAccountsSettings() {
  NOTREACHED();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileAccountManager::CreateAccessTokenFetcher(
    const account_manager::AccountKey& account,
    const std::string& oauth_consumer_name,
    OAuth2AccessTokenConsumer* consumer) {
  return GetSystemAccountManager()->CreateAccessTokenFetcher(
      account, oauth_consumer_name, consumer);
}

account_manager::AccountManagerFacade*
ProfileAccountManager::GetSystemAccountManager() const {
  return GetAccountManagerFacade(profile_path_.value());
}
