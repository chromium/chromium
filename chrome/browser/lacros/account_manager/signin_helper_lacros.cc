// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/signin_helper_lacros.h"

#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/lacros/account_manager/account_manager_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"

SigninHelperLacros::SigninHelperLacros(
    const base::FilePath& profile_path,
    AccountProfileMapper* account_profile_mapper,
    signin::IdentityManager* identity_manager,
    signin::ConsistencyCookieManager* consistency_cookie_manager,
    account_manager::AccountManagerFacade::AccountAdditionSource source,
    base::OnceCallback<void(const CoreAccountId&)> callback)
    : callback_(std::move(callback)),
      profile_path_(profile_path),
      account_profile_mapper_(account_profile_mapper),
      source_(source),
      identity_manager_(identity_manager) {
  DCHECK(consistency_cookie_manager);
  scoped_account_update_ =
      std::make_unique<signin::ConsistencyCookieManager::ScopedAccountUpdate>(
          consistency_cookie_manager->CreateScopedAccountUpdate());

  GetAllAvailableAccounts(
      account_profile_mapper, profile_path,
      base::BindOnce(&SigninHelperLacros::OnAccountsAvailableAsSecondaryFetched,
                     weak_factory_.GetWeakPtr()));
}

SigninHelperLacros::~SigninHelperLacros() {
  Finalize(CoreAccountId());
}

void SigninHelperLacros::OnAccountsAvailableAsSecondaryFetched(
    const std::vector<account_manager::Account>& accounts) {
  if (!accounts.empty()) {
    // Pass in the current profile to signal that the user wants to select a
    // _secondary_ account for this particular profile.
    ProfilePicker::Show(ProfilePicker::Params::ForLacrosSelectAvailableAccount(
        profile_path_, base::BindOnce(&SigninHelperLacros::OnAccountPicked,
                                      weak_factory_.GetWeakPtr())));
    return;
  }

  account_profile_mapper_->ShowAddAccountDialog(
      profile_path_, source_,
      base::BindOnce(&SigninHelperLacros::OnAccountAdded,
                     weak_factory_.GetWeakPtr()));
}

void SigninHelperLacros::OnAccountAdded(
    const std::optional<AccountProfileMapper::AddAccountResult>& result) {
  std::string gaia_id;
  if (result.has_value() && result->account.key.account_type() ==
                                account_manager::AccountType::kGaia) {
    gaia_id = result->account.key.id();
  }
  OnAccountPicked(gaia_id);
}

void SigninHelperLacros::OnAccountPicked(const std::string& gaia_id) {
  if (gaia_id.empty()) {
    Finalize(CoreAccountId());
    return;
  }

  std::vector<CoreAccountInfo> accounts_in_tokens =
      identity_manager_->GetAccountsWithRefreshTokens();
  auto it =
      base::ranges::find(accounts_in_tokens, gaia_id, &CoreAccountInfo::gaia);
  if (it != accounts_in_tokens.end()) {
    // Account is already in tokens.
    Finalize(it->account_id);
    return;
  }

  // Wait for account to be in tokens.
  account_added_to_mapping_ = gaia_id;
  identity_manager_observervation_.Observe(identity_manager_.get());
}

void SigninHelperLacros::Finalize(const CoreAccountId& account_id) {
  identity_manager_observervation_.Reset();
  scoped_account_update_.reset();
  if (callback_)
    std::move(callback_).Run(account_id);
  // `this` may be deleted.
}

void SigninHelperLacros::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_added_to_mapping_ == account_info.gaia) {
    Finalize(account_info.account_id);
    return;
  }
}
