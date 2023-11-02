// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_REAUTH_RESULT_H_
#define CHROME_BROWSER_SIGNIN_REAUTH_RESULT_H_

namespace signin {

// Indicates the result of the Gaia Reauth flow.
// Needs to be kept in sync with "SigninReauthResult" in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReauthResult {
  // The user was successfully re-authenticated.
  kSuccess = 0,

  // The user account is not signed in.
  kAccountNotSignedIn = 1,

  // The user dismissed the reauth prompt.
  kDismissedByUser = 2,

  // The reauth page failed to load.
  kLoadFailed = 3,

  // A caller canceled the reauth flow.
  kCancelled = 4,

  // An unexpected response was received from Gaia.
  kUnexpectedResponse = 5,

  kMaxValue = kUnexpectedResponse,
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_REAUTH_RESULT_H_
