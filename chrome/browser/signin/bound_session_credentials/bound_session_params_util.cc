// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"

#include <string>
#include <string_view>

#include "base/strings/escape.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "components/google/core/common/google_util.h"
#include "net/base/schemeful_site.h"
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
  // Site must be valid and must be in canonical form.
  if (!site.is_valid() || site.spec() != bound_session_params.site()) {
    return false;
  }

  // Restrict allowed sites fow now.
  if (!google_util::IsGoogleDomainUrl(site, google_util::ALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS) &&
      !google_util::IsYoutubeDomainUrl(site, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  // TODO(b/325441004): require that `refresh_url()` is not empty after
  // old clients data is migrated.
  if (bound_session_params.has_refresh_url()) {
    GURL refresh_url(bound_session_params.refresh_url());
    if (!refresh_url.is_valid() ||
        net::SchemefulSite(site) != net::SchemefulSite(refresh_url)) {
      return false;
    }
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

BoundSessionKey GetBoundSessionKey(
    const BoundSessionParams& bound_session_params) {
  DCHECK(AreParamsValid(bound_session_params));
  return {.site = GURL(bound_session_params.site()),
          .session_id = bound_session_params.session_id()};
}

bool AreSameSessionParams(const BoundSessionParams& lhs,
                          const BoundSessionParams& rhs) {
  return GetBoundSessionKey(lhs) == GetBoundSessionKey(rhs);
}

GURL ResolveEndpointPath(const GURL& request_url,
                         std::string_view endpoint_path) {
  std::string unescaped = base::UnescapeURLComponent(
      endpoint_path,
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  GURL result = request_url.Resolve(unescaped);
  if (net::SchemefulSite(result) == net::SchemefulSite(request_url)) {
    return result;
  }

  return GURL();
}

}  // namespace bound_session_credentials
