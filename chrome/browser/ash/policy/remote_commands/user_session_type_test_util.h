// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains utilities to test all the possible user session types
// that can be running on the local device when a remote command arrives.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_SESSION_TYPE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_SESSION_TYPE_TEST_UTIL_H_

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace policy::test {

// All possible session types we need to test - including permutations of
// manually and auto launched kiosk sessions.
enum class TestSessionType {
  // Kiosk sessions
  kManuallyLaunchedWebKioskSession,
  kManuallyLaunchedKioskSession,
  kAutoLaunchedWebKioskSession,
  kAutoLaunchedKioskSession,

  // Guest sessions
  kManagedGuestSession,
  kGuestSession,

  // User sessions
  kAffiliatedUserSession,
  kUnaffiliatedUserSession,

  // No user sessions
  kNoSession,
};

const char* SessionTypeToString(TestSessionType session_type);

// Start a session of the given type, which involves first creating an user
// of the given type and then logging the user in (unless the session type
// doesn't require a logged in user).
void StartSessionOfType(TestSessionType session_type,
                        ash::FakeChromeUserManager& user_manager);

// Start a session of the given type, which involves first creating an user
// of the given type and then logging the user in (unless the session type
// doesn't require a logged in user) and a main TestingProfile creation.
TestingProfile* StartSessionOfTypeWithProfile(
    TestSessionType session_type,
    ash::FakeChromeUserManager& user_manager,
    TestingProfileManager& profile_manager);

}  // namespace policy::test

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_SESSION_TYPE_TEST_UTIL_H_
