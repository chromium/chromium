// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_apps_availability.h"

#include "ash/constants/ash_features.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"

namespace ash {

AccountAppsAvailability::AccountAppsAvailability() = default;
AccountAppsAvailability::~AccountAppsAvailability() = default;

// static
bool AccountAppsAvailability::IsArcAccountRestrictionsEnabled() {
  return base::FeatureList::IsEnabled(
             chromeos::features::kArcAccountRestrictions) &&
         base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
}

void AccountAppsAvailability::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountAppsAvailability::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountAppsAvailability::SetIsAccountAvailableInArc(
    const account_manager::Account& account,
    bool is_available) {
  NOTIMPLEMENTED();
}

void AccountAppsAvailability::GetAccountsAvailableInArc(
    base::OnceCallback<void(const base::flat_set<account_manager::Account>&)>
        callback) {
  NOTIMPLEMENTED();
}

void AccountAppsAvailability::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {}

void AccountAppsAvailability::OnAccountUpserted(
    const account_manager::Account& account) {}

void AccountAppsAvailability::OnAccountRemoved(
    const account_manager::Account& account) {}

}  // namespace ash
