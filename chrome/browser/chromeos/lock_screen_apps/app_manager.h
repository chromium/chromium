// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_H_

#include <string>

#include "base/callback_forward.h"

class Profile;

namespace lock_screen_apps {

class LockScreenProfileCreator;

// Interface for managing lock screen enabled action handler apps in the lock
// screen enabled profile. Initially, it will be used primarily to manage lock
// screen note taking apps.
class AppManager {
 public:
  virtual ~AppManager() {}

  // Initializes the manager.
  // |primary_profile| - the profile which is the source of the lock screen
  //     action handler app (if one is set). This is the profile whose
  //     settings are used to determine whether and for which app lock screen
  //     action is enabled, and it should be associated with the primary user.
  // |lock_screen_profile_creator| - the object responsible for creating profile
  //     in which lock screen apps should be launched. It will detect when a
  //     note taking app is enabled on lock screen and create the profile.
  //     |AppManager| implementation can observe this class to detect when the
  //     profile is created and thus note taking app becomes available on lock
  //     screen.
  virtual void Initialize(
      Profile* primary_profile,
      LockScreenProfileCreator* lock_screen_profile_creator) = 0;

  // Activates the manager - this should ensure that lock screen enabled note
  // taking app, if available, is loaded and enabled in the lock screen profile.
  // |note_taking_changed_callback| - used to notify the client when the note
  //      taking app availability changes. It's cleared when the AppManager is
  //      stopped. It is not expected to be run after the app manager instance
  //      is destroyed.
  virtual void Start(
      const base::RepeatingClosure& note_taking_changed_callback) = 0;

  // Stops the manager. After this is called, the app can be unloaded from the
  // lock screen enabled profile. Subsequent launch requests should not be
  // allowed.
  virtual void Stop() = 0;

  // If lock screen note taking app is available, launches the app with lock
  // screen note taking action.
  // Returns whether the app launch was attempted.
  virtual bool LaunchNoteTaking() = 0;

  // Returns whether a lock screen note taking is enabled and ready to launch.
  virtual bool IsNoteTakingAppAvailable() const = 0;

  // Returns the lock screen enabled lock screen note taking app, if a note
  // taking app is enabled on lock screen (for primary profile).
  virtual std::string GetNoteTakingAppId() const = 0;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_H_
