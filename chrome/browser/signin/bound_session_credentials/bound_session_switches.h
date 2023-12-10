// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_SWITCHES_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_SWITCHES_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

// Command line flags to help manual testing the device bound session
// credentials feature.

namespace bound_session_credentials {

// Returns `kCookieRotationDelay` if set by commandline. See the description of
// `kCookieRotationDelay`.
std::optional<base::TimeDelta> GetCookieRotationDelayIfSetByCommandLine();

// Returns `kCookieRotationResult` if set by commandline. See the description of
// `kCookieRotationResult`.
std::optional<BoundSessionRefreshCookieFetcher::Result>
GetCookieRotationResultIfSetByCommandLine();

}  // namespace bound_session_credentials
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_SWITCHES_H_
