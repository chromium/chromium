// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/url_utils.h"

#include <optional>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_util.h"

namespace ash::input_method {

// Checks if domain is a sub-domain of url
bool IsSubDomain(const GURL& url, std::string_view domain) {
  const std::optional<std::string_view> registry =
      net::registry_controlled_domains::GetRegistry(
          url, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if (!registry) {
    return false;
  }
  // Localhost is valid and we want to deny features on it but has not registry.
  if (registry->empty() && domain != "localhost") {
    return false;
  }
  if (!registry->empty()) {
    // Host should consist of more than just the registry and a dot.
    CHECK_GT(url.host().size(), registry->size() + 1);
  }
  const std::string_view url_domain =
      registry->empty()
          ? url.host()
          : url.host().substr(0, url.host().size() - registry->size() - 1);

  return url::DomainIs(url_domain, domain);
}

// Checks if url belongs to domain and has the path_prefix
bool IsSubDomainWithPathPrefix(const GURL& url,
                               std::string_view domain,
                               std::string_view path_prefix) {
  return IsSubDomain(url, domain) && url.has_path() &&
         base::StartsWith(url.GetPath(), path_prefix);
}

// Checks if url is a file with a matching extension
bool HasFileExtension(const GURL& url, std::string_view extension) {
  return base::EndsWith(url.GetPath(), base::StrCat({".", extension}),
                        base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace ash::input_method
