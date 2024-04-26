// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_SIGN_IN_TEST_OBSERVER_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_SIGN_IN_TEST_OBSERVER_H_

#include "base/run_loop.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin::test {
enum class PrimarySyncAccountWait { kWaitForAdded, kWaitForCleared, kNotWait };

// Observes various sign-in events and allows to wait for a specific state of
// signed-in accounts.
class SignInTestObserver : public IdentityManager::Observer,
                           public AccountReconcilor::Observer {
 public:
  explicit SignInTestObserver(IdentityManager* identity_manager,
                              AccountReconcilor* reconcilor);
  ~SignInTestObserver() override;

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(const PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo&) override;
  void OnRefreshTokenRemovedForAccount(const CoreAccountId&) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo&,
      const GoogleServiceAuthError&,
      signin_metrics::SourceForRefreshTokenOperation) override;
  void OnAccountsInCookieUpdated(const AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override;

  // AccountReconcilor::Observer:
  // TODO(crbug.com/40673982): Remove this observer method once the bug
  // is fixed.
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

  void WaitForAccountChanges(int signed_in_accounts,
                             PrimarySyncAccountWait primary_sync_account_wait);

 private:
  void QuitIfConditionIsSatisfied();

  int CountAccountsWithValidRefreshToken() const;
  int CountSignedInAccountsInCookie() const;
  bool HasValidPrimarySyncAccount() const;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<AccountReconcilor> reconcilor_;
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observation_{this};
  base::RunLoop run_loop_;

  bool are_expectations_set = false;
  int expected_signed_in_accounts_ = 0;
  PrimarySyncAccountWait primary_sync_account_wait_ =
      PrimarySyncAccountWait::kNotWait;
};

}  // namespace signin::test

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_SIGN_IN_TEST_OBSERVER_H_
