// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_REAUTH_REASON_H_
#define ASH_PUBLIC_CPP_REAUTH_REASON_H_

namespace ash {

// Track all the ways a user may be sent through the re-auth flow.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence, existing enumerated constants should never be reordered, and all new
// constants should only be appended at the end of the  enumeration.
enum class ReauthReason {
  // Default value: no reauth reasons were detected so far, or the reason was
  // already reported.
  kNone = 0,

  // Legacy profile holders.
  kOther = 1,

  // Password changed, revoked credentials, account deleted.
  kInvalidTokenHandle = 2,

  // Incorrect password entered 3 times at the user pod.
  kIncorrectPasswordEntered = 3,

  // Incorrect password entered by a SAML user once.
  // OS would show a tooltip offering user to complete the online sign-in.
  kIncorrectSamlPasswordEntered = 4,

  // Device policy is set not to show user pods, which requires re-auth on every
  // login.
  kSamlReauthPolicy = 5,

  // Cryptohome is missing, most likely due to deletion during garbage
  // collection.
  kMissingCryptohome = 6,

  // During last login OS failed to connect to the sync with the existing RT.
  // This could be due to account deleted, password changed, account revoked,
  // etc.
  kSyncFailed = 7,

  // User cancelled the password change prompt when prompted by Chrome OS.
  kPasswordUpdateSkipped = 8,

  // SAML password sync token validation failed.
  kSamlPasswordSyncTokenValidationFailed = 9,

  // Corrupted cryptohome
  kUnrecoverableCryptohome = 10,

  // Gaia policy is set, which requires re-auth on every login if the offline
  // login time limit has been reached.
  kGaiaReauthPolicy = 11,

  // Gaia lock screen re-auth policy is set, which requires re-auth on lock
  // screen if the offline lock screen time limit has been reached.
  kGaiaLockScreenReauthPolicy = 12,

  // Saml lock screen re-auth policy is set, which requires re-auth on lock
  // screen if the offline lock screen time limit has been reached.
  kSamlLockScreenReauthPolicy = 13,

  // "Forgot Password" button clicked on the sign-in screen when password is
  // entered wrongly.
  kForgotPassword = 14,

  // Invalid, expired or empty reauth proof token during Cryptohome recovery.
  kCryptohomeRecovery = 15,

  // Must be the last value in this list.
  kNumReauthFlowReasons,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_REAUTH_REASON_H_
