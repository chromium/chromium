// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/lock_screen_apps/lock_screen_profile_creator.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
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
class LockScreenProfileCreatorImpl
    : public LockScreenProfileCreator,
      public chromeos::NoteTakingHelper::Observer {
 public:
  // |primary_profile| - the primary profile - i.e. the profile which should be
  //     used to determine lock screen note taking availability.
  LockScreenProfileCreatorImpl(Profile* primary_profile,
                               const base::TickClock* tick_clock);
  ~LockScreenProfileCreatorImpl() override;

  // chromeos::NoteTakingHelper::Observer:
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
  // |status| - profile creation status.
  void OnProfileReady(const base::TimeTicks& start_time,
                      Profile* profile,
                      Profile::CreateStatus status);

  Profile* const primary_profile_;
  const base::TickClock* tick_clock_;

  ScopedObserver<chromeos::NoteTakingHelper,
                 chromeos::NoteTakingHelper::Observer>
      note_taking_helper_observer_;

  base::WeakPtrFactory<LockScreenProfileCreatorImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LockScreenProfileCreatorImpl);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_IMPL_H_
