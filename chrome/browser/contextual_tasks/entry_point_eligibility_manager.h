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

namespace contextual_tasks {
// Manager that determines whether the browser entry points for contextual tasks
// are eligible to be shown.
class EntryPointEligibilityManager {
 public:
  DECLARE_USER_DATA(EntryPointEligibilityManager);

  explicit EntryPointEligibilityManager(
      BrowserWindowInterface* browser_window_interface);
  ~EntryPointEligibilityManager();

  static EntryPointEligibilityManager* From(
      BrowserWindowInterface* browser_window_interface);

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
  void MaybeNotifyEntryPointEligibilityChanged(bool eligible);

  bool entry_points_are_eligible_ = false;
  raw_ptr<Profile> profile_ = nullptr;
  ui::ScopedUnownedUserData<EntryPointEligibilityManager>
      scoped_unowned_user_data_;
  base::CallbackListSubscription eligibility_subscription_;
  EntryPointEligibilityChangeCallbackList
      entry_point_eligibility_change_callback_list_;
};
}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ENTRY_POINT_ELIGIBILITY_MANAGER_H_
