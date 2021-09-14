// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"

namespace {

// Synthetizes a `account_manager::Account` from a Gaia ID.
account_manager::Account AccountFromGaiaID(const std::string& gaia_id) {
  account_manager::Account account;
  account.key = {gaia_id, account_manager::AccountType::kGaia};
  // `account.raw_email` is left empty.
  return account;
}

}  // namespace

AccountProfileMapper::AccountProfileMapper(
    account_manager::AccountManagerFacade* facade,
    ProfileAttributesStorage* storage)
    : account_manager_facade_(facade), profile_attributes_storage_(storage) {
  DCHECK(profile_attributes_storage_);
  account_manager_facade_observation_.Observe(account_manager_facade_);
  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                     weak_factory_.GetWeakPtr()));
}

AccountProfileMapper::~AccountProfileMapper() = default;

void AccountProfileMapper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AccountProfileMapper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AccountProfileMapper::GetAccounts(
    const base::FilePath& profile_path,
    base::OnceCallback<void(const std::vector<account_manager::Account>&)>
        callback) {
  if (!initialized_) {
    initialization_callbacks_.push_back(base::BindOnce(
        &AccountProfileMapper::GetAccounts, weak_factory_.GetWeakPtr(),
        profile_path, std::move(callback)));
    return;
  }
  std::vector<account_manager::Account> accounts;
  const ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  // `entry` may be null during profile deletion, as it is destroyed before the
  // profile.
  if (entry) {
    for (const std::string& gaia_id : entry->GetGaiaIds())
      accounts.push_back(AccountFromGaiaID(gaia_id));
  }
  std::move(callback).Run(accounts);
}

void AccountProfileMapper::GetPersistentErrorForAccount(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback) {
  if (!initialized_) {
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountProfileMapper::GetPersistentErrorForAccount,
                       weak_factory_.GetWeakPtr(), profile_path, account,
                       std::move(callback)));
    return;
  }
  if (!ProfileContainsAccount(profile_path, account)) {
    std::move(callback).Run(
        GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
    return;
  }
  account_manager_facade_->GetPersistentErrorForAccount(account,
                                                        std::move(callback));
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountProfileMapper::CreateAccessTokenFetcher(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account,
    const std::string& oauth_consumer_name,
    OAuth2AccessTokenConsumer* consumer) {
  // TODO(https://crbug.com/1226045): Create a fetcher that can wait on
  // initialization of the class.
  if (!ProfileContainsAccount(profile_path, account)) {
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer,
        GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
  }

  return account_manager_facade_->CreateAccessTokenFetcher(
      account, oauth_consumer_name, consumer);
}

void AccountProfileMapper::OnAccountUpserted(
    const account_manager::Account& account) {
  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AccountProfileMapper::OnAccountRemoved(
    const account_manager::Account& account) {
  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                     weak_factory_.GetWeakPtr()));
}

std::vector<std::pair<base::FilePath, std::string>>
AccountProfileMapper::RemoveStaleAccounts(
    const base::flat_set<std::string>& system_gaia_ids) {
  std::vector<std::pair<base::FilePath, std::string>> removed_ids;
  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_->GetAllProfilesAttributes();
  // For each profile.
  for (ProfileAttributesEntry* entry : entries) {
    base::flat_set<std::string> entry_ids = entry->GetGaiaIds();
    bool entry_needs_update = false;
    // For each account in the profile.
    auto it = entry_ids.begin();
    while (it != entry_ids.end()) {
      if (system_gaia_ids.contains(*it)) {
        ++it;
      } else {
        // An account in the profile is no longer in the system.
        entry_needs_update = true;
        removed_ids.push_back(std::make_pair(entry->GetPath(), *it));
        it = entry_ids.erase(it);
      }
    }
    if (entry_needs_update)
      entry->SetGaiaIds(entry_ids);
  }
  return removed_ids;
}

std::vector<const account_manager::Account*>
AccountProfileMapper::AddNewGaiaAccounts(
    const std::vector<account_manager::Account>& system_accounts,
    ProfileAttributesEntry* entry_for_new_accounts) {
  // Compute the set of Gaia IDs in the profiles.
  base::flat_set<std::string> profile_gaia_ids;
  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_->GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::flat_set<std::string> entry_ids = entry->GetGaiaIds();
    profile_gaia_ids.insert(entry_ids.begin(), entry_ids.end());
  }
  // Diff that set against system accounts.
  std::vector<const account_manager::Account*> added_accounts;
  for (const account_manager::Account& account : system_accounts) {
    if (account.key.account_type == account_manager::AccountType::kGaia &&
        !profile_gaia_ids.contains(account.key.id)) {
      added_accounts.push_back(&account);
    }
  }
  // Update the `ProfileAttributesEntry`.
  if (entry_for_new_accounts) {
    base::flat_set<std::string> gaia_ids = entry_for_new_accounts->GetGaiaIds();
    for (const account_manager::Account* account : added_accounts)
      gaia_ids.insert(account->key.id);
    entry_for_new_accounts->SetGaiaIds(gaia_ids);
  }
  return added_accounts;
}

void AccountProfileMapper::OnGetAccountsCompleted(
    const std::vector<account_manager::Account>& system_accounts) {
  // Convert `system_accounts` to a set of Gaia IDs.
  std::vector<std::string> system_gaia_ids_list;
  for (const account_manager::Account& account : system_accounts) {
    const account_manager::AccountKey& key = account.key;
    // Filter out non-Gaia accounts.
    if (key.account_type != account_manager::AccountType::kGaia)
      continue;
    system_gaia_ids_list.push_back(account.key.id);
  }
  base::flat_set<std::string> system_gaia_ids(std::move(system_gaia_ids_list));
  // Accounts that were removed.
  std::vector<std::pair<base::FilePath, std::string>> removed_ids =
      RemoveStaleAccounts(system_gaia_ids);
  // Accounts that were added.
  ProfileAttributesEntry* entry_for_new_accounts =
      MaybeGetProfileForNewAccounts();
  std::vector<const account_manager::Account*> added_accounts =
      AddNewGaiaAccounts(system_accounts, entry_for_new_accounts);

  if (initialized_) {
    DCHECK(initialization_callbacks_.empty());
    // Call observers once all entries are updated.
    base::FilePath path_for_new_accounts;
    if (entry_for_new_accounts)
      path_for_new_accounts = entry_for_new_accounts->GetPath();
    for (auto& obs : observers_) {
      for (const auto* account : added_accounts) {
        DCHECK_EQ(account->key.account_type,
                  account_manager::AccountType::kGaia);
        obs.OnAccountUpserted(path_for_new_accounts, *account);
      }
      for (const auto& pair : removed_ids) {
        obs.OnAccountRemoved(pair.first, AccountFromGaiaID(pair.second));
      }
    }
  } else {
    initialized_ = true;
    for (auto& callback : initialization_callbacks_)
      std::move(callback).Run();
    initialization_callbacks_.clear();
  }
}

bool AccountProfileMapper::ProfileContainsAccount(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account) const {
  const ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  if (!entry)
    return false;
  return entry->GetGaiaIds().contains(account.id);
}

ProfileAttributesEntry* AccountProfileMapper::MaybeGetProfileForNewAccounts()
    const {
  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_->GetAllProfilesAttributes();
  // Ignore omitted profiles.
  base::EraseIf(entries,
                [](const auto* entry) -> bool { return entry->IsOmitted(); });
  DCHECK_GT(entries.size(), 0u);
  // If there are multiple profiles, leave new accounts unassigned.
  if (entries.size() > 1)
    return nullptr;
  // Otherwise auto-assign new accounts to the main profile.
  DCHECK(Profile::IsMainProfilePath(entries[0]->GetPath()));
  return entries[0];
}
