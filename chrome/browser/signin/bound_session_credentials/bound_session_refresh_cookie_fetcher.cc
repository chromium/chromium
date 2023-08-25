// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

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

std::ostream& operator<<(
    std::ostream& os,
    const BoundSessionRefreshCookieFetcher::Result& result) {
  switch (result) {
    case BoundSessionRefreshCookieFetcher::Result::kSuccess:
      return os << "Cookie rotation request finished with success.";
    case BoundSessionRefreshCookieFetcher::Result::kConnectionError:
      return os << "Cookie rotation request finished with Connection error.";
    case BoundSessionRefreshCookieFetcher::Result::kServerTransientError:
      return os
             << "Cookie rotation request finished with Server transient error.";
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
