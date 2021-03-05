// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REAUTH_STATS_H_
#define CHROME_BROWSER_ASH_LOGIN_REAUTH_STATS_H_

#include <string>

class AccountId;

namespace chromeos {

// Track all the ways a user may be sent through the re-auth flow.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence, existing enumerated constants should never be reordered, and all new
// constants should only be appended at the end of the  enumeration.
enum ReauthReason {
  // Default value: no reauth reasons were detected so far, or the reason was
  // already reported.
  NONE = 0,

  // Legacy profile holders.
  OTHER = 1,

  // Password changed, revoked credentials, account deleted.
  INVALID_TOKEN_HANDLE = 2,

  // Incorrect password entered 3 times at the user pod.
  INCORRECT_PASSWORD_ENTERED = 3,

  // Incorrect password entered by a SAML user once.
  // OS would show a tooltip offering user to complete the online sign-in.
  INCORRECT_SAML_PASSWORD_ENTERED = 4,

  // Device policy is set not to show user pods, which requires re-auth on every
  // login.
  SAML_REAUTH_POLICY = 5,

  // Cryptohome is missing, most likely due to deletion during garbage
  // collection.
  MISSING_CRYPTOHOME = 6,

  // During last login OS failed to connect to the sync with the existing RT.
  // This could be due to account deleted, password changed, account revoked,
  // etc.
  SYNC_FAILED = 7,

  // User cancelled the password change prompt when prompted by Chrome OS.
  PASSWORD_UPDATE_SKIPPED = 8,

  // SAML password sync token validation failed.
  SAML_PASSWORD_SYNC_TOKEN_VALIDATION_FAILED = 9,

  // Corrupted cryptohome
  UNRECOVERABLE_CRYPTOHOME = 10,

  // Gaia policy is set, which requires re-auth on every login if the offline
  // login time limit has been reached.
  GAIA_REAUTH_POLICY = 11,

  // Must be the last value in this list.
  NUM_REAUTH_FLOW_REASONS,
};

void RecordReauthReason(const AccountId& account_id, ReauthReason reason);
void SendReauthReason(const AccountId& account_id, bool password_changed);

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_REAUTH_STATS_H_
