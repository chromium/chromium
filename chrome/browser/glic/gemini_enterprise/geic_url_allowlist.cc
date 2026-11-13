// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace geic {

namespace {

// A pre-production or internal corp host pattern: `<prefix><label><suffix>`,
// where `<label>` is a single non-empty DNS label (e.g. `staging` or
// `widget-autopush`).
struct HostPattern {
  std::string_view prefix;
  std::string_view suffix;
};

bool MatchesHostPattern(std::string_view host, const HostPattern& pattern) {
  if (host.size() <= pattern.prefix.size() + pattern.suffix.size() ||
      !host.starts_with(pattern.prefix) || !host.ends_with(pattern.suffix)) {
    return false;
  }
  std::string_view label =
      host.substr(pattern.prefix.size(),
                  host.size() - pattern.prefix.size() - pattern.suffix.size());
  return label.find('.') == std::string_view::npos;
}

// Returns the production BYOID Gemini Enterprise parent domain paired with
// `url`'s Workforce Auth Portal host, or empty if `url` is not on an allowed
// production portal. `.google.com` hosts use 1P Gaia auth rather than
// Workforce Identity. Consult Chrome Security / CrAISec before adding hosts.
std::string_view GetPairedWorkforceAuthDomain(const GURL& url) {
  const std::string_view host = url.host();
  if (host == "auth.cloud.google") {
    return "cloud.google";
  }
  if (host == "auth.business.gemini.google") {
    return "business.gemini.google";
  }
  if (host == "auth.gemini-us.google") {
    return "gemini-us.google";
  }
  return {};
}

// Returns the pre-production BYOID Gemini Enterprise parent domain paired with
// `url`'s pre-production Workforce Auth Portal host, or empty if `url` does not
// match an allowed pattern. Consult Chrome Security / CrAISec before adding or
// widening patterns.
std::string_view GetPairedPreProductionWorkforceAuthDomain(
    const GURL& url LIFETIME_BOUND) {
  const std::string_view host = url.host();
  // e.g. auth-staging.corp.cloud.google, auth-autopush.corp.cloud.google
  if (MatchesHostPattern(host, {"auth-", ".corp.cloud.google"})) {
    return "corp.cloud.google";
  }
  // e.g. auth-staging.business-staging.gemini.google,
  // auth-autopush.business-autopush.gemini.google
  if (const size_t dot = host.find('.');
      dot != std::string_view::npos &&
      MatchesHostPattern(host.substr(0, dot), {"auth-", ""}) &&
      MatchesHostPattern(host.substr(dot + 1),
                         {"business-", ".gemini.google"})) {
    return host.substr(dot + 1);
  }
  // e.g. auth-staging.gemini-us.google, auth-autopush.gemini-us.google
  if (MatchesHostPattern(host, {"auth-", ".gemini-us.google"})) {
    return "gemini-us.google";
  }
  return {};
}

base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
ParseWorkforceAuthPortalUrlInternal(
    const GURL& url,
    std::string_view paired_domain,
    bool (*is_allowed_inner_host)(const GURL&)) {
  if (paired_domain.empty()) {
    return base::unexpected(
        WorkforceAuthPortalUrlError::kNotWorkforceAuthPortal);
  }

  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) || url.has_port() ||
      url.has_username() || url.has_password()) {
    return base::unexpected(WorkforceAuthPortalUrlError::kInvalidUrl);
  }

  // Matches `/signin[/]` (multi-provider pool; `provider` is empty) or
  // `/signin/locations/global/workforcePools/<pool>/providers/<provider>[/]`.
  static const base::NoDestructor<re2::RE2> kSignInPathPattern(
      "^/signin(?:/(locations/global/workforcePools/"
      "[a-z][a-z0-9-]{4,61}[a-z0-9]/providers/[a-z0-9-]{4,32}))?/?$");
  std::string_view provider;
  if (!re2::RE2::FullMatch(url.path(), *kSignInPathPattern, &provider)) {
    return base::unexpected(WorkforceAuthPortalUrlError::kInvalidUrl);
  }

  // Require a single non-empty `continueUrl` query parameter.
  std::optional<std::string> continue_url_spec;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() != "continueUrl") {
      continue;
    }
    if (continue_url_spec.has_value()) {
      return base::unexpected(WorkforceAuthPortalUrlError::kInvalidUrl);
    }
    continue_url_spec = it.GetUnescapedValue();
  }
  if (!continue_url_spec.has_value() || continue_url_spec->empty()) {
    return base::unexpected(WorkforceAuthPortalUrlError::kInvalidUrl);
  }

  GURL inner_url(*continue_url_spec);
  if (!inner_url.is_valid() || inner_url.has_port() ||
      inner_url.has_username() || inner_url.has_password()) {
    return base::unexpected(WorkforceAuthPortalUrlError::kInvalidUrl);
  }
  if (!inner_url.SchemeIs(url::kHttpsScheme)) {
    return base::unexpected(WorkforceAuthPortalUrlError::kInnerUrlNotHttps);
  }
  if (!is_allowed_inner_host(inner_url) || !inner_url.DomainIs(paired_domain)) {
    return base::unexpected(WorkforceAuthPortalUrlError::kDisallowedInnerHost);
  }

  // The outer Auth Portal path is authoritative for `idp`; strip any
  // pre-existing `idp` on `continueUrl` so a multi-provider `/signin/` URL does
  // not preserve a stale provider.
  std::string unused;
  if (net::GetValueForKeyInQuery(inner_url, kWorkforceProviderQueryParam,
                                 &unused)) {
    inner_url = net::AppendOrReplaceQueryParameter(
        inner_url, kWorkforceProviderQueryParam, std::nullopt);
    if (inner_url.query().empty()) {
      GURL::Replacements replacements;
      replacements.ClearQuery();
      inner_url = inner_url.ReplaceComponents(replacements);
    }
  }

  return WorkforceAuthPortalInfo{
      .inner_url = std::move(inner_url),
      .workforce_provider = std::string(provider),
  };
}

}  // namespace

bool IsAllowedGeminiEnterpriseHost(const GURL& url) {
  // IMPORTANT: Consult Chrome Security / CrAISec before adding any new domains
  // or hostnames to this allowlist. Any origin allowed here can be set through
  // the `GeminiEnterpriseUrl` policy, run in the GEiC side panel and receive
  // Glic Mojo API access.
  static constexpr auto kAllowedHosts =
      base::MakeFixedFlatSet<std::string_view>({
          "business.gemini.google",
          "enterprise.gemini.cloud.google",
          "enterprise.gemini.cloud.google.com",
          "enterprise.gemini.google.com",
          "gemini.cloud.google",
          "vertexaisearch.cloud.google",
          "vertexaisearch.cloud.google.com",
          "vertexaisearch.gemini-us.google",
          // Gemini Enterprise for Googlers, so it can be set through policy
          // (e.g. for dogfood rollouts). Exact host only; other
          // `ucs-*.corp.google.com` hosts are pre-production.
          "ucs-widget.corp.google.com",
      });
  return kAllowedHosts.contains(url.host());
}

bool IsAllowedGeminiEnterprisePreProductionHost(const GURL& url) {
  // IMPORTANT: Consult Chrome Security / CrAISec before adding or widening any
  // pattern. Each pattern is anchored to a Gemini Enterprise product prefix so
  // that other services on the same parent domain are not allowed.
  static constexpr HostPattern kPreProductionHostPatterns[] = {
      // e.g. business-staging.gemini.google
      {"business-", ".gemini.google"},
      // e.g. enterprise-autopush.gemini.google.com
      {"enterprise-", ".gemini.google.com"},
      // e.g. vertexaisearch-staging.gemini-us.google
      {"vertexaisearch-", ".gemini-us.google"},
      // e.g. ucs-widget-staging.corp.google.com,
      // ucs-app-autopush.corp.google.com
      {"ucs-", ".corp.google.com"},
      // e.g. ucs-widget-staging.corp.cloud.google
      {"ucs-", ".corp.cloud.google"},
  };
  return std::ranges::any_of(kPreProductionHostPatterns,
                             [host = url.host()](const HostPattern& pattern) {
                               return MatchesHostPattern(host, pattern);
                             });
}

base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
ParseWorkforceAuthPortalUrl(const GURL& url) {
  return ParseWorkforceAuthPortalUrlInternal(
      url, GetPairedWorkforceAuthDomain(url), &IsAllowedGeminiEnterpriseHost);
}

base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
ParsePreProductionWorkforceAuthPortalUrl(const GURL& url) {
  return ParseWorkforceAuthPortalUrlInternal(
      url, GetPairedPreProductionWorkforceAuthDomain(url),
      &IsAllowedGeminiEnterprisePreProductionHost);
}

}  // namespace geic
