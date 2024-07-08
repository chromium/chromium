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
                  !bound_session_params.credentials().empty() &&
                  !bound_session_params.refresh_url().empty();

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

  GURL refresh_url(bound_session_params.refresh_url());
  if (!refresh_url.is_valid() ||
      net::SchemefulSite(site) != net::SchemefulSite(refresh_url)) {
    return false;
  }

  GURL scope = GetBoundSessionScope(bound_session_params);
  if (scope.is_empty()) {
    return false;
  }

  return base::ranges::all_of(
      bound_session_params.credentials(), [](const auto& credential) {
        return credential.has_cookie_credential() &&
               !credential.cookie_credential().name().empty() &&
               !credential.cookie_credential().domain().empty();
      });
}

BoundSessionKey GetBoundSessionKey(
    const BoundSessionParams& bound_session_params) {
  DCHECK(AreParamsValid(bound_session_params));
  return {.site = GURL(bound_session_params.site()),
          .session_id = bound_session_params.session_id()};
}

GURL GetBoundSessionScope(const BoundSessionParams& bound_session_params) {
  GURL site(bound_session_params.site());
  GURL scope;

  for (const auto& credential : bound_session_params.credentials()) {
    if (!credential.has_cookie_credential()) {
      continue;
    }

    std::string cookie_domain = net::cookie_util::CookieDomainAsHost(
        credential.cookie_credential().domain());
    if (cookie_domain.empty()) {
      // Domain must be non-empty in DBSC parameters even if the cookie itself
      // has an empty domain attribute.
      // Note: the current implementation doesn't support `host-only` for
      // narrowing the scope of throttling.
      return GURL();
    }
    GURL::Replacements replacements;
    replacements.SetHostStr(cookie_domain);
    replacements.SetPathStr(credential.cookie_credential().path());
    // Domain+path is not enough to build a URL. Inherit all other URL
    // components (like scheme and port) from `site`.
    GURL credential_scope = site.ReplaceComponents(replacements);
    if (!credential_scope.is_valid() ||
        !credential_scope.DomainIs(site.host_piece())) {
      return GURL();
    }

    if (scope.is_empty()) {
      scope = std::move(credential_scope);
    } else if (scope != credential_scope) {
      return GURL();
    }
  }

  // If none of the cookies specifies a domain, use the site scope.
  return scope.is_empty() ? site : scope;
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
