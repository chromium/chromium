// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

#include "net/traffic_annotation/network_traffic_annotation.h"

// static
// TODO(b/273920907): Update the `traffic_annotation` setting once a mechanism
// allowing the user to disable the feature is implemented.
const net::NetworkTrafficAnnotationTag
    BoundSessionRefreshCookieFetcher::kTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("gaia_auth_rotate_bound_cookies",
                                            R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to rotate bound Google authentication "
            "cookies."
          trigger:
            "This request is triggered in a bound session when the bound Google"
            " authentication cookies are soon to expire."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "Request includes cookies and a signed token proving that a"
                " request comes from the same device as was registered before."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
                email: "chrome-signin-team@google.com"
            }
          }
          last_reviewed: "2024-05-30"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
             "This feature cannot be disabled in settings, but this request "
             "won't be made unless the user signs in to google.com."
          chrome_policy: {
            BoundSessionCredentialsEnabled {
              BoundSessionCredentialsEnabled: false
            }
          }
        })");

// static
bool BoundSessionRefreshCookieFetcher::IsPersistentError(Result result) {
  switch (result) {
    case Result::kSuccess:
    case Result::kConnectionError:
    case Result::kServerTransientError:
      return false;
    case Result::kServerPersistentError:
    case Result::kServerUnexepectedResponse:
    case Result::kChallengeRequiredUnexpectedFormat:
    case Result::kChallengeRequiredLimitExceeded:
    case Result::kSignChallengeFailed:
      return true;
  }
}

// static
bool BoundSessionRefreshCookieFetcher::IsTransientError(Result result) {
  return result == Result::kConnectionError ||
         result == Result::kServerTransientError;
}

std::ostream& operator<<(
    std::ostream& os,
    const BoundSessionRefreshCookieFetcher::Result& result) {
  switch (result) {
    case BoundSessionRefreshCookieFetcher::Result::kSuccess:
      return os << "Cookie rotation request finished with success.";
    case BoundSessionRefreshCookieFetcher::Result::kConnectionError:
      return os << "Cookie rotation request finished with Connection error.";
    case BoundSessionRefreshCookieFetcher::Result::kServerTransientError:
      return os << "Cookie rotation request finished with Server transient "
                   "error.";
    case BoundSessionRefreshCookieFetcher::Result::kServerPersistentError:
      return os << "Cookie rotation request finished with Server Persistent "
                   "error.";
    case BoundSessionRefreshCookieFetcher::Result::kServerUnexepectedResponse:
      return os << "Cookie rotation request didn't set the expected cookies.";
    case BoundSessionRefreshCookieFetcher::Result::
        kChallengeRequiredUnexpectedFormat:
      return os << "Challenge required unexpected format.";
    case BoundSessionRefreshCookieFetcher::Result::
        kChallengeRequiredLimitExceeded:
      return os << "Challenge required limit exceeded.";
    case BoundSessionRefreshCookieFetcher::Result::kSignChallengeFailed:
      return os << "Sign challenge failed on cookie rotation request.";
  }
}
