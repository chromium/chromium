// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_

#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace ash {

// This class is tracking which accounts from `AccountManager` should be
// available in apps. Currently only availability in ARC++ is being tracked.
// ARC++ availability may be set just after account addition or when user
// changes it manually in OS Settings.
// There should be only one instance of this class, which is attached to the
// only regular Ash profile. The class should exist only if Account Manager
// exists (if `ash::IsAccountManagerAvailable(profile)` is `true`).
class AccountAppsAvailability
    : public KeyedService,
      public account_manager::AccountManagerFacade::Observer,
      public signin::IdentityManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Called when the account should become available in ARC
    // (`SetIsAccountAvailableInArc` is called with `true`). Also called when
    // the token of an account that is already available in ARC is updated. At
    // the time of this call the `account`'s token is already updated /
    // available in `IdentityManager`.
    virtual void OnAccountAvailableInArc(
        const account_manager::Account& account) {}

    // Called when the account becomes unavailable in ARC
    // (`SetIsAccountAvailableInArc` is called with `false`).
    // Also called when the account is removed from Account Manager.
    virtual void OnAccountUnavailableInArc(
        const account_manager::Account& account) {}
  };

  AccountAppsAvailability();
  ~AccountAppsAvailability() override;

  AccountAppsAvailability(const AccountAppsAvailability&) = delete;
  AccountAppsAvailability& operator=(const AccountAppsAvailability&) = delete;

  // Returns `true` if `kArcAccountRestrictions` and `kLacrosSupport` are
  // enabled.
  static bool IsArcAccountRestrictionsEnabled();

  // Registers an observer.
  void AddObserver(Observer* observer);
  // Unregisters an observer that was registered using AddObserver.
  void RemoveObserver(Observer* observer);

  // Set whether the specified account should be available in ARC. Only Gaia
  // accounts are supported.
  void SetIsAccountAvailableInArc(const account_manager::Account& account,
                                  bool is_available);

  // Calls the `callback` with the set of accounts that should be
  // available in ARC.
  void GetAccountsAvailableInArc(
      base::OnceCallback<void(const base::flat_set<account_manager::Account>&)>
          callback);

 private:
  // `IdentityManager::Observer`:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // `AccountManagerFacade::Observer`:
  void OnAccountUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;

  base::ObserverList<Observer> observer_list_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_
