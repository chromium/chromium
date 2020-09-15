// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_ERROR_H_

#include <string>

#include "base/strings/string_piece_forward.h"

namespace extensions {

class IdentityGetAuthTokenError {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class State {
    kNone = 0,
    kInvalidClientId = 1,
    kEmptyScopes = 2,
    kOAuth2InvalidScopes = 3,
    kGaiaFlowAuthFailure = 4,
    kMintTokenAuthFailure = 5,
    kGetAccessTokenAuthFailure = 6,
    kOAuth2Failure = 7,
    kNoGrant = 8,
    kGaiaConsentInteractionRequired = 9,
    kGaiaConsentInteractionAlreadyRunning = 10,
    kOAuth2AccessDenied = 11,
    kGaiaFlowRejected = 12,
    kRemoteConsentFlowRejected = 13,
    kUserNotSignedIn = 14,
    kNotAllowlistedInPublicSession = 15,
    kSignInFailed = 16,
    kRemoteConsentUserNotSignedIn = 17,
    kUserNonPrimary = 18,
    kRemoteConsentUserNonPrimary = 19,
    kBrowserSigninNotAllowed = 20,
    kInvalidRedirect = 21,
    kOffTheRecord = 22,
    kPageLoadFailure = 23,
    kRemoteConsentPageLoadFailure = 24,
    kSetAccountsInCookieFailure = 25,
    kInvalidConsentResult = 26,
    kCanceled = 27,
    kMaxValue = kCanceled,
  };

  // Constructs a |State::kGaiaFlowAuthFailure| error with an |error_message|.
  static IdentityGetAuthTokenError FromGaiaFlowAuthError(
      base::StringPiece error_message);

  // Constructs a |State::kMintTokenAuthFailure| error with an
  // |error_message|.
  static IdentityGetAuthTokenError FromMintTokenAuthError(
      base::StringPiece error_message);

  // Constructs a |State::kGetAccessTokenAuthFailure| error with an
  // |error_message|.
  static IdentityGetAuthTokenError FromGetAccessTokenAuthError(
      base::StringPiece error_message);

  // Constructs an IdentityGetAuthTokenError from |oauth2_error|.
  static IdentityGetAuthTokenError FromOAuth2Error(
      base::StringPiece oauth2_error);

  // Constructs a |State::kNone| error.
  IdentityGetAuthTokenError();

  // Constructs an IdentityGetAuthTokenError from |state| with no additional
  // data.
  explicit IdentityGetAuthTokenError(State state);

  State state() const;

  // Returns an error message that can be returned to the developer in
  // chrome.runtime.lastError.
  std::string ToString() const;

 private:
  IdentityGetAuthTokenError(State state, base::StringPiece error);

  State state_;
  std::string error_message_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_ERROR_H_
