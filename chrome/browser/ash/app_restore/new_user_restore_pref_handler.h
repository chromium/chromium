// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_NEW_USER_RESTORE_PREF_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_NEW_USER_RESTORE_PREF_HANDLER_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

class Profile;

namespace ash::full_restore {

// This class handles the new user's OS restore pref "restore_apps_and_pages".
//
// 1. At startup, both the OS restore pref "restore_apps_and_pages" and the
// browser's "restore_on_startup" are empty, so the "restore_apps_and_pages"'s
// value is initialized as |kAskEveryTime|.
//
// 2. When synced, there are 3 scenarios:
// A. The OS restore pref "restore_apps_and_pages" is not empty in the init sync
// list, then don't update the "restore_apps_and_pages"'s value, and use the
// value from sync.
// B. The OS restore pref "restore_apps_and_pages" is empty in the init sync
// list, and "restore_apps_and_pages" is set by the user, then don't update
// the "restore_apps_and_pages"'s value, and use the value set by the user.
// C. The OS restore pref "restore_apps_and_pages" is empty in the init sync
// list, "restore_apps_and_pages" is not set by the user, and the browser's
// "restore_on_startup" is not empty; then reset the "restore_apps_and_pages"
// value based on the browser's setting "restore_on_startup".
class NewUserRestorePrefHandler
    : public sync_preferences::SyncedPrefObserver,
      public sync_preferences::PrefServiceSyncableObserver {
 public:
  explicit NewUserRestorePrefHandler(Profile* profile);
  ~NewUserRestorePrefHandler() override;

  NewUserRestorePrefHandler(const NewUserRestorePrefHandler&) = delete;
  NewUserRestorePrefHandler& operator=(const NewUserRestorePrefHandler&) =
      delete;

  // sync_preferences::SyncedPrefObserver overrides:
  void OnStartedSyncing(std::string_view path) override;

  // sync_preferences::PrefServiceSyncableObserver overrides:
  void OnIsSyncingChanged() override;

 private:
  // Callback method for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  raw_ptr<Profile> profile_ = nullptr;

  bool is_restore_pref_changed_ = false;
  bool is_restore_pref_synced_ = false;

  // Local restore pref to keep track the pref |kRestoreAppsAndPagesPrefName|
  // changes.
  std::unique_ptr<IntegerPrefMember> local_restore_pref_;

  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      syncable_pref_observer_{this};
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_NEW_USER_RESTORE_PREF_HANDLER_H_
