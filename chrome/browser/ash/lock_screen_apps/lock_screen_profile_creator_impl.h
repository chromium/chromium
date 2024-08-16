// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_IMPL_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_profile_creator.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace lock_screen_apps {

// Real, production implementation of |LockScreenProfileCreator|.
// When initialized, it starts observing lock screen note taking availabiltiy.
// If/when a note taking app enaled on the lock screen is detected,
// |LockScreenProfileCreatorImpl| will start async lock screen profile creation.
class LockScreenProfileCreatorImpl : public LockScreenProfileCreator,
                                     public ash::NoteTakingHelper::Observer {
 public:
  // |primary_profile| - the primary profile - i.e. the profile which should be
  //     used to determine lock screen note taking availability.
  LockScreenProfileCreatorImpl(Profile* primary_profile,
                               const base::TickClock* tick_clock);

  LockScreenProfileCreatorImpl(const LockScreenProfileCreatorImpl&) = delete;
  LockScreenProfileCreatorImpl& operator=(const LockScreenProfileCreatorImpl&) =
      delete;

  ~LockScreenProfileCreatorImpl() override;

  // ash::NoteTakingHelper::Observer:
  void OnAvailableNoteTakingAppsUpdated() override;
  void OnPreferredNoteTakingAppUpdated(Profile* profile) override;

 protected:
  // lock_screen_apps::LockScreenProfileCreator:
  void InitializeImpl() override;

 private:
  // Called when the extension system for the primary profile is ready.
  // Testing note taking app availability before this is called might be
  // unreliable, as extension list in the profile's extension registry might not
  // be complete.
  void OnExtensionSystemReady();

  // Called when the lock screen profile is created and initialized (i.e. this
  // is called more than once for a single profile).
  // |start_time| - time at which the profile creation started.
  // |profile| - the created profile - i.e. the lock screen profile.
  void OnProfileReady(const base::TimeTicks& start_time, Profile* profile);

  const raw_ptr<Profile> primary_profile_;
  raw_ptr<const base::TickClock> tick_clock_;

  base::ScopedObservation<ash::NoteTakingHelper,
                          ash::NoteTakingHelper::Observer>
      note_taking_helper_observation_{this};

  base::WeakPtrFactory<LockScreenProfileCreatorImpl> weak_ptr_factory_{this};
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_IMPL_H_
