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

  DCHECK(!account || !account->IsEmpty());
  if (account) {
    if (identity_manager_->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin) != account) {
      DCHECK(
          !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
      identity_manager_->GetPrimaryAccountMutator()
          ->SetUnconsentedPrimaryAccount(account->account_id);
    }
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    DCHECK(!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
    identity_manager_->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::USER_DELETED_ACCOUNT_COOKIES,
        signin_metrics::SignoutDelete::kIgnoreMetric);
  }
}

base::Optional<CoreAccountInfo>
SigninManager::ComputeUnconsentedPrimaryAccountInfo() const {
  // UPA is equal to the primary account with sync consent if it exists.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return identity_manager_->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSync);
  }

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
      return base::nullopt;
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

    return error_state ? base::nullopt : account_info;
  }

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin))
    return base::nullopt;

  // If cookies or tokens are not loaded, it is not possible to fully compute
  // the unconsented primary account. However, if the current unconsented
  // primary account is no longer valid, it has to be removed.
  CoreAccountId current_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  if (are_refresh_tokens_loaded &&
      !identity_manager_->HasAccountWithRefreshToken(current_account)) {
    // Tokens are loaded, but the current UPA doesn't have a refresh token.
    // Clear the current UPA.
    return base::nullopt;
  }

  if (!are_refresh_tokens_loaded &&
      unconsented_primary_account_revoked_during_load_) {
    // Tokens are not loaded, but the current UPA's refresh token has been
    // revoked. Clear the current UPA.
    return base::nullopt;
  }

  if (cookie_info.accounts_are_fresh) {
    if (cookie_accounts.empty() || cookie_accounts[0].id != current_account) {
      // The current UPA is not the first in fresh cookies. It needs to be
      // cleared.
      return base::nullopt;
    }
  }

  // No indication that the current UPA is invalid, return current UPA.
  return identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

// signin::IdentityManager::Observer implementation.
void SigninManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // This is needed for the case where the user chooses to start syncing
  // with an account that is different from the unconsented primary account
  // (not the first in cookies) but then cancels. In that case, the tokens stay
  // the same. In all the other cases, either the token will be revoked which
  // will trigger an update for the unconsented primary account or the
  // primary account stays the same but the sync consent is revoked.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSync) !=
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    return;
  }

  // It is important to update the primary account after all observers process
  // the current OnPrimaryAccountChanged() as all observers should see the same
  // value for the unconsented primary account. Schedule the potential update
  // on the next run loop.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SigninManager::UpdateUnconsentedPrimaryAccount,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SigninManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if (!identity_manager_->AreRefreshTokensLoaded() &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
      account_id == identity_manager_->GetPrimaryAccountId(
                        signin::ConsentLevel::kSignin)) {
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
  CoreAccountInfo current_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

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
