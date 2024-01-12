// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_H_

#include <list>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace lock_screen_apps {

// Abstract class to be used to create the lock screen apps profile - the
// profile used for launching/running lock screen enabled apps.
class LockScreenProfileCreator {
 public:
  LockScreenProfileCreator();

  LockScreenProfileCreator(const LockScreenProfileCreator&) = delete;
  LockScreenProfileCreator& operator=(const LockScreenProfileCreator&) = delete;

  virtual ~LockScreenProfileCreator();

  // Initializes the creator - it marks the object as initialized and calls
  // |InitializeImpl|, a function that should be override to provide actual
  // initialization logic. After this, the |LockScreenProfileCreator|
  // implementation should be allowed to create lock screen profile.
  void Initialize();

  // Adds a closure that should be called when the lock screen profile provided
  // by the class is created. If the profile is alredy created at the time this
  // is called, |callback| will be run immediately.
  void AddCreateProfileCallback(base::OnceClosure callback);

  // Whether the |LockScreenProfileCreator| has been initialized.
  bool Initialized() const;

  // Whether the |LockScreenProfileCreator| finished profile creation, and the
  // created, if any, profile can be retrieved using |lock_screen_profile()|.
  // Note that |lock_screen_profile| might be null even if |ProfileCreated|
  // returns true - in case the profile creation failed.
  bool ProfileCreated() const;

  Profile* lock_screen_profile() const { return lock_screen_profile_; }

 protected:
  // Should be overriden to provide initialization logic - the
  // |LockScreenProfileCreator| instance should be put in state where it can
  // determine whether the lock screen profile should be created and start
  // profile creation when appropriate.
  // For example, the class instance might start observing lock screen note
  // taking availability, and start profile creation when a lock screen note
  // taking app is available.
  virtual void InitializeImpl() = 0;

  // Should be called by the implementation to indicate profile creation has
  // started.
  void OnLockScreenProfileCreateStarted();

  // Should be called by the implementation to finish profile creation - to
  // set |lock_screen_profile_| and run profile creation callbacks.
  void OnLockScreenProfileCreated(Profile* lock_screen_profile);

 private:
  enum class State {
    kNotInitialized,
    kInitialized,
    kCreatingProfile,
    kProfileCreated
  };

  // The current profile  creator state.
  State state_ = State::kNotInitialized;

  // The lock screen profile created by this, set when the profile creation
  // finishes.
  raw_ptr<Profile> lock_screen_profile_ = nullptr;

  std::list<base::OnceClosure> create_profile_callbacks_;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_PROFILE_CREATOR_H_
