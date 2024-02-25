// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_STATUS_CODE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_STATUS_CODE_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"

namespace ash::printing::oauth2 {

enum class StatusCode {
  // Success - no errors occurred.
  kOK = 0,
  // The provided server URL is invalid.
  kInvalidURL,
  // The client is registered to the server but there is no active OAuth2
  // sessions. Run the method InitAuthorization(...) and then
  // FinishAuthorization(...) to start OAuth2 session,
  kAuthorizationNeeded,
  // The client is not registered to the server and the server does not support
  // dynamic registration (as described in rfc7591).
  kClientNotRegistered,
  // The server is untrusted.
  kUntrustedAuthorizationServer,
  // The server denied the request.
  kAccessDenied,
  // RedirectURL obtained during authorization process does not match any
  // existing session. It means that the obtained response was invalid or the
  // session was removed (see StatusCode::kTooManySessions) in the meantime.
  kNoMatchingSession,
  // Access token is invalid or expired (used internally only).
  kInvalidAccessToken,
  // The server sent 503 Service Unavailable HTTP status code.
  kServerTemporarilyUnavailable,
  // The server sent 500 Internal Server Error HTTP status code.
  kServerError,
  // The server sent invalid or unexpected response.
  kInvalidResponse,
  // Cannot open HTTPS connection to the server.
  kConnectionError,
  // The session was terminated because maximum number of sessions was reached.
  kTooManySessions,
  // Unexpected or unknown error occurred.
  kUnexpectedError
};

// Returns the given `status` as strings that can be used in device-log.
// Returned string equals C++ name of `status` without leading 'k', e.g.:
// kClientNotRegistered is converted to "ClientNotRegistered".
std::string_view ToStringPiece(StatusCode status);

// This is the standard callback used in oauth2 namespace. When `status` equals
// StatusCode::kOK, `data` may contain an access token or authorization URL.
// When `status` is different than StatusCode::kOK, `data` may contain
// an additional error message.
using StatusCallback =
    base::OnceCallback<void(StatusCode status, std::string data)>;

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_STATUS_CODE_H_
