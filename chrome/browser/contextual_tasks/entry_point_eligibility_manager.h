// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ENTRY_POINT_ELIGIBILITY_MANAGER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ENTRY_POINT_ELIGIBILITY_MANAGER_H_

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;

namespace signin {
class PrimaryAccountChangeEvent;
}

namespace contextual_tasks {
// Manager that determines whether the browser entry points for contextual tasks
// are eligible to be shown.
class EntryPointEligibilityManager : public signin::IdentityManager::Observer {
 public:
  DECLARE_USER_DATA(EntryPointEligibilityManager);

  explicit EntryPointEligibilityManager(
      BrowserWindowInterface* browser_window_interface);
  ~EntryPointEligibilityManager() override;

  static EntryPointEligibilityManager* From(
      BrowserWindowInterface* browser_window_interface);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsCookieDeletedByUserAction() override;

  // Returns true if the entry points are eligible to be shown. Returns false
  // otherwise.
  bool AreEntryPointsEligible();

  // Returns true if the entry points are eligible to be shown for the given
  // profile. Returns false otherwise.
  static bool IsEligible(Profile* profile);

  // Runs callback when the entry point eligibility changes
  using EntryPointEligibilityChangeCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  base::CallbackListSubscription RegisterOnEntryPointEligibilityChanged(
      EntryPointEligibilityChangeCallbackList::CallbackType callback);

 private:
  void OnAimPolicyChanged();
  void MaybeNotifyEntryPointEligibilityChanged();

  bool entry_points_are_eligible_ = false;
  raw_ptr<Profile> profile_ = nullptr;
  IntegerPrefMember aim_policy_;
  ui::ScopedUnownedUserData<EntryPointEligibilityManager>
      scoped_unowned_user_data_;
  base::CallbackListSubscription aim_eligibility_callback_subscription_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  EntryPointEligibilityChangeCallbackList
      entry_point_eligibility_change_callback_list_;
};
}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ENTRY_POINT_ELIGIBILITY_MANAGER_H_
