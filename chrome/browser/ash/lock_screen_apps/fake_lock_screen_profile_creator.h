// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_FAKE_LOCK_SCREEN_PROFILE_CREATOR_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_FAKE_LOCK_SCREEN_PROFILE_CREATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_profile_creator.h"

class TestingProfileManager;

namespace lock_screen_apps {

// Fake implementation of LockScreenProfileCreator that can be used in tests.
class FakeLockScreenProfileCreator : public LockScreenProfileCreator {
 public:
  // |profile_manager| - Testing profile manager that can be used to create
  //      testing profiles.
  explicit FakeLockScreenProfileCreator(TestingProfileManager* profile_manager);

  FakeLockScreenProfileCreator(const FakeLockScreenProfileCreator&) = delete;
  FakeLockScreenProfileCreator& operator=(const FakeLockScreenProfileCreator&) =
      delete;

  ~FakeLockScreenProfileCreator() override;

  // Simulate lock screen profile creation - this will create a TestingProfile
  // for lock screen apps, and initialize the profile's extension system.
  void CreateProfile();

  // Simulate lock screen profile creation failure - this will finish the
  // profile creation (notifying observers of the profile change), but the lock
  // screen profile provided by the class will remain null.
  void SetProfileCreationFailed();

 protected:
  void InitializeImpl() override;

 private:
  const raw_ptr<TestingProfileManager> profile_manager_;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_FAKE_LOCK_SCREEN_PROFILE_CREATOR_H_
