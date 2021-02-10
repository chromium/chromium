// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_USER_REMOVAL_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_USER_REMOVAL_MANAGER_H_

#include "base/callback_forward.h"

// This file contains a collection of functions that are used to support
// removing users from the device.
//
// InitiateUserRemoval starts the process and restarts the Chrome session back
// to the login screen, while RemoveUsersIfNeeded does the actual removal at the
// login screen.

namespace chromeos {
namespace user_removal_manager {

// This function is called early in the startup sequence on the login screen. It
// removes users from the device in case the magic pref is set to true, and
// otherwise it does nothing. Returns true if users were removed, false
// otherwise.
bool RemoveUsersIfNeeded();

// Performs a log out. Can be overridden for testing.
void LogOut();

// Overrides LogOut (AttemptUserExit) inside tests with a custom callback that
// gets run instead.
void OverrideLogOutForTesting(base::OnceClosure callback);

// Write the magic local_state pref and run the callback once the pref is
// persisted to disk (the callback is expected to restart the Chrome session
// back to the login screen). Also start a fail-safe timer that shuts down
// Chrome in case the callback doesn't do its job in time (eg. callback wants to
// do some network communication, but the connection is lost).
void InitiateUserRemoval(base::OnceClosure on_pref_persisted_callback);

}  // namespace user_removal_manager
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_USER_REMOVAL_MANAGER_H_
