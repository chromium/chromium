// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

SigninManager::SigninManager(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  if (identity_manager_->AreRefreshTokensLoaded()) {
    UpdateUnconsentedPrimaryAccount();
  }

  identity_manager_->AddObserver(this);
}

SigninManager::~SigninManager() {
  identity_manager_->RemoveObserver(this);
}

void SigninManager::UpdateUnconsentedPrimaryAccount() {
  base::Optional<CoreAccountInfo> account =
      ComputeUnconsentedPrimaryAccountInfo();
  if (!account)
    return;

  identity_manager_->GetPrimaryAccountMutator()->SetUnconsentedPrimaryAccount(
      account->account_id);
}

base::Optional<CoreAccountInfo>
SigninManager::ComputeUnconsentedPrimaryAccountInfo() const {
  // UPA is equal to the primary account with sync consent if it exists.
  if (identity_manager_->HasPrimaryAccount())
    return identity_manager_->GetPrimaryAccountInfo();

  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager_->GetAccountsInCookieJar();

  std::vector<gaia::ListedAccount> cookie_accounts =
      cookie_info.signed_in_accounts;

  bool are_refresh_tokens_loaded = identity_manager_->AreRefreshTokensLoaded();

  // Fresh cookies and loaded tokens are needed to compute the UPA.
  if (are_refresh_tokens_loaded && cookie_info.accounts_are_fresh) {
    // Cookies are fresh and tokens are loaded, UPA is the first account
    // in cookies if it exists and has a refresh token.
    if (cookie_accounts.empty()) {
      // Cookies are empty, the UPA is empty.
      return CoreAccountInfo();
    }

    base::Optional<AccountInfo> account_info =
        identity_manager_
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
                cookie_accounts[0].id);

    // Verify the first account in cookies has a refresh token that is valid.
    bool error_state =
        !account_info.has_value() ||
        identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info->account_id);

    return error_state ? AccountInfo() : account_info;
  }

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kNotRequired))
    return base::nullopt;

  // If cookies or tokens are not loaded, it is not possible to fully compute
  // the unconsented primary account. However, if the current unconsented
  // primary account is no longer valid, it has to be removed.
  CoreAccountId current_account = identity_manager_->GetPrimaryAccountId(
      signin::ConsentLevel::kNotRequired);

  if (are_refresh_tokens_loaded &&
      !identity_manager_->HasAccountWithRefreshToken(current_account)) {
    // Tokens are loaded, but the current UPA doesn't have a refresh token.
    // Clear the current UPA.
    return CoreAccountInfo();
  }

  if (!are_refresh_tokens_loaded &&
      unconsented_primary_account_revoked_during_load_) {
    // Tokens are not loaded, but the current UPA's refresh token has been
    // revoked. Clear the current UPA.
    return CoreAccountInfo();
  }

  if (cookie_info.accounts_are_fresh) {
    if (cookie_accounts.empty() || cookie_accounts[0].id != current_account) {
      // The current UPA is not the first in fresh cookies. It needs to be
      // cleared.
      return CoreAccountInfo();
    }
  }

  // No indication that the current UPA is invalid, return no op.
  return base::nullopt;
}

// signin::IdentityManager::Observer implementation.
void SigninManager::AfterSyncPrimaryAccountCleared() {
  // This is needed for the case where the user chooses to start syncing
  // with an account that is different from the unconsented primary account
  // (not the first in cookies) but then cancels. In that case, the tokens stay
  // the same. In all the other cases, either the token will be revoked which
  // will trigger an update for the unconsented primary account or the
  // primary account stays the same but the sync consent is revoked.
  // |OnPrimaryAccountCleared| is not used to ensure the value of the
  // unconsented primary account doesn't change during other observers being
  // notified. All observers should see the same value for the unconsented
  // primary account.
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if (!identity_manager_->AreRefreshTokensLoaded() &&
      identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired) &&
      account_id == identity_manager_->GetPrimaryAccountId(
                        signin::ConsentLevel::kNotRequired)) {
    unconsented_primary_account_revoked_during_load_ = true;
  }
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokensLoaded() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnAccountsCookieDeletedByUserAction() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  CoreAccountInfo current_account = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  bool should_update = false;
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    should_update = current_account.IsEmpty();
  } else {
    // In error state, update if the account in error is the current UPA.
    should_update = (account_info == current_account);
  }

  if (should_update)
    UpdateUnconsentedPrimaryAccount();
}
