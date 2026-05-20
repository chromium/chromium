// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ELIGIBILITY_MANAGER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ELIGIBILITY_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class AimEligibilityService;
class PrefService;

namespace contextual_tasks {

// Helper class to manage Contextual Tasks eligibility.
class ContextualTasksEligibilityManager
    : public signin::IdentityManager::Observer {
 public:
  ContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service);
  ~ContextualTasksEligibilityManager() override;

  ContextualTasksEligibilityManager(const ContextualTasksEligibilityManager&) =
      delete;
  ContextualTasksEligibilityManager& operator=(
      const ContextualTasksEligibilityManager&) = delete;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsCookieDeletedByUserAction() override;

  // Returns true if the user is eligible for Contextual Tasks.
  bool IsEligible() const;

  // Returns true if the user is eligible for Contextual Tasks, excluding
  // identity and signed in checks.
  virtual bool IsEligibleWithoutIdentity() const;

  // Runs `callback` with the eligibility status when the eligibility changes.
  using EligibilityChangeCallbackList = base::RepeatingCallbackList<void(bool)>;
  base::CallbackListSubscription RegisterEligibilityChangedCallback(
      EligibilityChangeCallbackList::CallbackType callback);

 protected:
  virtual bool CalculateEligibility() const;
  void MaybeNotifyEligibilityChanged();
  raw_ptr<AimEligibilityService> aim_eligibility_service_ = nullptr;

 private:
  base::CallbackListSubscription aim_eligibility_callback_subscription_;

  raw_ptr<PrefService> pref_service_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  bool is_eligible_ = false;
  EligibilityChangeCallbackList callback_list_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ELIGIBILITY_MANAGER_H_
