// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/glic/public/features.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace geic {

bool IsAllowedGeminiEnterpriseHost(const GURL& url) {
  if (url.DomainIs("gemini.google") || url.DomainIs("gemini.google.com")) {
    return true;
  }

  static constexpr auto kAllowedCloudHosts =
      base::MakeFixedFlatSet<std::string_view>({
          "enterprise.gemini.cloud.google",
          "enterprise.gemini.cloud.google.com",
          "gemini.cloud.google",
          "vertexaisearch.cloud.google",
          "vertexaisearch.cloud.google.com",
      });
  if (kAllowedCloudHosts.contains(url.host())) {
    return true;
  }

  const GURL finch_url(features::kGeicGuestURL.Get());
  return finch_url.is_valid() && finch_url.SchemeIs(url::kHttpsScheme) &&
         url.host() == finch_url.host();
}

}  // namespace geic
