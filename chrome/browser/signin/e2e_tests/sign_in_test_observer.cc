// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sign_in_test_observer.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

namespace signin::test {

SignInTestObserver::SignInTestObserver(IdentityManager* identity_manager,
                                       AccountReconcilor* reconcilor,
                                       signin::ConsentLevel consent_level)
    : identity_manager_(identity_manager),
      reconcilor_(reconcilor),
      consent_level_(consent_level) {
  identity_manager_observation_.Observe(identity_manager_.get());
  account_reconcilor_observation_.Observe(reconcilor_.get());
}

SignInTestObserver::~SignInTestObserver() = default;

void SignInTestObserver::OnPrimaryAccountChanged(
    const PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(consent_level_) ==
      PrimaryAccountChangeEvent::Type::kNone) {
    return;
  }
  QuitIfConditionIsSatisfied();
}
void SignInTestObserver::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo&) {
  QuitIfConditionIsSatisfied();
}
void SignInTestObserver::OnRefreshTokenRemovedForAccount(const CoreAccountId&) {
  QuitIfConditionIsSatisfied();
}
void SignInTestObserver::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo&,
    const GoogleServiceAuthError&,
    signin_metrics::SourceForRefreshTokenOperation) {
  QuitIfConditionIsSatisfied();
}
void SignInTestObserver::OnAccountsInCookieUpdated(
    const AccountsInCookieJarInfo&,
    const GoogleServiceAuthError&) {
  QuitIfConditionIsSatisfied();
}

// TODO(crbug.com/40673982): Remove this observer method once the bug is
// fixed.
void SignInTestObserver::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (state == signin_metrics::AccountReconcilorState::kOk) {
    // This will trigger cookie update if accounts are stale.
    identity_manager_->GetAccountsInCookieJar();
  }
}

void SignInTestObserver::WaitForAccountChanges(
    int signed_in_accounts,
    PrimaryAccountWait primary_account_wait) {
  expected_signed_in_accounts_ = signed_in_accounts;
  primary_account_wait_ = primary_account_wait;
  are_expectations_set = true;
  QuitIfConditionIsSatisfied();
  run_loop_.Run();
}

void SignInTestObserver::QuitIfConditionIsSatisfied() {
  if (!are_expectations_set)
    return;

  int accounts_with_valid_refresh_token = CountAccountsWithValidRefreshToken();
  int accounts_in_cookie = CountSignedInAccountsInCookie();

  if (accounts_with_valid_refresh_token != accounts_in_cookie ||
      accounts_with_valid_refresh_token != expected_signed_in_accounts_) {
    return;
  }

  switch (primary_account_wait_) {
    case PrimaryAccountWait::kWaitForAdded:
      if (!HasValidPrimaryAccount()) {
        return;
      }
      break;
    case PrimaryAccountWait::kWaitForCleared:
      if (identity_manager_->HasPrimaryAccount(consent_level_)) {
        return;
      }
      break;
    case PrimaryAccountWait::kNotWait:
      break;
  }

  run_loop_.Quit();
}

int SignInTestObserver::CountAccountsWithValidRefreshToken() const {
  std::vector<CoreAccountInfo> accounts_with_refresh_tokens =
      identity_manager_->GetAccountsWithRefreshTokens();
  int valid_accounts = 0;
  for (const auto& account_info : accounts_with_refresh_tokens) {
    if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info.account_id)) {
      ++valid_accounts;
    }
  }
  return valid_accounts;
}

int SignInTestObserver::CountSignedInAccountsInCookie() const {
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager_->GetAccountsInCookieJar();
  if (!accounts_in_cookie_jar.AreAccountsFresh()) {
    return -1;
  }

  return accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts().size();
}

bool SignInTestObserver::HasValidPrimaryAccount() const {
  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(consent_level_);
  if (primary_account_id.empty())
    return false;

  return !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
      primary_account_id);
}

}  // namespace signin::test
