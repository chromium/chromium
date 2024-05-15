// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/add_account_helper.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_upsertion_result.h"

AddAccountHelper::AddAccountHelper(
    IsAccountInCacheCallback is_account_in_cache_callback,
    account_manager::AccountManagerFacade* facade,
    ProfileAttributesStorage* storage)
    : is_account_in_cache_callback_(std::move(is_account_in_cache_callback)),
      account_manager_facade_(facade),
      profile_attributes_storage_(storage) {
  DCHECK(is_account_in_cache_callback_);
  DCHECK(account_manager_facade_);
  DCHECK(profile_attributes_storage_);
}

AddAccountHelper::~AddAccountHelper() = default;

void AddAccountHelper::Start(
    const base::FilePath& profile_path,
    const absl::variant<
        account_manager::AccountManagerFacade::AccountAdditionSource,
        account_manager::Account>& source_or_account,
    AccountProfileMapper::AddAccountCallback callback) {
  DCHECK(!callback_) << "Start() must be called only once.";
  callback_ = std::move(callback);
  DCHECK(callback_);

  if (const account_manager::Account* account =
          absl::get_if<account_manager::Account>(&source_or_account)) {
    DCHECK(is_account_in_cache_callback_.Run(*account))
        << "Cannot add an unknown account " << account->raw_email
        << " to profile " << profile_path;
    OnShowAddAccountDialogCompleted(
        profile_path,
        account_manager::AccountUpsertionResult::FromAccount(*account));
  } else {
    account_manager_facade_->ShowAddAccountDialog(
        absl::get<account_manager::AccountManagerFacade::AccountAdditionSource>(
            source_or_account),
        base::BindOnce(&AddAccountHelper::OnShowAddAccountDialogCompleted,
                       weak_factory_.GetWeakPtr(), profile_path));
  }
}

void AddAccountHelper::UpsertAccountForTesting(
    const base::FilePath& profile_path,
    const account_manager::Account& account,
    const std::string& token_value,
    AccountProfileMapper::AddAccountCallback callback) {
  DCHECK(!callback_)
      << "UpsertAccountForTesting() must be called only once.";  // IN-TEST
  callback_ = std::move(callback);
  DCHECK(callback_);

  account_manager_facade_->UpsertAccountForTesting(  // IN-TEST
      account, token_value);
  OnShowAddAccountDialogCompleted(
      profile_path,
      account_manager::AccountUpsertionResult::FromAccount(account));
}

void AddAccountHelper::OnAccountCacheUpdated() {
  if (!account_ || is_account_in_cache_)
    return;

  is_account_in_cache_ = is_account_in_cache_callback_.Run(*account_);

  MaybeCompleteAddAccount();
  // `this` may be deleted.
}

void AddAccountHelper::OnShowAddAccountDialogCompleted(
    const base::FilePath& profile_path,
    const account_manager::AccountUpsertionResult& result) {
  DCHECK(!account_);

  bool add_account_failure =
      result.status() !=
          account_manager::AccountUpsertionResult::Status::kSuccess ||
      result.account()->key.account_type() !=
          account_manager::AccountType::kGaia;
  if (add_account_failure) {
    std::move(callback_).Run(std::nullopt);
    // `this` may be deleted.
    return;
  }

  account_ = result.account().value();
  is_account_in_cache_ = is_account_in_cache_callback_.Run(*account_);

  if (profile_path.empty()) {
    // If the profile does not exist, create it first.
    size_t icon_index = profiles::GetPlaceholderAvatarIndex();
    ProfileManager::CreateMultiProfileAsync(
        profile_attributes_storage_->ChooseNameForNewProfile(icon_index),
        icon_index,
        /*is_hidden=*/true,
        base::BindOnce(&AddAccountHelper::OnNewProfileInitialized,
                       weak_factory_.GetWeakPtr()));
  } else {
    OnShowAddAccountDialogCompletedWithProfilePath(profile_path);
  }
}

void AddAccountHelper::OnNewProfileInitialized(Profile* new_profile) {
  DCHECK(account_);
  if (!new_profile) {
    NOTREACHED_IN_MIGRATION() << "Error creating new profile";
    profile_path_ = base::FilePath();
    MaybeCompleteAddAccount();
    // `this` may be deleted.
    return;
  }

  OnShowAddAccountDialogCompletedWithProfilePath(new_profile->GetPath());
}

void AddAccountHelper::OnShowAddAccountDialogCompletedWithProfilePath(
    const base::FilePath& profile_path) {
  DCHECK(account_);
  DCHECK(!profile_path.empty());
  profile_path_ = profile_path;

  MaybeCompleteAddAccount();
  // `this` may be deleted.
}

void AddAccountHelper::MaybeCompleteAddAccount() {
  if (!account_ || !profile_path_ || !is_account_in_cache_)
    return;

  DCHECK_EQ(account_->key.account_type(), account_manager::AccountType::kGaia);
  const std::string& gaia_id = account_->key.id();
  DCHECK(!gaia_id.empty());
  std::optional<AccountProfileMapper::AddAccountResult> result;

  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(
          profile_path_.value_or(base::FilePath()));
  if (entry) {
    base::flat_set<std::string> profile_accounts = entry->GetGaiaIds();
    auto inserted = profile_accounts.insert(gaia_id);
    if (inserted.second) {
      entry->SetGaiaIds(profile_accounts);
      result = {*profile_path_, *account_};
    } else {
      // Duplicate account, this was a no-op.
      LOG(ERROR) << "Account " << account_->raw_email
                 << " already exists in profile " << *profile_path_;
    }
  } else {
    // If the profile does not exist, the account is left unassigned.
    result = {base::FilePath(), *account_};
  }
  std::move(callback_).Run(result);
  // `this` may be deleted.
}
