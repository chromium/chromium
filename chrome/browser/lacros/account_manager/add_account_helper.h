// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ADD_ACCOUNT_HELPER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ADD_ACCOUNT_HELPER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace account_manager {
struct Account;
class AccountUpsertionResult;
class AccountManagerFacade;
}  // namespace account_manager

namespace base {
class FilePath;
}

class ProfileAttributesStorage;

// Helper class encapsulating creation of accounts and profiles for
// `AccountProfileMapper`.
class AddAccountHelper {
 public:
  using IsAccountInCacheCallback =
      base::RepeatingCallback<bool(const account_manager::Account&)>;

  AddAccountHelper(IsAccountInCacheCallback is_account_in_cache_callback,
                   account_manager::AccountManagerFacade* facade,
                   ProfileAttributesStorage* storage);
  ~AddAccountHelper();

  AddAccountHelper(const AddAccountHelper&) = delete;
  AddAccountHelper& operator=(const AddAccountHelper&) = delete;

  // Starts the account addition flow. `callback` must not be null.
  // If `profile_path` is empty, created a new profile that is ephemeral and
  // omitted. The account in `source_or_account` is used if present, otherwise a
  // new account is added with the source.
  // Note: it is important that `AddAccountHelper` completes quickly, as account
  // updates are blocked in the meantime. In particular, this should not block
  // on any user input beside the OS account addition flow.
  void Start(const base::FilePath& profile_path,
             const absl::variant<
                 account_manager::AccountManagerFacade::AccountAdditionSource,
                 account_manager::Account>& source_or_account,
             AccountProfileMapper::AddAccountCallback callback);

  // Adds or updates an account programmatically without user interaction.
  // Should only be used in tests.
  void UpsertAccountForTesting(
      const base::FilePath& profile_path,
      const account_manager::Account& account,
      const std::string& token_value,
      AccountProfileMapper::AddAccountCallback callback);

  // Notifies an `AddAccountHelper` that the account cache has been updated.
  void OnAccountCacheUpdated();

 private:
  // Called once the user added an account, and before the account is added to
  // the profile.
  void OnShowAddAccountDialogCompleted(
      const base::FilePath& profile_path,
      const account_manager::AccountUpsertionResult& result);

  // Called as part of the account addition flow, if the profile does not
  // already exist.
  void OnNewProfileInitialized(Profile* new_profile);

  // Called after the user added and account and the profile exists.
  // `profile_path` must not be empty.
  void OnShowAddAccountDialogCompletedWithProfilePath(
      const base::FilePath& profile_path);

  // Attempts to finish the account addition. Proceeds only if all of the
  // following is true:
  // - an account has been chosen by the user
  // - an account has been added to cache
  // - a profile path has been determined
  void MaybeCompleteAddAccount();

  IsAccountInCacheCallback is_account_in_cache_callback_;
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
  const raw_ptr<ProfileAttributesStorage> profile_attributes_storage_;
  AccountProfileMapper::AddAccountCallback callback_;

  std::optional<account_manager::Account> account_;
  // std::nullopt means that the path hasn't been determined yet.
  // An empty path means that the account should be unassigned.
  std::optional<base::FilePath> profile_path_;

  bool is_account_in_cache_ = false;

  base::WeakPtrFactory<AddAccountHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ADD_ACCOUNT_HELPER_H_
