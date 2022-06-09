// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_HELPER_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_HELPER_H_

#include <set>
#include <string>

#include "base/no_destructor.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace ash {

// Describes an app's level of support for use on the lock screen.
// IMPORTANT: These constants are used in settings UI, so be careful about
//     reordering/adding/removing items.
enum class LockScreenAppSupport {
  // The app does not support use on lock screen.
  kNotSupported = 0,
  // The app supports use on the lock screen, but is not allowed to run on the
  // lock screen due to policy settings.
  kNotAllowedByPolicy = 1,
  // The app supports use on the lock screen, but is not enabled as a
  // lock screen app by the user. This state implies that the user
  // can be offered to enable this app for use on the lock screen.
  kSupported = 2,
  // The app is enabled by the user to run as on the lock
  // screen. Note that, while more than one app can be in enabled state at a
  // same time, currently only the preferred note taking app will be launchable
  // from the lock screen UI.
  kEnabled = 3,
};

// Singleton class used to track available lock screen apps.
class LockScreenHelper {
 public:
  static LockScreenHelper& GetInstance();

  LockScreenHelper(const LockScreenHelper&) = delete;
  LockScreenHelper& operator=(const LockScreenHelper&) = delete;

  // Sets up the helper. Must only be called once.
  void Initialize(Profile* profile);

  // Removes initialized state.
  void Shutdown();

  // Updates the cached list of apps allowed on the lock screen. Sets
  // `allowed_lock_screen_apps_state_`  and
  // `allowed_lock_screen_apps_by_policy_` to values appropriate for the current
  // `profile_with_enabled_lock_screen_apps_` state.
  void UpdateAllowedLockScreenAppsList();

  // Returns the state of the app's support for running on the lock screen.
  LockScreenAppSupport GetLockScreenSupportForApp(Profile* profile,
                                                  const std::string& app_id);

  // Attempts to set the given app as enabled on the lock screen. Returns
  // whether the app status changed.
  bool SetAppEnabledOnLockScreen(Profile* profile,
                                 const std::string& app_id,
                                 bool enabled);

 private:
  LockScreenHelper();
  ~LockScreenHelper();
  friend class base::NoDestructor<LockScreenHelper>;

  // The state of the allowed app ID cache (used for determining the state of
  // note-taking apps allowed on the lock screen).
  enum class AllowedAppListState {
    // The allowed apps have not yet been determined.
    kUndetermined,
    // No app ID restriction exists in the profile.
    kAllAppsAllowed,
    // A list of allowed app IDs exists in the profile.
    kAllowedAppsListed
  };

  // Called when kNoteTakingAppsLockScreenAllowlist pref changes for
  // |profile_with_enabled_lock_screen_apps_|.
  void OnAllowedLockScreenAppsChanged();

  // The profile for which lock screen apps are enabled,
  Profile* profile_with_enabled_lock_screen_apps_ = nullptr;

  // Tracks kNoteTakingAppsLockScreenAllowlist pref for the profile for which
  // lock screen apps are enabled.
  PrefChangeRegistrar pref_change_registrar_;

  // The current AllowedAppListState for lock screen note taking in
  // `profile_with_enabled_lock_screen_apps_`. If kAllowedAppsListed,
  // `lock_screen_apps_allowed_by_policy_` should contain the set of allowed
  // app IDs.
  AllowedAppListState allowed_lock_screen_apps_state_ =
      AllowedAppListState::kUndetermined;

  // If `allowed_lock_screen_apps_state_` is kAllowedAppsListed, contains all
  // app IDs that are allowed to handle new-note action on the lock screen. The
  // set should only be used for apps from
  // `profile_with_enabled_lock_screen_apps_` and when
  // `allowed_lock_screen_apps_state_` equals kAllowedAppsListed.
  std::set<std::string> allowed_lock_screen_apps_by_policy_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_HELPER_H_
