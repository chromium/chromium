// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/url_utils.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_util.h"

namespace ash {
namespace input_method {

// Checks if domain is a sub-domain of url
bool IsSubDomain(const GURL& url, std::string_view domain) {
  const size_t registryLength =
      net::registry_controlled_domains::GetRegistryLength(
          url, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  // Localhost is valid and we want to deny features on it but has not registry.
  if (registryLength == 0 && domain != "localhost") {
    return false;
  }
  const std::string_view urlContent = url.host_piece();
  const std::string_view urlDomain = urlContent.substr(
      0, urlContent.length() - registryLength - (registryLength == 0 ? 0 : 1));

  return url::DomainIs(urlDomain, domain);
}

// Checks if url belongs to domain and has the path_prefix
bool IsSubDomainWithPathPrefix(const GURL& url,
                               std::string_view domain,
                               std::string_view path_prefix) {
  return IsSubDomain(url, domain) && url.has_path() &&
         base::StartsWith(url.path(), path_prefix);
}

}  // namespace input_method
}  // namespace ash
