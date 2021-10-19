// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/add_account_helper.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

AddAccountHelper::AddAccountHelper(
    account_manager::AccountManagerFacade* facade,
    ProfileAttributesStorage* storage)
    : account_manager_facade_(facade), profile_attributes_storage_(storage) {
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
    OnShowAddAccountDialogCompleted(
        profile_path,
        account_manager::AccountAdditionResult::FromAccount(*account));
  } else {
    account_manager_facade_->ShowAddAccountDialog(
        absl::get<account_manager::AccountManagerFacade::AccountAdditionSource>(
            source_or_account),
        base::BindOnce(&AddAccountHelper::OnShowAddAccountDialogCompleted,
                       weak_factory_.GetWeakPtr(), profile_path));
  }
}

void AddAccountHelper::OnShowAddAccountDialogCompleted(
    const base::FilePath& profile_path,
    const account_manager::AccountAdditionResult& result) {
  bool add_account_failure =
      result.status() !=
          account_manager::AccountAdditionResult::Status::kSuccess ||
      result.account()->key.account_type() !=
          account_manager::AccountType::kGaia;
  if (add_account_failure) {
    std::move(callback_).Run(absl::nullopt);
    // `this` may be deleted.
    return;
  }

  if (profile_path.empty()) {
    // If the profile does not exist, create it first.
    size_t icon_index = profiles::GetPlaceholderAvatarIndex();
    ProfileManager::CreateMultiProfileAsync(
        profile_attributes_storage_->ChooseNameForNewProfile(icon_index),
        icon_index,
        /*is_hidden=*/true,
        base::BindRepeating(&AddAccountHelper::OnNewProfileCreated,
                            weak_factory_.GetWeakPtr(),
                            result.account().value()));
  } else {
    OnShowAddAccountDialogCompletedWithProfilePath(profile_path,
                                                   result.account().value());
  }
}

void AddAccountHelper::OnNewProfileCreated(
    const account_manager::Account& account,
    Profile* new_profile,
    Profile::CreateStatus status) {
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      // Ignore this, wait for profile to be initialized.
      return;
    case Profile::CREATE_STATUS_INITIALIZED:
      OnShowAddAccountDialogCompletedWithProfilePath(new_profile->GetPath(),
                                                     account);
      return;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
      NOTREACHED() << "Error creating new profile";
      std::move(callback_).Run(
          AccountProfileMapper::AddAccountResult{base::FilePath(), account});
      // `this` may be deleted.
      return;
  }
}

void AddAccountHelper::OnShowAddAccountDialogCompletedWithProfilePath(
    const base::FilePath& profile_path,
    const account_manager::Account& account) {
  DCHECK(!profile_path.empty());
  DCHECK_EQ(account.key.account_type(), account_manager::AccountType::kGaia);
  const std::string& gaia_id = account.key.id();
  DCHECK(!gaia_id.empty());
  absl::optional<AccountProfileMapper::AddAccountResult> result;

  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  if (entry) {
    base::flat_set<std::string> profile_accounts = entry->GetGaiaIds();
    auto inserted = profile_accounts.insert(gaia_id);
    if (inserted.second) {
      entry->SetGaiaIds(profile_accounts);
      result = {profile_path, account};
    } else {
      // Duplicate account, this was a no-op.
      LOG(ERROR) << "Account " << account.raw_email
                 << " already exists in profile " << profile_path;
    }
  } else {
    // If the profile does not exist, the account is left unassigned.
    result = {base::FilePath(), account};
  }
  std::move(callback_).Run(result);
  // `this` may be deleted.
}
