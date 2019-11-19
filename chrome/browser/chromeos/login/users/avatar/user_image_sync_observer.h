// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_SYNC_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_SYNC_OBSERVER_H_

#include <memory>
#include <string>

#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"

class PrefChangeRegistrar;
class Profile;

namespace content {
class NotificationRegistrar;
}

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace user_manager {
class User;
}

namespace chromeos {

// This class is responsible for keeping local user image synced with
// image saved in syncable preference.
class UserImageSyncObserver
    : public sync_preferences::PrefServiceSyncableObserver,
      public content::NotificationObserver,
      public user_manager::UserManager::Observer {
 public:
  explicit UserImageSyncObserver(const user_manager::User* user);
  ~UserImageSyncObserver() override;

  // Register syncable preference for profile.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns |true| if sync was initialized and prefs have actual state.
  bool is_synced() const { return is_synced_; }

 private:
  // sync_preferences::PrefServiceSyncableObserver implementation.
  void OnIsSyncingChanged() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // user_manager::UserManager::Observer implementation.
  void OnUserImageChanged(const user_manager::User& user) override;

  // Called after user profile was loaded.
  void OnProfileGained(Profile* profile);

  // Called when sync servise started it's work and we are able to sync needed
  // preferences.
  void OnInitialSync();

  // Called when preference |pref_name| was changed.j
  void OnPreferenceChanged(const std::string& pref_name);

  // Saves local image preferences to sync.
  void UpdateSyncedImageFromLocal();

  // Saves sync preferences to local state and updates user image.
  void UpdateLocalImageFromSynced();

  // Gets synced image index. Returns false if user has no needed preferences.
  bool GetSyncedImageIndex(int* result);

  const user_manager::User* user_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<content::NotificationRegistrar> notification_registrar_;
  sync_preferences::PrefServiceSyncable* prefs_;
  bool is_synced_;
  // Indicates if local user image changed during initialization.
  bool local_image_changed_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_SYNC_OBSERVER_H_
