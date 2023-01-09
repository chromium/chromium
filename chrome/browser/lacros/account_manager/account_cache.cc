// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_cache.h"

#include "base/values.h"
#include "components/account_manager_core/account.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

// List of the Gaia Ids of all accounts known to Lacros. Also includes accounts
// that don't belong to any Chrome profile.
const char kLacrosAccountIdsPref[] =
    "profile.account_manager_lacros_account_ids";

// Reads from `prefs` a list of account ids known by Lacros.
AccountCache::AccountIdSet GetLacrosAccountIdsPref(PrefService* prefs) {
  const base::Value& list = prefs->GetValue(kLacrosAccountIdsPref);
  if (!list.is_list())
    return {};
  AccountCache::AccountIdSet account_ids;
  for (const base::Value& value : list.GetList()) {
    const std::string* gaia_id = value.GetIfString();
    if (gaia_id)
      account_ids.insert(*gaia_id);
  }
  return account_ids;
}

// Saves to `prefs` a list of account ids from `accounts`.
void SetLacrosAccountIdsPref(PrefService* prefs,
                             const AccountCache::AccountByGaiaIdMap& accounts) {
  base::Value::List list;
  for (const auto& gaia_id_account_pair : accounts)
    list.Append(gaia_id_account_pair.first);
  prefs->SetList(kLacrosAccountIdsPref, std::move(list));
}

}  // namespace

AccountCache::AccountCache(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}
AccountCache::~AccountCache() = default;

// static
void AccountCache::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kLacrosAccountIdsPref);
}

void AccountCache::UpdateAccounts(
    const std::vector<account_manager::Account>& system_accounts) {
  accounts_.clear();
  for (const account_manager::Account& account : system_accounts) {
    const account_manager::AccountKey& key = account.key;
    // Filter out non-Gaia accounts.
    if (key.account_type() != account_manager::AccountType::kGaia)
      continue;
    accounts_.emplace(key.id(), account);
  }
}

AccountCache::AccountIdSet AccountCache::CreateSnapshot() {
  DCHECK(!snapshot_created_);
  DCHECK(snapshot_.empty());
  snapshot_ = accounts_;
  snapshot_created_ = true;
  AccountCache::AccountIdSet previous_accounts =
      GetLacrosAccountIdsPref(local_state_);
  SetLacrosAccountIdsPref(local_state_, accounts_);
  return previous_accounts;
}

AccountCache::AccountByGaiaIdMap AccountCache::UpdateSnapshot() {
  DCHECK(snapshot_created_);
  AccountByGaiaIdMap previous_snapshot;
  previous_snapshot.swap(snapshot_);
  snapshot_ = accounts_;
  SetLacrosAccountIdsPref(local_state_, accounts_);
  return previous_snapshot;
}

const account_manager::Account* AccountCache::FindAccountByGaiaId(
    const std::string& gaia_id) const {
  auto it = accounts_.find(gaia_id);
  return (it == accounts_.end()) ? nullptr : &it->second;
}

AccountCache::AccountByGaiaIdMap AccountCache::GetAccountsCopy() const {
  return accounts_;
}
