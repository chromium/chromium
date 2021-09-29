// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_PROFILE_MAPPER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_PROFILE_MAPPER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace account_manager {
struct Account;
class AccountKey;
}  // namespace account_manager

namespace base {
class FilePath;
}

class ProfileAttributesStorage;
class ProfileAttributesEntry;

// `AccountProfileMapper` is the main class maintaining the mapping between
// accounts and profiles. There is only one global `AccountProfileMapper` (it is
// not attached to a particular `Profile` instance).
// Its two main tasks are:
// - Persisting the mapping to disk (in `ProfileAttributesStorage`), and resolve
//   conflicts at startup, as the system accounts may change while Lacros is not
//   running.
// - Implement an API similar to `AccountManagerFacade`, but split by profile.
//   This API is consumed by `ProfileAccountManager` (a per-profile object),
//   which then updates the corresponding `IdentityManager`.
//
// Design considerations:
// - The `AccountProfileMapper` calls `GetAccounts()` at construction. While
//   this call is in flight, consumers have to wait.
// - To avoid race conditions, the `AccountProfileMapper` does not forward the
//   notifications from the facade. Instead, `GetAccounts()` is called.
// - When `GetAccounts()` completes, the AccountProfileMapper compares the
//   system accounts with the storage accounts. Then, it updates the storage
//   accordingly, and notifies the observers.
class AccountProfileMapper
    : public account_manager::AccountManagerFacade::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // `profile_path` is empty if the account is not assigned to a profile.
    virtual void OnAccountUpserted(const base::FilePath& profile_path,
                                   const account_manager::Account& account) {}
    virtual void OnAccountRemoved(const base::FilePath& profile_path,
                                  const account_manager::Account& account) {}
  };

  AccountProfileMapper(account_manager::AccountManagerFacade* facade,
                       ProfileAttributesStorage* storage);
  ~AccountProfileMapper() override;

  AccountProfileMapper(const AccountProfileMapper& other) = delete;
  AccountProfileMapper& operator=(const AccountProfileMapper& other) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Interface similar to `AccountManagerFacade`, but split per profile. If
  // there is no profile with `profile_path`, the behavior is the same as a
  // profile without accounts.
  void GetAccounts(
      const base::FilePath& profile_path,
      base::OnceCallback<void(const std::vector<account_manager::Account>&)>
          callback);
  void GetPersistentErrorForAccount(
      const base::FilePath& profile_path,
      const account_manager::AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback);
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const base::FilePath& profile_path,
      const account_manager::AccountKey& account,
      const std::string& oauth_consumer_name,
      OAuth2AccessTokenConsumer* consumer);
  // `callback` is called with nullopt if the operation failed.
  void ShowAddAccountDialog(
      const base::FilePath& profile_path,
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      base::OnceCallback<void(const absl::optional<account_manager::Account>&)>
          callback);

  // account_manager::AccountManagerFacade::Observer:
  void OnAccountUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;

 private:
  // Computes the stale accounts (accounts that are in Lacros but no longer in
  // the system) and removes them from the `ProfileAttributesStorage`.
  std::vector<std::pair<base::FilePath, std::string>> RemoveStaleAccounts();

  // Computes the new accounts (accounts that are in the system but not in
  // Lacros) and adds them to `entry_for_new_accounts`. If the entry is nullptr,
  // the accounts are left unassigned.
  std::vector<const account_manager::Account*> AddNewGaiaAccounts(
      const std::vector<account_manager::Account>& system_accounts,
      ProfileAttributesEntry* entry_for_new_accounts);

  // Update the `ProfileAttributesStorage` to match the system accounts.
  void OnGetAccountsCompleted(const std::vector<account_manager::Account>&);

  // Assigns the newly added account to the specified profile.
  void OnShowAddAccountDialogCompleted(
      const base::FilePath& profile_path,
      base::OnceCallback<void(const absl::optional<account_manager::Account>&)>
          client_callback,
      const account_manager::AccountAdditionResult& result);

  // Returns whether the profile at `profile_path` contains `account`.
  bool ProfileContainsAccount(const base::FilePath& profile_path,
                              const account_manager::AccountKey& account) const;

  // If there is only a single profile in Lacros, new accounts are added there
  // by default and this returns the corresponding entry. Otherwise, new
  // accounts are not assigned and this returns nullptr.
  ProfileAttributesEntry* MaybeGetProfileForNewAccounts() const;

  // All requests are delayed until the first `GetAccounts()` call completes.
  bool initialized_ = false;
  std::vector<base::OnceClosure> initialization_callbacks_;

  // Number of `ShowAddAccountDialog()` calls is in progress. Accounts are not
  // auto-assigned to the main profile for this flow.
  int account_addition_in_progress_ = 0;

  account_manager::AccountManagerFacade* const account_manager_facade_;
  ProfileAttributesStorage* const profile_attributes_storage_;
  base::ObserverList<Observer> observers_;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation_{this};

  // Map of account_manager::Account keyed by GaiaID.
  base::flat_map<std::string, account_manager::Account> account_cache_;

  base::WeakPtrFactory<AccountProfileMapper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_PROFILE_MAPPER_H_
