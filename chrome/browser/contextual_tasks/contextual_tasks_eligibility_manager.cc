// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"

#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace contextual_tasks {

namespace {

bool IsSignedInToBrowserWithValidCredentials(
    signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return false;
  }

  if (!identity_manager->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    return false;
  }

  const CoreAccountId primary_account =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  return !identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
      primary_account);
}

}  // namespace

ContextualTasksEligibilityManager::ContextualTasksEligibilityManager(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service)
    : aim_eligibility_service_(aim_eligibility_service),
      pref_service_(pref_service),
      identity_manager_(identity_manager) {
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }

  if (aim_eligibility_service_) {
    aim_eligibility_callback_subscription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(&ContextualTasksEligibilityManager::
                                    MaybeNotifyEligibilityChanged,
                                base::Unretained(this)));
  }

  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        contextual_search::kSearchContentSharingSettings,
        base::BindRepeating(
            &ContextualTasksEligibilityManager::MaybeNotifyEligibilityChanged,
            base::Unretained(this)));
  }

  is_eligible_ = CalculateEligibility();
}

ContextualTasksEligibilityManager::~ContextualTasksEligibilityManager() =
    default;

void ContextualTasksEligibilityManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  MaybeNotifyEligibilityChanged();
}

void ContextualTasksEligibilityManager::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  MaybeNotifyEligibilityChanged();
}

void ContextualTasksEligibilityManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  MaybeNotifyEligibilityChanged();
}

void ContextualTasksEligibilityManager::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  MaybeNotifyEligibilityChanged();
}

void ContextualTasksEligibilityManager::
    OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error,
        signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  MaybeNotifyEligibilityChanged();
}

void ContextualTasksEligibilityManager::OnAccountsCookieDeletedByUserAction() {
  MaybeNotifyEligibilityChanged();
}

bool ContextualTasksEligibilityManager::IsEligible() const {
  return is_eligible_;
}

bool ContextualTasksEligibilityManager::IsEligibleWithoutIdentity() const {
  if (!base::FeatureList::IsEnabled(kContextualTasks)) {
    return false;
  }

  if (!aim_eligibility_service_ || !aim_eligibility_service_->IsAimEligible()) {
    return false;
  }

  if (!aim_eligibility_service_->IsCobrowseEligible()) {
    return false;
  }

  if (pref_service_ &&
      !contextual_search::ContextualSearchService::IsContextSharingEnabled(
          pref_service_)) {
    return false;
  }

  return true;
}

base::CallbackListSubscription
ContextualTasksEligibilityManager::RegisterEligibilityChangedCallback(
    EligibilityChangeCallbackList::CallbackType callback) {
  return callback_list_.Add(std::move(callback));
}

void ContextualTasksEligibilityManager::MaybeNotifyEligibilityChanged() {
  bool new_eligibility = CalculateEligibility();
  if (new_eligibility != is_eligible_) {
    is_eligible_ = new_eligibility;
    callback_list_.Notify(is_eligible_);
  }
}

bool ContextualTasksEligibilityManager::CalculateEligibility() const {
  if (!IsEligibleWithoutIdentity()) {
    return false;
  }

  if (!IsSignedInToBrowserWithValidCredentials(identity_manager_)) {
    return false;
  }

  if (!contextual_tasks::CookieJarContainsPrimaryAccount(identity_manager_)) {
    return false;
  }

  return true;
}

}  // namespace contextual_tasks
