// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_AUTH_RECORDER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_AUTH_RECORDER_H_

#include "components/session_manager/core/session_manager_observer.h"

namespace ash {

// A metrics recorder that records login authentication related metrics.
// This keeps track of the last authentication method we used and records
// switching between different authentication methods.
// This is tied to LoginScreenClientImpl lifetime.
class LoginAuthRecorder : public session_manager::SessionManagerObserver {
 public:
  // Authentication method to unlock the screen. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kMaxValue.
  enum class AuthMethod {
    kPassword = 0,
    kPin = 1,
    kSmartlock = 2,
    kFingerprint = 3,
    kChallengeResponse = 4,
    kNothing = 5,
    kMaxValue = kNothing,
  };

  // The type of switching between auth methods. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kMaxValue.
  enum class AuthMethodSwitchType {
    kPasswordToPin = 0,
    kPasswordToSmartlock = 1,
    kPinToPassword = 2,
    kPinToSmartlock = 3,
    kSmartlockToPassword = 4,
    kSmartlockToPin = 5,
    kPasswordToFingerprint = 6,
    kPinToFingerprint = 7,
    kSmartlockToFingerprint = 8,
    kFingerprintToPassword = 9,
    kFingerprintToPin = 10,
    kFingerprintToSmartlock = 11,
    kPasswordToChallengeResponse = 12,
    kNothingToPassword = 13,
    kNothingToPin = 14,
    kNothingToSmartlock = 15,
    kNothingToFingerprint = 16,
    kNothingToChallengeResponse = 17,
    kMaxValue = kNothingToChallengeResponse,
  };

  // The result of fingerprint auth attempt on the lock screen. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class FingerprintUnlockResult {
    kSuccess = 0,
    kFingerprintUnavailable = 1,
    kAuthTemporarilyDisabled = 2,
    kMatchFailed = 3,
    kMatchNotForPrimaryUser = 4,
    kMaxValue = kMatchNotForPrimaryUser,
  };

  LoginAuthRecorder();

  LoginAuthRecorder(const LoginAuthRecorder&) = delete;
  LoginAuthRecorder& operator=(const LoginAuthRecorder&) = delete;

  ~LoginAuthRecorder() override;

  // Called when user attempts authentication using AuthMethod `type`.
  void RecordAuthMethod(AuthMethod type);

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  AuthMethod last_auth_method_ = AuthMethod::kNothing;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_AUTH_RECORDER_H_
