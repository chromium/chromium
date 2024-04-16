// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_PROFILE_MAPPER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_PROFILE_MAPPER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/lacros/account_manager/account_cache.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace account_manager {
struct Account;
class AccountKey;
}  // namespace account_manager

namespace base {
class FilePath;
}

class AddAccountHelper;
class ProfileAttributesEntry;
class PrefService;

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
    : public account_manager::AccountManagerFacade::Observer,
      public ProfileAttributesStorage::Observer {
 public:
  // Result type for `ShowAddAccountDialog()`.
  // If the account was added to the system, but could not be added to the
  // profile, `profile_path` is empty.
  struct AddAccountResult {
    base::FilePath profile_path;
    account_manager::Account account;
  };

  using AddAccountCallback =
      base::OnceCallback<void(const std::optional<AddAccountResult>&)>;
  using ListAccountsCallback =
      base::OnceCallback<void(const std::vector<account_manager::Account>&)>;
  using MapAccountsCallback = base::OnceCallback<void(
      const std::map<base::FilePath, std::vector<account_manager::Account>>&)>;

  class Observer : public base::CheckedObserver {
   public:
    // `profile_path` is empty if the account is not assigned to a profile.
    // Note: to avoid sending too many notifications, the notification with an
    // empty path may not always be sent. For example if an account is added to
    // the facade and to a profile at the same time, only the notification with
    // a non-empty path is sent.
    virtual void OnAccountUpserted(const base::FilePath& profile_path,
                                   const account_manager::Account& account) {}
    virtual void OnAccountRemoved(const base::FilePath& profile_path,
                                  const account_manager::Account& account) {}
    virtual void OnAuthErrorChanged(const base::FilePath& profile_path,
                                    const account_manager::AccountKey& account,
                                    const GoogleServiceAuthError& error) {}
  };

  AccountProfileMapper(account_manager::AccountManagerFacade* facade,
                       ProfileAttributesStorage* storage,
                       PrefService* local_state);
  ~AccountProfileMapper() override;

  AccountProfileMapper(const AccountProfileMapper& other) = delete;
  AccountProfileMapper& operator=(const AccountProfileMapper& other) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Interface similar to `AccountManagerFacade`, but split per profile. If
  // there is no profile with `profile_path`, the behavior is the same as a
  // profile without accounts.
  void GetAccounts(const base::FilePath& profile_path,
                   ListAccountsCallback callback);
  void GetPersistentErrorForAccount(
      const base::FilePath& profile_path,
      const account_manager::AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback);
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const base::FilePath& profile_path,
      const account_manager::AccountKey& account,
      OAuth2AccessTokenConsumer* consumer);
  void ReportAuthError(const base::FilePath& profile_path,
                       const account_manager::AccountKey& account,
                       const GoogleServiceAuthError& error);

  // Returns the whole map of accounts per profile. An empty path is used as the
  // key for unassigned accounts (this key is not set if there are no unassigned
  // accounts).
  void GetAccountsMap(MapAccountsCallback callback);

  // Profile creation methods and assignment of accounts to profiles:

  // Adds an account to the specified profile. Some notes:
  // - `callback` is called with nullopt if the operation failed;
  // - fails if the user adds a non-Gaia account;
  // - may return the empty path if the account was added but could not be
  //   assigned to the profile.
  void ShowAddAccountDialog(
      const base::FilePath& profile_path,
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      AddAccountCallback callback);
  // Similar to `ShowAddAccountDialog()` but uses an account that already exists
  // in the `AccountManagerFacade` instead of creating a new one.
  void AddAccount(const base::FilePath& profile_path,
                  const account_manager::AccountKey& account,
                  AddAccountCallback callback);
  // Similar to `ShowAddAccountDialog()` but creates a new profile first. The
  // new profile is omitted and ephemeral: it's the caller responsibility to
  // clean these flags if needed.
  void ShowAddAccountDialogAndCreateNewProfile(
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      AddAccountCallback callback);
  // Similar to `ShowAddAccountDialogAndCreateNewProfile()` but uses an account
  // that already exists in the `AccountManagerFacade` instead of creating a new
  // one.
  void CreateNewProfileWithAccount(const account_manager::AccountKey& account,
                                   AddAccountCallback callback);

  // Remove all accounts from profile with profile_path.
  void RemoveAllAccounts(const base::FilePath& profile_path);
  void RemoveAccount(const base::FilePath& profile_path,
                     const account_manager::AccountKey& account_key);

  // account_manager::AccountManagerFacade::Observer:
  void OnAccountUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;
  void OnAuthErrorChanged(const account_manager::AccountKey& account,
                          const GoogleServiceAuthError& error) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;

  // Adds or updates an account programmatically without user interaction
  // Should only be used in tests.
  void UpsertAccountForTesting(const base::FilePath& profile_path,
                               const account_manager::Account& account,
                               const std::string& token_value);

  // Removes an account from the specified profile. Should only be used in
  // tests.
  // Note: this currently removes the account from the OS. A better
  // implementation would remove it from the profile, but keep it in the OS.
  void RemoveAccountForTesting(const base::FilePath& profile_path,
                               const account_manager::AccountKey& account_key);

 private:
  // Shared code for methods related to profile creation and adding accounts to
  // profiles. Pass an empty path to request a new profile. If
  // `source_or_accountkey` is an account, this account is used, otherwise a new
  // account is added with the source.
  void AddAccountInternal(
      const base::FilePath& profile_path,
      const absl::variant<
          account_manager::AccountManagerFacade::AccountAdditionSource,
          account_manager::AccountKey>& source_or_accountkey,
      AddAccountCallback callback);

  // Callback for `AddAccountHelper`, end of the flow starting with
  // `AddAccountInternal()`.
  void OnAddAccountCompleted(AddAccountHelper* helper,
                             AddAccountCallback callback,
                             const std::optional<AddAccountResult>& result);

  // Computes the stale accounts (accounts that are in Lacros but no longer in
  // the system) and removes them from the `ProfileAttributesStorage`. Might
  // also remove profiles that no longer have an associated primary account.
  std::vector<std::pair<base::FilePath, std::string>> RemoveStaleAccounts();

  // Computes the new accounts (accounts that are in the system but not in
  // Lacros) and adds them to `entry_for_new_accounts`. If the entry is nullptr,
  // the accounts are left unassigned.
  std::vector<const account_manager::Account*> AddNewGaiaAccounts(
      const std::vector<account_manager::Account>& system_accounts,
      AccountCache::AccountIdSet lacros_account_ids,
      ProfileAttributesEntry* entry_for_new_accounts);

  // Update the `ProfileAttributesStorage` to match the system accounts.
  void OnGetAccountsCompleted(const std::vector<account_manager::Account>&);

  // Returns whether the profile at `profile_path` contains `account`.
  bool ProfileContainsAccount(const base::FilePath& profile_path,
                              const account_manager::AccountKey& account) const;

  // Returns whether the `account_cache_` contains `account`.
  bool IsAccountInCache(const account_manager::Account& account);

  // If there is only a single profile in Lacros, new accounts are added there
  // by default and this returns the corresponding entry. Otherwise, new
  // accounts are not assigned and this returns nullptr.
  ProfileAttributesEntry* MaybeGetProfileForNewAccounts() const;

  // Returns whether the profile corresponding to `entry` should be deleted
  // after a system accounts update.
  bool ShouldDeleteProfile(ProfileAttributesEntry* entry) const;

  // Ensure the profiles are in good shape at startup. This is useful in
  // particular to migrate old profiles that were created before this class and
  // don't have their Gaia IDs populated. This migrates both Dice profiles and
  // the Ash main profile.
  // TODO(crbug.com/40802153): Consider deleting this code once all Dice
  // profiles were converted.
  void MigrateOldProfiles();

  // Remove accounts from a profile.
  void RemoveAccountsInternal(const base::FilePath& profile_path,
                              const base::flat_set<std::string>& gaia_ids);

  // All requests are delayed until the first `GetAccounts()` call completes.
  bool initialized_ = false;
  std::vector<base::OnceClosure> initialization_callbacks_;

  std::vector<std::unique_ptr<AddAccountHelper>> add_account_helpers_;

  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
  const raw_ptr<ProfileAttributesStorage> profile_attributes_storage_;
  base::ObserverList<Observer> observers_;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation_{this};
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_attributes_storage_observation_{this};

  AccountCache account_cache_;

  base::WeakPtrFactory<AccountProfileMapper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_PROFILE_MAPPER_H_
