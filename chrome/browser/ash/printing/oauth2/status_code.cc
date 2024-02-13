// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/status_code.h"

#include <string_view>

namespace ash::printing::oauth2 {

std::string_view ToStringPiece(StatusCode status) {
  switch (status) {
    case StatusCode::kOK:
      return "OK";
    case StatusCode::kInvalidURL:
      return "InvalidURL";
    case StatusCode::kAuthorizationNeeded:
      return "AuthorizationNeeded";
    case StatusCode::kClientNotRegistered:
      return "ClientNotRegistered";
    case StatusCode::kUntrustedAuthorizationServer:
      return "UntrustedAuthorizationServer";
    case StatusCode::kAccessDenied:
      return "AccessDenied";
    case StatusCode::kNoMatchingSession:
      return "NoMatchingSession";
    case StatusCode::kInvalidAccessToken:
      return "InvalidAccessToken";
    case StatusCode::kServerTemporarilyUnavailable:
      return "ServerTemporarilyUnavailable";
    case StatusCode::kServerError:
      return "ServerError";
    case StatusCode::kInvalidResponse:
      return "InvalidResponse";
    case StatusCode::kConnectionError:
      return "ConnectionError";
    case StatusCode::kTooManySessions:
      return "TooManySessions";
    case StatusCode::kUnexpectedError:
      return "UnexpectedError";
  }
}

}  // namespace ash::printing::oauth2
