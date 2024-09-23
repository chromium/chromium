// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace ash {

// This class is tracking which accounts from `AccountManager` should be
// available in apps. Currently only availability in ARC++ is being tracked.
// ARC++ availability may be set just after account addition or when user
// changes it manually in OS Settings.
// There should be only one instance of this class, which is attached to the
// only regular Ash profile. The class should exist only if Account Manager
// exists (if `IsAccountManagerAvailable(profile)` is `true`).
class AccountAppsAvailability
    : public KeyedService,
      public account_manager::AccountManagerFacade::Observer,
      public signin::IdentityManager::Observer {
 public:
  static const char kNumAccountsInArcMetricName[];
  static const char kPercentAccountsInArcMetricName[];

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

  // The parameters are not owned pointers, and should outlive this class
  // instance.
  AccountAppsAvailability(
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager,
      PrefService* prefs);
  ~AccountAppsAvailability() override;

  AccountAppsAvailability(const AccountAppsAvailability&) = delete;
  AccountAppsAvailability& operator=(const AccountAppsAvailability&) = delete;

  // ARC account restrictions are enabled iff Lacros is enabled.
  static bool IsArcAccountRestrictionsEnabled();

  // Managed secondary accounts are restricted if
  // SecondaryAccountAllowedInArcPolicy is enabled.
  static bool IsArcManagedAccountRestrictionEnabled();

  static void RegisterPrefs(PrefRegistrySimple* registry);

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
  // If the class is not initialized yet (`IsInitialized()` is `false`), waits
  // for initialization to complete.
  void GetAccountsAvailableInArc(
      base::OnceCallback<void(const base::flat_set<account_manager::Account>&)>
          callback);

  // Returns `true` if the class is initialized.
  bool IsInitialized() const;

 private:
  // `KeyedService`:
  void Shutdown() override;

  // `IdentityManager::Observer`:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // `AccountManagerFacade::Observer`:
  void OnAccountUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;
  void OnAuthErrorChanged(const account_manager::AccountKey& account,
                          const GoogleServiceAuthError& error) override;

  // Initialize the prefs: add all Gaia accounts from Account Manager with
  // is_available_in_arc=true.
  void InitAccountsAvailableInArcPref(
      const std::vector<account_manager::Account>& accounts);

  // Report metrics (e.g. number of accounts in ARC).
  void ReportMetrics(const std::vector<account_manager::Account>& accounts);

  // Call `GetAccounts` and find the account by `gaia_id`. Call the `callback`
  // with the resulted account or with `nullopt` if requested account is not in
  // Account Manager.
  void FindAccountByGaiaId(
      const std::string& gaia_id,
      base::OnceCallback<void(const std::optional<account_manager::Account>&)>
          callback);

  // Call `NotifyObservers` if account is not `nullopt`.
  void MaybeNotifyObservers(
      bool is_available_in_arc,
      const std::optional<account_manager::Account>& account);

  // Call `OnAccountAvailableInArc` if `is_available_in_arc` is `true`.
  // Otherwise call `OnAccountUnavailableInArc`.
  void NotifyObservers(const account_manager::Account& account,
                       bool is_available_in_arc);

  bool is_initialized_ = false;

  // Callbacks waiting on class initialization.
  std::vector<base::OnceClosure> initialization_callbacks_;

  // Non-owning pointers:
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<PrefService> prefs_;

  // A list of observers registered via `AddObserver`.
  base::ObserverList<Observer> observer_list_;

  // An observer for `IdentityManager`. Automatically deregisters when
  // `this` is destructed.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // An observer for `AccountManagerFacade`. Automatically deregisters when
  // `this` is destructed.
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountAppsAvailability> weak_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_
