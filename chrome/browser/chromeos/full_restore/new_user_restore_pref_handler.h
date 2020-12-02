// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_NEW_USER_RESTORE_PREF_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_NEW_USER_RESTORE_PREF_HANDLER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

class Profile;

namespace chromeos {
namespace full_restore {

// This class handles the new user's restore prefs. It sets the default restore
// pref setting as |kAskEveryTime|. If the user has the browser restore setting,
// updates the restore pref |kRestoreAppsAndPagesPrefName|, when the browser
// restore settings is synced.
class NewUserRestorePrefHanlder
    : public sync_preferences::PrefServiceSyncableObserver {
 public:
  explicit NewUserRestorePrefHanlder(Profile* profile);
  ~NewUserRestorePrefHanlder() override;

  NewUserRestorePrefHanlder(const NewUserRestorePrefHanlder&) = delete;
  NewUserRestorePrefHanlder& operator=(const NewUserRestorePrefHanlder&) =
      delete;

  // sync_preferences::PrefServiceSyncableObserver overrides:
  void OnIsSyncingChanged() override;

 private:
  // Callback method for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  Profile* profile_ = nullptr;

  bool is_restore_pref_changed_ = false;

  // Local restore pref to keep track the pref |kRestoreAppsAndPagesPrefName|
  // changes.
  std::unique_ptr<IntegerPrefMember> local_restore_pref_;

  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      syncable_pref_observer_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_NEW_USER_RESTORE_PREF_HANDLER_H_
