// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/errors.h"

namespace extensions {

namespace login_api_errors {

const char kAlreadyActiveSession[] = "There is already an active session";
const char kLoginScreenIsNotActive[] = "Login screen is not active";
const char kAnotherLoginAttemptInProgress[] =
    "Another login attempt is in progress";
const char kNoManagedGuestSessionAccounts[] =
    "No managed guest session accounts";
const char kNoLockableSession[] = "There is no lockable session";
const char kSessionIsNotActive[] = "Session is not active";
const char kNoUnlockableSession[] = "There is no unlockable session";
const char kSessionIsNotLocked[] = "Session is not locked";
const char kAnotherUnlockAttemptInProgress[] =
    "Another unlock attempt is in progress";
const char kSharedMGSAlreadyLaunched[] =
    "Shared Managed Guest Session has already been launched";
const char kAuthenticationFailed[] = "Authentication failed";
const char kNoSharedMGSFound[] = "No shared Managed Guest Session found";
const char kSharedSessionIsNotActive[] = "Shared session is not active";
const char kSharedSessionAlreadyLaunched[] =
    "Another shared session has already been launched";
const char kScryptFailure[] = "Scrypt failed";
const char kCleanupInProgress[] = "Cleanup is already in progress";
const char kUnlockFailure[] = "Managed Guest Session unlock failed";
const char kDeviceRestrictedManagedGuestSessionNotEnabled[] =
    "DeviceRestrictedManagedGuestSessionEnabled policy is not enabled for "
    "shared kiosk mode";

}  // namespace login_api_errors

}  // namespace extensions
