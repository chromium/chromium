// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_manager_util.h"

#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_features.h"

namespace {

void GetAccountsNotDenylisted(
    const std::map<base::FilePath, std::vector<account_manager::Account>>&
        accounts_map,
    const base::flat_set<std::string>& denylisted_gaia_ids,
    AccountProfileMapper::ListAccountsCallback callback) {
  std::vector<account_manager::Account> result;
  // Copy all accounts into result, except for those denylisted.
  for (const auto& path_and_list_pair : accounts_map) {
    const std::vector<account_manager::Account>& list =
        path_and_list_pair.second;

    std::copy_if(
        list.begin(), list.end(), std::back_inserter(result),
        [&denylisted_gaia_ids](const account_manager::Account& account) {
          return !denylisted_gaia_ids.contains(account.key.id());
        });
  }
  std::move(callback).Run(result);
}

void GetAccountsAvailableAsPrimaryImpl(
    ProfileAttributesStorage* storage,
    AccountProfileMapper::ListAccountsCallback callback,
    const std::map<base::FilePath, std::vector<account_manager::Account>>&
        accounts_map) {
  // Collect all primary syncing accounts.
  std::vector<std::string> syncing_gaia_ids_temp;
  for (ProfileAttributesEntry* entry : storage->GetAllProfilesAttributes()) {
    // Skip if not syncing.
    if (!entry->IsAuthenticated())
      continue;
    DCHECK(!entry->GetGAIAId().empty());
    syncing_gaia_ids_temp.push_back(entry->GetGAIAId());
  }
  // Insert them all at once to avoid O(N^2) complexity.
  base::flat_set<std::string> syncing_gaia_ids(
      std::move(syncing_gaia_ids_temp));

  GetAccountsNotDenylisted(accounts_map, syncing_gaia_ids, std::move(callback));
}

void GetAccountsAvailableAsSecondaryImpl(
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

bool IsAccountManagerAvailable(const Profile* profile) {
  // Account Manager / Mirror is only enabled on Lacros's Main Profile for now.
  if (!profile->IsMainProfile())
    return base::FeatureList::IsEnabled(kMultiProfileAccountConsistency);

  // TODO(anastasiian): check for Web kiosk mode.

  // Account Manager is unavailable on Guest (Incognito) Sessions.
  if (profile->IsGuestSession() || profile->IsOffTheRecord())
    return false;

  // Account Manager is unavailable on Managed Guest Sessions / Public Sessions.
  if (profiles::IsPublicSession())
    return false;

  // Available in all other cases.
  return true;
}

void GetAccountsAvailableAsPrimary(
    AccountProfileMapper* mapper,
    ProfileAttributesStorage* storage,
    AccountProfileMapper::ListAccountsCallback callback) {
  DCHECK(mapper);
  DCHECK(storage);
  DCHECK(callback);
  mapper->GetAccountsMap(base::BindOnce(&GetAccountsAvailableAsPrimaryImpl,
                                        storage, std::move(callback)));
}

void GetAccountsAvailableAsSecondary(
    AccountProfileMapper* mapper,
    const base::FilePath& profile_path,
    AccountProfileMapper::ListAccountsCallback callback) {
  DCHECK(mapper);
  DCHECK(callback);
  mapper->GetAccountsMap(base::BindOnce(&GetAccountsAvailableAsSecondaryImpl,
                                        profile_path, std::move(callback)));
}
