// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"

#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "net/cookies/cookie_util.h"
#include "url/gurl.h"

namespace bound_session_credentials {

Timestamp TimeToTimestamp(base::Time time) {
  Timestamp timestamp = Timestamp();
  timestamp.set_microseconds(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return timestamp;
}

base::Time TimestampToTime(const Timestamp& timestamp) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(timestamp.microseconds()));
}

bool AreParamsValid(const BoundSessionParams& bound_session_params) {
  // Note: The check for params validity checks for empty value as
  // `bound_session_params.has*()` doesn't check against explicitly set empty
  // value.
  bool is_valid = !bound_session_params.session_id().empty() &&
                  !bound_session_params.site().empty() &&
                  !bound_session_params.wrapped_key().empty() &&
                  !bound_session_params.credentials().empty();

  if (!is_valid) {
    return false;
  }

  GURL site(bound_session_params.site());
  if (!site.is_valid()) {
    return false;
  }

  return base::ranges::all_of(
      bound_session_params.credentials(), [&site](const auto& credential) {
        return IsCookieCredentialValid(credential, site);
      });
}

bool IsCookieCredentialValid(const Credential& credential, const GURL& site) {
  if (!credential.has_cookie_credential() ||
      credential.cookie_credential().name().empty()) {
    return false;
  }

  // Empty cookie domain is considered `host-only` cookie.
  // Note: `host-only` isn't supported yet in the current implementation to
  // narrow the scope of throttling.
  // `cookie_domain` is currently not used. We rely on the `site` param which
  // defines the scope of the session. `cookie_domain` might be set on a higher
  // level domain (e.g. site: `foo.bar.baz.com/`, cookie domain: `bar.baz.com`,
  // throttling will be limited to `foo.bar.baz.com/`).
  const std::string& cookie_domain = credential.cookie_credential().domain();
  if (!cookie_domain.empty() &&
      !site.DomainIs(net::cookie_util::CookieDomainAsHost(cookie_domain))) {
    return false;
  }
  return true;
}

bool AreSameSessionParams(const BoundSessionParams& lhs,
                          const BoundSessionParams& rhs) {
  return lhs.site() == rhs.site() && lhs.session_id() == rhs.session_id();
}

}  // namespace bound_session_credentials
