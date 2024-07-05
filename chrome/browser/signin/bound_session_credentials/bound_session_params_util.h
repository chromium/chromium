// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_UTIL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_UTIL_H_

#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_key.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"

class GURL;

namespace bound_session_credentials {

Timestamp TimeToTimestamp(base::Time time);

base::Time TimestampToTime(const Timestamp& timestamp);

bool AreParamsValid(const BoundSessionParams& bound_session_params);

BoundSessionKey GetBoundSessionKey(
    const BoundSessionParams& bound_session_params);

// Computes the session scope based on cookie domains.
// The current implementation only supports a single scope URL for the whole
// session. If such a URL cannot be built based on cookie domains, this function
// will return an empty URL.
GURL GetBoundSessionScope(const BoundSessionParams& bound_session_params);

bool AreSameSessionParams(const BoundSessionParams& lhs,
                          const BoundSessionParams& rhs);

// Converts an URL-encoded `endpoint_path` to a `GURL` relative to
// `request_url`. Supports both relative and absolute paths.
// Resulting endpoint must be on the same site than `request_url`.
// Returns an invalid `GURL` if the resulting endpoint cannot be used.
GURL ResolveEndpointPath(const GURL& request_url,
                         std::string_view endpoint_path);

}  // namespace bound_session_credentials

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_UTIL_H_
