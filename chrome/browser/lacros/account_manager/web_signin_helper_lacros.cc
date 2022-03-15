// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/web_signin_helper_lacros.h"

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "chrome/browser/lacros/account_manager/account_manager_util.h"
#include "chrome/browser/ui/profile_picker.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"

WebSigninHelperLacros::WebSigninHelperLacros(
    const base::FilePath& profile_path,
    AccountProfileMapper* account_profile_mapper,
    signin::IdentityManager* identity_manager,
    signin::ConsistencyCookieManager* consistency_cookie_manager,
    base::OnceClosure callback)
    : callback_(std::move(callback)),
      profile_path_(profile_path),
      account_profile_mapper_(account_profile_mapper),
      identity_manager_(identity_manager) {
  if (base::FeatureList::IsEnabled(switches::kLacrosNonSyncingProfiles)) {
    DCHECK(consistency_cookie_manager);
    scoped_account_update_ =
        std::make_unique<signin::ConsistencyCookieManager::ScopedAccountUpdate>(
            consistency_cookie_manager->CreateScopedAccountUpdate());
  }

  GetAccountsAvailableAsSecondary(
      account_profile_mapper, profile_path,
      base::BindOnce(
          &WebSigninHelperLacros::OnAccountsAvailableAsSecondaryFetched,
          weak_factory_.GetWeakPtr()));
}

WebSigninHelperLacros::~WebSigninHelperLacros() {
  Finalize();
}

void WebSigninHelperLacros::OnAccountsAvailableAsSecondaryFetched(
    const std::vector<account_manager::Account>& accounts) {
  if (!accounts.empty()) {
    // Pass in the current profile to signal that the user wants to select a
    // _secondary_ account for this particular profile.
    ProfilePicker::Show(ProfilePicker::Params::ForLacrosSelectAvailableAccount(
        profile_path_, base::BindOnce(&WebSigninHelperLacros::OnAccountPicked,
                                      weak_factory_.GetWeakPtr())));
    return;
  }

  account_profile_mapper_->ShowAddAccountDialog(
      profile_path_,
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kOgbAddAccount,
      base::BindOnce(&WebSigninHelperLacros::OnAccountAdded,
                     weak_factory_.GetWeakPtr()));
}

void WebSigninHelperLacros::OnAccountAdded(
    const absl::optional<AccountProfileMapper::AddAccountResult>& result) {
  std::string gaia_id;
  if (result.has_value() && result->account.key.account_type() ==
                                account_manager::AccountType::kGaia) {
    gaia_id = result->account.key.id();
  }
  OnAccountPicked(gaia_id);
}

void WebSigninHelperLacros::OnAccountPicked(const std::string& gaia_id) {
  if (gaia_id.empty()) {
    Finalize();
    return;
  }

  std::vector<CoreAccountInfo> accounts_in_tokens =
      identity_manager_->GetAccountsWithRefreshTokens();
  if (base::Contains(accounts_in_tokens, gaia_id,
                     [](const CoreAccountInfo& info) { return info.gaia; })) {
    // Account is already in tokens.
    Finalize();
    return;
  }

  // Wait for account to be in tokens.
  account_added_to_mapping_ = gaia_id;
  identity_manager_observervation_.Observe(identity_manager_);
}

void WebSigninHelperLacros::Finalize() {
  identity_manager_observervation_.Reset();
  scoped_account_update_.reset();
  if (callback_)
    std::move(callback_).Run();
  // `this` may be deleted.
}

void WebSigninHelperLacros::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_added_to_mapping_ == account_info.gaia) {
    Finalize();
    return;
  }
}
