// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_UTIL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_UTIL_H_

#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"

class GURL;

namespace bound_session_credentials {

Timestamp TimeToTimestamp(base::Time time);

base::Time TimestampToTime(const Timestamp& timestamp);

bool AreParamsValid(const BoundSessionParams& bound_session_params);

bool IsCookieCredentialValid(const Credential& credential, const GURL& site);

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
