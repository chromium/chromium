// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_SIGNIN_HELPER_LACROS_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_SIGNIN_HELPER_LACROS_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/core/browser/consistency_cookie_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class AccountProfileMapper;
struct CoreAccountId;

// Handles the Lacros signin flow. Maintains a `ScopedAccountUpdate`
// for the whole duration of the flow. The steps are:
// 1) Call `GetAccountsAvailableAsSecondary()` to check whether there are
//    available accounts in the OS that the user could pick.
// 2) If there are available accounts in the OS, show the account picker in
//    Chrome. If the user picks an existing account then skip to step 4.
// 3) Call `AccountProfileMapper::ShowAddAccountDialog()` to add a new account
//    to the OS.
// 4) Once the account is added to the profile, wait until the `Identitymanager`
//    picks it up.
class SigninHelperLacros : public signin::IdentityManager::Observer {
 public:
  SigninHelperLacros(
      const base::FilePath& profile_path,
      AccountProfileMapper* account_profile_mapper,
      signin::IdentityManager* identity_manager,
      signin::ConsistencyCookieManager* consistency_cookie_manager,
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      base::OnceCallback<void(const CoreAccountId&)> callback);

  ~SigninHelperLacros() override;

  SigninHelperLacros(const SigninHelperLacros&) = delete;
  SigninHelperLacros& operator=(const SigninHelperLacros&) = delete;

 private:
  // Callback for `GetAccountsAvailableAsSecondary()`.
  void OnAccountsAvailableAsSecondaryFetched(
      const std::vector<account_manager::Account>& accounts);

  // Callback for `AccountProfileMapper::ShowAddAccountDialog()`.
  void OnAccountAdded(
      const std::optional<AccountProfileMapper::AddAccountResult>& result);

  // Called once the user has picked an account (either from the picker or by
  // adding it to the OS directly).
  void OnAccountPicked(const std::string& gaia_id);

  // Unregisters the identity manager observation, releases
  // the `ScopedAccountUpdate`, and calls `callback_`.
  void Finalize(const CoreAccountId& account_id);

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // Sets the consistency cookie to "Updating".
  std::unique_ptr<signin::ConsistencyCookieManager::ScopedAccountUpdate>
      scoped_account_update_;

  base::OnceCallback<void(const CoreAccountId&)> callback_;
  base::FilePath profile_path_;
  const raw_ptr<AccountProfileMapper> account_profile_mapper_;
  account_manager::AccountManagerFacade::AccountAdditionSource source_;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observervation_{this};

  // Gaia ID returned by `AccountProfileMapper::ShowAddAccountDialog()`.
  std::string account_added_to_mapping_;

  base::WeakPtrFactory<SigninHelperLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_SIGNIN_HELPER_LACROS_H_
