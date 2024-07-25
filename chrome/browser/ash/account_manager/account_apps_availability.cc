// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_apps_availability.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"

// Structure of `account_manager::prefs::kAccountAppsAvailability`.
// `kAccountAppsAvailability` is a dictionary of dictionaries of the following
// format:
// {
//   "gaia_id_1": { "is_available_in_arc": <bool> },
//   "gaia_id_2": { "is_available_in_arc": <bool> },
// }
// Regular users will always have an entry for the primary account in the
// `kAccountAppsAvailability` pref (so it will never be empty). Active Directory
// users may have no Gaia accounts in-session and therefore may have an empty
// `kAccountAppsAvailability` pref.

namespace ash {

namespace {

constexpr int kMaxNumAccountsInArcMetric =
    10;  // To match AccountManager.NumAccounts metrics.

bool IsPrimaryGaiaAccount(const std::string& gaia_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  // GetPrimaryUser may return nullptr in tests.
  if (!user)
    return false;

  return user->GetAccountId().GetAccountType() == AccountType::GOOGLE &&
         user->GetAccountId().GetGaiaId() == gaia_id;
}

bool IsPrefInitialized(PrefService* prefs) {
  const base::Value::Dict& accounts =
      prefs->GetDict(account_manager::prefs::kAccountAppsAvailability);
  return accounts.size() > 0;
}

void CompleteFindAccountByGaiaId(
    const std::string& gaia_id,
    base::OnceCallback<void(const std::optional<account_manager::Account>&)>
        callback,
    const std::vector<account_manager::Account>& accounts) {
  for (const auto& account : accounts) {
    if (account.key.account_type() == account_manager::AccountType::kGaia &&
        account.key.id() == gaia_id) {
      std::move(callback).Run(account);
      return;
    }
  }
  LOG(ERROR) << "Couldn't find account by gaia id in AccountManager";
  std::move(callback).Run(std::nullopt);
}

void CompleteGetAccountsAvailableInArc(
    const base::flat_set<std::string>& gaia_ids_in_arc,
    base::OnceCallback<void(const base::flat_set<account_manager::Account>&)>
        callback,
    const std::vector<account_manager::Account>& all_accounts) {
  base::flat_set<account_manager::Account> result;
  for (const auto& account : all_accounts) {
    if (account.key.account_type() != account_manager::AccountType::kGaia)
      continue;

    if (gaia_ids_in_arc.contains(account.key.id()))
      result.insert(account);
  }

  DCHECK_EQ(result.size(), gaia_ids_in_arc.size());
  if (result.size() != gaia_ids_in_arc.size()) {
    LOG(ERROR) << "Expected " << gaia_ids_in_arc.size() << " accounts, but "
               << result.size() << " accounts were found in Account Manager.";
    // TODO(crbug.com/1277453): Repair prefs if this happens.
  }
  std::move(callback).Run(result);
}

base::flat_set<std::string> GetGaiaIdsAvailableInArc(PrefService* prefs) {
  base::flat_set<std::string> result;
  const base::Value::Dict& accounts =
      prefs->GetDict(account_manager::prefs::kAccountAppsAvailability);

  // See structure of `accounts` at the top of the file.
  for (const auto dict : accounts) {
    std::optional<bool> is_available = dict.second.GetDict().FindBool(
        account_manager::prefs::kIsAvailableInArcKey);
    if (!is_available.has_value() || !is_available.value())
      continue;

    result.insert(dict.first);
  }

  return result;
}

// Return `true` if account with `gaia_id` should be available in ARC.
// Return `false` if account with `gaia_id` should not be available in ARC.
// Return `nullopt` if account with `gaia_id` is not in prefs (it can happen if
// `SetIsAccountAvailableInArc` wasn't called for this account yet).
std::optional<bool> IsAccountAvailableInArc(PrefService* prefs,
                                            const std::string& gaia_id) {
  const base::Value::Dict& accounts =
      prefs->GetDict(account_manager::prefs::kAccountAppsAvailability);

  // See structure of `accounts` at the top of the file.
  const base::Value::Dict* account_entry = accounts.FindDict(gaia_id);
  if (!account_entry)
    return std::nullopt;

  std::optional<bool> is_available_in_arc =
      account_entry->FindBool(account_manager::prefs::kIsAvailableInArcKey);
  DCHECK(is_available_in_arc);
  // If there is no `is_available_in_arc` key, assume that account is available
  // in ARC.
  // TODO(crbug.com/1277453): Repair prefs if it happens.
  return is_available_in_arc.value_or(true);
}

void RemoveAccountFromPrefs(PrefService* prefs, const std::string& gaia_id) {
  DCHECK(!IsPrimaryGaiaAccount(gaia_id));

  ScopedDictPrefUpdate update(prefs,
                              account_manager::prefs::kAccountAppsAvailability);
  const bool success = update->Remove(gaia_id);
  if (!success)
    LOG(ERROR) << "Account apps availability pref not found";
}

void AddAccountToPrefs(PrefService* prefs,
                       const std::string& gaia_id,
                       bool is_available_in_arc) {
  // Account shouldn't already exist.
  DCHECK(!IsAccountAvailableInArc(prefs, gaia_id).has_value());

  base::Value::Dict account_entry;
  account_entry.Set(account_manager::prefs::kIsAvailableInArcKey,
                    base::Value(is_available_in_arc));

  ScopedDictPrefUpdate update(prefs,
                              account_manager::prefs::kAccountAppsAvailability);
  update->Set(gaia_id, std::move(account_entry));
}

void UpdateAccountInPrefs(PrefService* prefs,
                          const std::string& gaia_id,
                          bool is_available_in_arc) {
  ScopedDictPrefUpdate update(prefs,
                              account_manager::prefs::kAccountAppsAvailability);
  base::Value::Dict* account_entry = update->FindDict(gaia_id);
  DCHECK(account_entry);

  account_entry->Set(account_manager::prefs::kIsAvailableInArcKey,
                     is_available_in_arc);
}

}  // namespace

// static
const char AccountAppsAvailability::kNumAccountsInArcMetricName[] =
    "Arc.Auth.NumAccounts";

// static
const char AccountAppsAvailability::kPercentAccountsInArcMetricName[] =
    "Arc.Auth.PercentAccounts";

AccountAppsAvailability::AccountAppsAvailability(
    account_manager::AccountManagerFacade* account_manager_facade,
    signin::IdentityManager* identity_manager,
    PrefService* prefs)
    : account_manager_facade_(account_manager_facade),
      identity_manager_(identity_manager),
      prefs_(prefs) {
  DCHECK(account_manager_facade_);
  DCHECK(identity_manager_);
  DCHECK(prefs_);

  account_manager_facade_observation_.Observe(account_manager_facade_.get());
  identity_manager_observation_.Observe(identity_manager_.get());

  if (IsPrefInitialized(prefs_)) {
    is_initialized_ = true;
    // The metric is recorded once per session.
    account_manager_facade_->GetAccounts(base::BindOnce(
        &AccountAppsAvailability::ReportMetrics, weak_factory_.GetWeakPtr()));
    return;
  }

  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountAppsAvailability::InitAccountsAvailableInArcPref,
                     weak_factory_.GetWeakPtr()));
}

AccountAppsAvailability::~AccountAppsAvailability() = default;

// static
bool AccountAppsAvailability::IsArcAccountRestrictionsEnabled() {
  return crosapi::browser_util::IsLacrosEnabled();
}

bool AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled() {
  return base::FeatureList::IsEnabled(
      ash::features::kSecondaryAccountAllowedInArcPolicy);
}

// static
void AccountAppsAvailability::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      account_manager::prefs::kAccountAppsAvailability);
}

void AccountAppsAvailability::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void AccountAppsAvailability::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void AccountAppsAvailability::SetIsAccountAvailableInArc(
    const account_manager::Account& account,
    bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(account.key.account_type(), account_manager::AccountType::kGaia);

  if (!IsInitialized()) {
    // Using base::Unretained(this) is fine because `initialization_callbacks_`
    // is owned by this.
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountAppsAvailability::SetIsAccountAvailableInArc,
                       base::Unretained(this), account, is_available));
    return;
  }

  std::optional<bool> current_status =
      IsAccountAvailableInArc(prefs_, account.key.id());
  if (!current_status.has_value()) {
    // Account is not in prefs yet - add a new entry.
    AddAccountToPrefs(prefs_, account.key.id(), is_available);

    // Notify observers only if account should be available.
    if (is_available)
      NotifyObservers(account, is_available);

    return;
  }

  if (current_status.value() == is_available)
    return;

  UpdateAccountInPrefs(prefs_, account.key.id(), is_available);
  NotifyObservers(account, is_available);
}

void AccountAppsAvailability::GetAccountsAvailableInArc(
    base::OnceCallback<void(const base::flat_set<account_manager::Account>&)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsInitialized()) {
    // Using base::Unretained(this) is fine because `initialization_callbacks_`
    // is owned by this.
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountAppsAvailability::GetAccountsAvailableInArc,
                       base::Unretained(this), std::move(callback)));
    return;
  }

  account_manager_facade_->GetAccounts(
      base::BindOnce(&CompleteGetAccountsAvailableInArc,
                     GetGaiaIdsAvailableInArc(prefs_), std::move(callback)));
}

void AccountAppsAvailability::Shutdown() {
  identity_manager_observation_.Reset();
  account_manager_facade_observation_.Reset();
}

void AccountAppsAvailability::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsInitialized()) {
    // Using base::Unretained(this) is fine because `initialization_callbacks_`
    // is owned by this.
    initialization_callbacks_.push_back(base::BindOnce(
        &AccountAppsAvailability::OnRefreshTokenUpdatedForAccount,
        base::Unretained(this), account_info));
    return;
  }

  std::optional<bool> current_status =
      IsAccountAvailableInArc(prefs_, account_info.gaia);
  // - If `current_status.has_value()` is `false` - this account is not in prefs
  // yet. This happens when account is just added and
  // `SetIsAccountAvailableInArc()` wasn't called yet.
  // - If `current_status.value()` is `false` - this account is not available in
  // ARC. In this case we don't want to notify the observers.
  if (!current_status.has_value() || !current_status.value())
    return;

  FindAccountByGaiaId(
      account_info.gaia,
      base::BindOnce(&AccountAppsAvailability::MaybeNotifyObservers,
                     weak_factory_.GetWeakPtr(),
                     /*is_available_in_arc=*/true));
}

void AccountAppsAvailability::OnAccountUpserted(
    const account_manager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsInitialized())
    return;

  // Initialize the prefs list:
  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountAppsAvailability::InitAccountsAvailableInArcPref,
                     weak_factory_.GetWeakPtr()));
}

void AccountAppsAvailability::OnAccountRemoved(
    const account_manager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (account.key.account_type() != account_manager::AccountType::kGaia)
    return;

  if (!IsInitialized()) {
    // Using base::Unretained(this) is fine because `initialization_callbacks_`
    // is owned by this.
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountAppsAvailability::OnAccountRemoved,
                       base::Unretained(this), account));
    return;
  }

  std::optional<bool> current_status =
      IsAccountAvailableInArc(prefs_, account.key.id());
  RemoveAccountFromPrefs(prefs_, account.key.id());
  if (!current_status.has_value() || !current_status.value())
    return;

  NotifyObservers(account, /*is_available_in_arc=*/false);
}

void AccountAppsAvailability::OnAuthErrorChanged(
    const account_manager::AccountKey& account,
    const GoogleServiceAuthError& error) {
  // Nothing to do.
}

bool AccountAppsAvailability::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_initialized_;
}

void AccountAppsAvailability::InitAccountsAvailableInArcPref(
    const std::vector<account_manager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsInitialized())
    return;

  // If there are no accounts in Account Manager at the moment,
  // `OnAccountUpserted` will be called when the primary account is added.
  if (accounts.size() == 0)
    return;

  prefs_->Set(account_manager::prefs::kAccountAppsAvailability,
              base::Value(base::Value::Type::DICT));

  ScopedDictPrefUpdate update(prefs_,
                              account_manager::prefs::kAccountAppsAvailability);
  DCHECK(update->empty());

  // See structure of `update` dictionary at the top of the file.
  for (const auto& account : accounts) {
    if (account.key.account_type() != account_manager::AccountType::kGaia)
      continue;

    base::Value::Dict account_entry;
    account_entry.Set(account_manager::prefs::kIsAvailableInArcKey, true);

    // Key: `account.key.id()` = Gaia ID
    // Value: { "is_available_in_arc": true }
    update->Set(account.key.id(), std::move(account_entry));
  }

  // User type cannot be active directory, so we expect to have at least
  // primary account in the list.
  DCHECK(!update->empty());

  is_initialized_ = true;

  for (auto& callback : initialization_callbacks_)
    std::move(callback).Run();
  initialization_callbacks_.clear();
}

void AccountAppsAvailability::ReportMetrics(
    const std::vector<account_manager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int num_total_accounts = accounts.size();
  const int num_arc_accounts = GetGaiaIdsAvailableInArc(prefs_).size();
  base::UmaHistogramExactLinear(kNumAccountsInArcMetricName, num_arc_accounts,
                                kMaxNumAccountsInArcMetric + 1);

  DCHECK_GE(num_total_accounts, num_arc_accounts);
  const int percent_arc_accounts =
      (num_arc_accounts * 100.0) / num_total_accounts;
  base::UmaHistogramPercentage(kPercentAccountsInArcMetricName,
                               percent_arc_accounts);
}

void AccountAppsAvailability::FindAccountByGaiaId(
    const std::string& gaia_id,
    base::OnceCallback<void(const std::optional<account_manager::Account>&)>
        callback) {
  account_manager_facade_->GetAccounts(base::BindOnce(
      &CompleteFindAccountByGaiaId, gaia_id, std::move(callback)));
}

void AccountAppsAvailability::MaybeNotifyObservers(
    bool is_available_in_arc,
    const std::optional<account_manager::Account>& account) {
  if (!account)
    return;

  NotifyObservers(account.value(), is_available_in_arc);
}

void AccountAppsAvailability::NotifyObservers(
    const account_manager::Account& account,
    bool is_available_in_arc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_available_in_arc) {
    for (auto& observer : observer_list_) {
      observer.OnAccountAvailableInArc(account);
    }
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnAccountUnavailableInArc(account);
  }
}

}  // namespace ash
