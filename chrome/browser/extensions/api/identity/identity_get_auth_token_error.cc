// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_auth_token_error.h"

#include <string_view>

#include "chrome/browser/extensions/api/identity/identity_constants.h"

namespace extensions {

// static
IdentityGetAuthTokenError IdentityGetAuthTokenError::FromMintTokenAuthError(
    std::string_view error_message) {
  return IdentityGetAuthTokenError(State::kMintTokenAuthFailure, error_message);
}

// static
IdentityGetAuthTokenError
IdentityGetAuthTokenError::FromGetAccessTokenAuthError(
    std::string_view error_message) {
  return IdentityGetAuthTokenError(State::kGetAccessTokenAuthFailure,
                                   error_message);
}

IdentityGetAuthTokenError::IdentityGetAuthTokenError()
    : IdentityGetAuthTokenError(State::kNone) {}

IdentityGetAuthTokenError::IdentityGetAuthTokenError(State state)
    : IdentityGetAuthTokenError(state, std::string_view()) {}

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
      return identity_constants::kInvalidScopes;
    case State::kMintTokenAuthFailure:
    case State::kGetAccessTokenAuthFailure:
      return identity_constants::kAuthFailure + error_message_;
    case State::kNoGrant:
    case State::kGaiaConsentInteractionRequired:
    case State::kGaiaConsentInteractionAlreadyRunning:
      return identity_constants::kNoGrant;
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
    case State::kOffTheRecord:
      return identity_constants::kOffTheRecord;
    case State::kRemoteConsentPageLoadFailure:
      return identity_constants::kPageLoadFailure;
    case State::kInvalidConsentResult:
      return identity_constants::kInvalidConsentResult;
    case State::kInteractivityDenied:
      return identity_constants::kGetAuthTokenInteractivityDeniedError;
    case State::kCannotCreateWindow:
      return identity_constants::kCannotCreateWindow;
    case State::kBrowserContextShutDown:
      return identity_constants::kBrowserContextShutDown;
  }
}

IdentityGetAuthTokenError::IdentityGetAuthTokenError(State state,
                                                     std::string_view error)
    : state_(state), error_message_(error) {}

}  // namespace extensions
