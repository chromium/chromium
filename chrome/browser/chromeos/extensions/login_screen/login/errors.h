// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_ERRORS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_ERRORS_H_

namespace extensions {

namespace login_api_errors {

extern const char kAlreadyActiveSession[];
extern const char kLoginScreenIsNotActive[];
extern const char kAnotherLoginAttemptInProgress[];
extern const char kNoManagedGuestSessionAccounts[];
extern const char kNoLockableSession[];
extern const char kSessionIsNotActive[];
extern const char kNoUnlockableSession[];
extern const char kSessionIsNotLocked[];
extern const char kAnotherUnlockAttemptInProgress[];
extern const char kAuthenticationFailed[];
extern const char kSharedMGSAlreadyLaunched[];
extern const char kNoSharedMGSFound[];
extern const char kSharedSessionIsNotActive[];
extern const char kSharedSessionAlreadyLaunched[];
extern const char kScryptFailure[];
extern const char kCleanupInProgress[];
extern const char kUnlockFailure[];
extern const char kDeviceRestrictedManagedGuestSessionNotEnabled[];

}  // namespace login_api_errors

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_ERRORS_H_
