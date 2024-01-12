// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_SYNC_OBSERVER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_SYNC_OBSERVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/user_manager/user_manager.h"

class PrefChangeRegistrar;
class Profile;
class AccountId;

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace user_manager {
class User;
}

namespace ash {

// This class is responsible for keeping local user image synced with
// image saved in syncable preference.
class UserImageSyncObserver
    : public sync_preferences::PrefServiceSyncableObserver,
      public session_manager::SessionManagerObserver,
      public user_manager::UserManager::Observer {
 public:
  explicit UserImageSyncObserver(const user_manager::User* user);
  ~UserImageSyncObserver() override;

  // Register syncable preference for profile.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // sync_preferences::PrefServiceSyncableObserver implementation.
  void OnIsSyncingChanged() override;

  // session_manager::SessionManagerObserver implementation.
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // user_manager::UserManager::Observer implementation.
  void OnUserImageChanged(const user_manager::User& user) override;

  // Called after user profile was loaded.
  void OnProfileGained(Profile* profile);

  // Called when sync servise started it's work and we are able to sync needed
  // preferences.
  void OnInitialSync();

  // Called when preference `pref_name` was changed.j
  void OnPreferenceChanged(const std::string& pref_name);

  // Saves local image preferences to sync.
  void UpdateSyncedImageFromLocal();

  // Saves sync preferences to local state and updates user image.
  void UpdateLocalImageFromSynced();

  // Gets synced image index. Returns false if user has no needed preferences.
  bool GetSyncedImageIndex(int* result);

  raw_ptr<const user_manager::User> user_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  raw_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  bool is_synced_;
  // Indicates if local user image changed during initialization.
  bool local_image_changed_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_SYNC_OBSERVER_H_
