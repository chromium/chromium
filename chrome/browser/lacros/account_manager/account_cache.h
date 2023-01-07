// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_CACHE_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_CACHE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"

class PrefService;
class PrefRegistrySimple;

namespace account_manager {
struct Account;
}

// Data structure holding in memory a map of all Lacros accounts keyed by Gaia
// id. Also maintains a snapshot of one of the previous cache states. Gaia ids
// from this snapshot are persisted in local state preferences and restored on
// startup.
class AccountCache {
 public:
  using AccountByGaiaIdMap =
      base::flat_map<std::string, account_manager::Account>;
  using AccountIdSet = base::flat_set<std::string>;

  explicit AccountCache(PrefService* local_state);

  AccountCache(const AccountCache&) = delete;
  AccountCache& operator=(const AccountCache&) = delete;

  ~AccountCache();

  // Registers account cache related preferences in local state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Rewrites the `accounts_` map with Gaia accounts found in `system_accounts`.
  // Does not modify the snapshot.
  void UpdateAccounts(
      const std::vector<account_manager::Account>& system_accounts);

  // Saves `accounts_` in `snapshot_` and persists the accounts Gaia ids in
  // local state prefs.
  // Returns the list of Gaia ids saved previously in local state prefs.
  // Must be called only once, before the first `UpdateSnapshot()` call.
  AccountIdSet CreateSnapshot();

  // Similarly to `CreateSnapshot()`, saves `accounts_` in `snapshot_` and
  // persists the accounts Gaia ids in local state prefs.
  // Returns previously stored snapshot.
  // `CreateSnapshot()` must have been called before this method.
  AccountByGaiaIdMap UpdateSnapshot();

  // Returns an Account with `gaia_id` if found in the `accounts_` map or
  // nullptr if not found.
  const account_manager::Account* FindAccountByGaiaId(
      const std::string& gaia_id) const;

  // Returns a deep copy of the `accounts_` map.
  AccountByGaiaIdMap GetAccountsCopy() const;

 private:
  const raw_ptr<PrefService> local_state_;

  bool snapshot_created_ = false;

  // Map of account_manager::Account keyed by Gaia id as provided by the last
  // `UpdateAccounts()` call. Contains only Gaia accounts.
  AccountByGaiaIdMap accounts_;
  // The state of the `accounts_` map at the moment `CreateSnapshot()` or
  // `UpdateSnapshot()` was called for the last time.
  AccountByGaiaIdMap snapshot_;
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_CACHE_H_
