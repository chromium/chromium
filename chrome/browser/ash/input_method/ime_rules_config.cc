// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_rules_config.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_util.h"

namespace ash {
namespace input_method {
namespace {

// The default denylist of domains that will turn off autocorrect and multi word
// suggestions.
const char* kDefaultDomainDenylist[] = {
    "amazon",
    "b.corp.google",
    "buganizer.corp.google",
    "cider.corp.google",
    "classroom.google",
    "desmos",
    "docs.google",
    "facebook",
    "instagram",
    "mail.google",
    "outlook.live",
    "outlook.office",
    "quizlet",
    "reddit",
    "web.skype",
    "teams.microsoft",
    "twitter",
    "whatsapp",
    "youtube",
};

// exceptions where the features are enabled.
const char* kAllowedDomainsWithPaths[][2] = {{"mail.google", "/chat"}};
}  // namespace

// Checks if domain is a sub-domain of url
bool IsSubDomain(const GURL& url, const base::StringPiece domain) {
  const size_t registryLength =
      net::registry_controlled_domains::GetRegistryLength(
          url, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  // Localhost is valid and we want to deny features on it but has not registry.
  if (registryLength == 0 && domain != "localhost") {
    return false;
  }
  const base::StringPiece urlContent = url.host_piece();
  const base::StringPiece urlDomain = urlContent.substr(
      0, urlContent.length() - registryLength - (registryLength == 0 ? 0 : 1));

  return url::DomainIs(urlDomain, domain);
}

// Checks if url belongs to domain and has the path_prefix
bool IsSubDomainWithPathPrefix(const GURL& url,
                               const base::StringPiece domain,
                               const base::StringPiece path_prefix) {
  return IsSubDomain(url, domain) && url.has_path() &&
         base::StartsWith(url.path(), path_prefix);
}

bool IsMatchedSubDomain(const GURL& url) {
  for (const char* domain : kDefaultDomainDenylist) {
    if (IsSubDomain(url, domain)) {
      return true;
    }
  }
  return false;
}

bool IsMatchedSubDomainWithPathPrefix(const GURL& url) {
  for (const auto& [domain, path_prefix] : kAllowedDomainsWithPaths) {
    if (IsSubDomainWithPathPrefix(url, domain, path_prefix)) {
      return true;
    }
  }
  return false;
}

bool IsAssistiveInputDisabled(const absl::optional<GURL>& url) {
  if (!url.has_value()) {
    return false;
  }
  return IsMatchedSubDomain(*url) && !IsMatchedSubDomainWithPathPrefix(*url);
}
}  // namespace input_method
}  // namespace ash
