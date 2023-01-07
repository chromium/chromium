// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"

class Profile;

namespace lock_screen_apps {

class LockScreenProfileCreator;

// Interface for managing enabled apps in the lock screen profile. Apps must be
// Chrome apps with a note taking action handler or web apps with a lock screen
// URL declared, and may need to be on an allow-list or enterprise policy.
class AppManager {
 public:
  virtual ~AppManager() {}

  // Initializes the manager.
  // |primary_profile| - the profile which is the source of the lock screen
  //     action handler app (if one is set). This is the profile whose
  //     settings are used to determine whether and for which app lock screen
  //     action is enabled, and it should be associated with the primary user.
  // |lock_screen_profile_creator| - the object responsible for creating profile
  //     in which lock screen apps should be launched. It will detect when an
  //     app is enabled on lock screen and create the profile. The |AppManager|
  //     implementation can observe this class to detect when the profile is
  //     created and and update the availability of the lock screen app.
  virtual void Initialize(
      Profile* primary_profile,
      LockScreenProfileCreator* lock_screen_profile_creator) = 0;

  // Activates the manager - this should ensure that the lock screen app, if
  // available, is loaded and enabled in the lock screen profile.
  // |app_changed_callback| - used to notify the client when the lock screen app
  //      availability changes. It's cleared when the AppManager is stopped. It
  //      is not expected to be run after the app manager instance is destroyed.
  virtual void Start(const base::RepeatingClosure& app_changed_callback) = 0;

  // Stops the manager. After this is called, the app can be unloaded from the
  // lock screen enabled profile. Subsequent launch requests should not be
  // allowed.
  virtual void Stop() = 0;

  // If lock screen app is available, launches the app.
  // Returns whether the app launch was attempted.
  virtual bool LaunchLockScreenApp() = 0;

  // Returns whether a lock screen app is enabled and ready to launch.
  virtual bool IsLockScreenAppAvailable() const = 0;

  // Returns the ID of the current lock screen app, if one is enabled on lock
  // screen (for primary profile).
  virtual std::string GetLockScreenAppId() const = 0;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_MANAGER_H_
