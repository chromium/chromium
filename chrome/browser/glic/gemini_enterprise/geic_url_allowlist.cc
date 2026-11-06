// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"

#include <algorithm>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "url/gurl.h"

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

}  // namespace geic
