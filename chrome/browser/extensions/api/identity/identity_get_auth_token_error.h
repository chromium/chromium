// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_ERROR_H_

#include <string>
#include <string_view>

namespace extensions {

class IdentityGetAuthTokenError {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class State {
    kNone = 0,
    kInvalidClientId = 1,
    kEmptyScopes = 2,
    // kOAuth2InvalidScopes = 3,  // Deprecated
    // kGaiaFlowAuthFailure = 4,  // Deprecated
    kMintTokenAuthFailure = 5,
    kGetAccessTokenAuthFailure = 6,
    // kOAuth2Failure = 7,  // Deprecated
    kNoGrant = 8,
    kGaiaConsentInteractionRequired = 9,
    kGaiaConsentInteractionAlreadyRunning = 10,
    // kOAuth2AccessDenied = 11,  // Deprecated
    // kGaiaFlowRejected = 12,  // Deprecated
    kRemoteConsentFlowRejected = 13,
    kUserNotSignedIn = 14,
    kNotAllowlistedInPublicSession = 15,
    kSignInFailed = 16,
    kRemoteConsentUserNotSignedIn = 17,
    kUserNonPrimary = 18,
    kRemoteConsentUserNonPrimary = 19,
    kBrowserSigninNotAllowed = 20,
    // kInvalidRedirect = 21,  // Deprecated
    kOffTheRecord = 22,
    // kPageLoadFailure = 23,  // Deprecated
    kRemoteConsentPageLoadFailure = 24,
    // kSetAccountsInCookieFailure = 25, // Deprecated
    kInvalidConsentResult = 26,
    // kCanceled = 27, // Deprecated
    kInteractivityDenied = 28,
    kCannotCreateWindow = 29,
    kBrowserContextShutDown = 30,
    kMaxValue = kBrowserContextShutDown,
  };

  // Constructs a |State::kMintTokenAuthFailure| error with an
  // |error_message|.
  static IdentityGetAuthTokenError FromMintTokenAuthError(
      std::string_view error_message);

  // Constructs a |State::kGetAccessTokenAuthFailure| error with an
  // |error_message|.
  static IdentityGetAuthTokenError FromGetAccessTokenAuthError(
      std::string_view error_message);

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
  IdentityGetAuthTokenError(State state, std::string_view error);

  State state_;
  std::string error_message_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_ERROR_H_
