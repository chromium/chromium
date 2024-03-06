// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_manager_util.h"

#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

void GetAccountsNotDenylisted(
    const std::map<base::FilePath, std::vector<account_manager::Account>>&
        accounts_map,
    const base::flat_set<std::string>& denylisted_gaia_ids,
    AccountProfileMapper::ListAccountsCallback callback) {
  std::vector<account_manager::Account> result;
  // Copy all accounts into result, except for those denylisted.
  for (const auto& path_and_list_pair : accounts_map) {
    base::ranges::copy_if(
        path_and_list_pair.second, std::back_inserter(result),
        [&denylisted_gaia_ids](const account_manager::Account& account) {
          return !denylisted_gaia_ids.contains(account.key.id());
        });
  }
  std::move(callback).Run(result);
}

void GetAllAvailableAccountsImpl(
    const base::FilePath& profile_path,
    AccountProfileMapper::ListAccountsCallback callback,
    const std::map<base::FilePath, std::vector<account_manager::Account>>&
        accounts_map) {
  auto it = accounts_map.find(profile_path);
  if (profile_path.empty() || it == accounts_map.end()) {
    // When empty or invalid profile path is given, return all accounts.
    GetAccountsNotDenylisted(accounts_map, {}, std::move(callback));
    return;
  }

  // Put the gaia ids of all the accounts in the given profile into a flat set.
  const std::vector<account_manager::Account>& accounts = it->second;
  auto profile_gaia_ids = base::MakeFlatSet<std::string>(
      accounts, {},
      [](const account_manager::Account& account) { return account.key.id(); });

  GetAccountsNotDenylisted(accounts_map, profile_gaia_ids, std::move(callback));
}

}  // namespace

void GetAllAvailableAccounts(
    AccountProfileMapper* mapper,
    const base::FilePath& profile_path,
    AccountProfileMapper::ListAccountsCallback callback) {
  DCHECK(mapper);
  DCHECK(callback);
  DCHECK_NE(profile_path, ProfileManager::GetGuestProfilePath());
  DCHECK_NE(profile_path, ProfileManager::GetSystemProfilePath());
  mapper->GetAccountsMap(base::BindOnce(&GetAllAvailableAccountsImpl,
                                        profile_path, std::move(callback)));
}
