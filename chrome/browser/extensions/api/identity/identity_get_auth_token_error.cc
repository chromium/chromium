// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_auth_token_error.h"

#include "base/strings/string_piece.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"

namespace extensions {

// static
IdentityGetAuthTokenError IdentityGetAuthTokenError::FromGaiaFlowAuthError(
    base::StringPiece error_message) {
  return IdentityGetAuthTokenError(State::kGaiaFlowAuthFailure, error_message);
}

// static
IdentityGetAuthTokenError IdentityGetAuthTokenError::FromMintTokenAuthError(
    base::StringPiece error_message) {
  return IdentityGetAuthTokenError(State::kMintTokenAuthFailure, error_message);
}

// static
IdentityGetAuthTokenError
IdentityGetAuthTokenError::FromGetAccessTokenAuthError(
    base::StringPiece error_message) {
  return IdentityGetAuthTokenError(State::kGetAccessTokenAuthFailure,
                                   error_message);
}

// static
IdentityGetAuthTokenError IdentityGetAuthTokenError::FromOAuth2Error(
    base::StringPiece oauth2_error) {
  const char kOAuth2ErrorAccessDenied[] = "access_denied";
  const char kOAuth2ErrorInvalidScope[] = "invalid_scope";

  if (oauth2_error == kOAuth2ErrorAccessDenied) {
    return IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kOAuth2AccessDenied);
  } else if (oauth2_error == kOAuth2ErrorInvalidScope) {
    return IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kOAuth2InvalidScopes);
  } else {
    return IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kOAuth2Failure, oauth2_error);
  }
}

IdentityGetAuthTokenError::IdentityGetAuthTokenError()
    : IdentityGetAuthTokenError(State::kNone) {}

IdentityGetAuthTokenError::IdentityGetAuthTokenError(State state)
    : IdentityGetAuthTokenError(state, base::StringPiece()) {}

IdentityGetAuthTokenError::State IdentityGetAuthTokenError::state() const {
  return state_;
}

std::string IdentityGetAuthTokenError::ToString() const {
  switch (state_) {
    case State::kNone:
      return std::string();
    case State::kInvalidClientId:
      return identity_constants::kInvalidClientId;
    case State::kEmptyScopes:
    case State::kOAuth2InvalidScopes:
      return identity_constants::kInvalidScopes;
    case State::kGaiaFlowAuthFailure:
    case State::kMintTokenAuthFailure:
    case State::kGetAccessTokenAuthFailure:
    case State::kOAuth2Failure:
      return identity_constants::kAuthFailure + error_message_;
    case State::kNoGrant:
    case State::kGaiaConsentInteractionRequired:
    case State::kGaiaConsentInteractionAlreadyRunning:
      return identity_constants::kNoGrant;
    case State::kOAuth2AccessDenied:
    case State::kGaiaFlowRejected:
    case State::kRemoteConsentFlowRejected:
      return identity_constants::kUserRejected;
    case State::kUserNotSignedIn:
    case State::kNotAllowlistedInPublicSession:
    case State::kSignInFailed:
    case State::kRemoteConsentUserNotSignedIn:
      return identity_constants::kUserNotSignedIn;
    case State::kUserNonPrimary:
    case State::kRemoteConsentUserNonPrimary:
      return identity_constants::kUserNonPrimary;
    case State::kBrowserSigninNotAllowed:
      return identity_constants::kBrowserSigninNotAllowed;
    case State::kInvalidRedirect:
      return identity_constants::kInvalidRedirect;
    case State::kOffTheRecord:
      return identity_constants::kOffTheRecord;
    case State::kPageLoadFailure:
    case State::kRemoteConsentPageLoadFailure:
      return identity_constants::kPageLoadFailure;
    case State::kSetAccountsInCookieFailure:
      return identity_constants::kSetAccountsInCookieFailure;
    case State::kInvalidConsentResult:
      return identity_constants::kInvalidConsentResult;
    case State::kCanceled:
      return identity_constants::kCanceled;
  }
}

IdentityGetAuthTokenError::IdentityGetAuthTokenError(State state,
                                                     base::StringPiece error)
    : state_(state), error_message_(error) {}

}  // namespace extensions
