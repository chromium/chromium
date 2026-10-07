// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/topics/topics_feedback_url_util.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

// Lowercase names of query parameters whose values are treated as secrets.
constexpr auto kSecretQueryParams = base::MakeFixedFlatSet<std::string_view>({
    "access_token",
    "api_key",
    "apikey",
    "auth",
    "code",
    "id_token",
    "key",
    "passwd",
    "password",
    "refresh_token",
    "secret",
    "session",
    "session_id",
    "sessionid",
    "sig",
    "signature",
    "token",
});

// Hosts excluded by default, matched exactly.
constexpr auto kExcludedHosts = base::MakeFixedFlatSet<std::string_view>({
    "calendar.google.com",
    "chat.google.com",
    "colab.research.google.com",
    "docs.google.com",
    "drive.google.com",
    "goto.google.com",
    "mail.google.com",
    "meet.google.com",
});

// Domains excluded by default along with all of their subdomains.
constexpr std::string_view kExcludedDomainSuffixes[] = {
    "corp.google.com",
    "googleplex.com",
};

// Characters that separate query parameters. ';' is not standard, but some
// servers (including some internal Google ones) accept it in place of '&'.
constexpr std::string_view kQueryParamSeparators = "&;";

bool IsSecretQueryParam(std::string_view escaped_name) {
  // Unescape everything, so no escaping can disguise a secret name.
  std::string name = base::UnescapeBinaryURLComponent(
      escaped_name, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
  std::ranges::transform(name, name.begin(),
                         [](char c) { return base::ToLowerASCII(c); });
  return kSecretQueryParams.contains(name);
}

// Returns `query` with the value of every secret parameter replaced by
// `kTopicsFeedbackRedactedValue`, leaving everything else, including which
// separator each parameter is followed by, byte-for-byte intact.
std::string RedactSecretQueryValues(std::string_view query) {
  std::string result;
  result.reserve(query.size());
  while (true) {
    const size_t separator = query.find_first_of(kQueryParamSeparators);
    const std::string_view param = query.substr(0, separator);
    const size_t eq = param.find('=');
    if (eq != std::string_view::npos &&
        IsSecretQueryParam(param.substr(0, eq))) {
      base::StrAppend(&result,
                      {param.substr(0, eq + 1), kTopicsFeedbackRedactedValue});
    } else {
      result += param;
    }
    if (separator == std::string_view::npos) {
      return result;
    }
    result += query[separator];
    query.remove_prefix(separator + 1);
  }
}

}  // namespace

std::optional<GURL> MinimizeUrlForTopicsFeedback(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  std::string query;
  if (url.has_query()) {
    query = RedactSecretQueryValues(url.query());
    replacements.SetQueryStr(query);
  }
  return url.ReplaceComponents(replacements);
}

bool IsDefaultExcludedDomain(std::string_view host) {
  // GURL keeps the trailing dot of a fully qualified host, e.g.
  // "docs.google.com.", which refers to the same host as "docs.google.com".
  const std::string trimmed_host = net::TrimEndingDot(host);
  if (kExcludedHosts.contains(trimmed_host)) {
    return true;
  }
  return std::ranges::any_of(kExcludedDomainSuffixes,
                             [&trimmed_host](std::string_view domain) {
                               return net::IsSubdomainOf(trimmed_host, domain);
                             });
}

}  // namespace context_hub
