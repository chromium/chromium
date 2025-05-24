// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_utils.h"

#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace privacy_sandbox {

std::string GetEmbeddedPrivacyPolicyURL(PrivacyPolicyDomainType domain_type,
                                        PrivacyPolicyColorScheme color_scheme,
                                        const std::string& locale) {
  GURL base_url(domain_type == PrivacyPolicyDomainType::kChina
                    ? chrome::kPrivacyPolicyEmbeddedURLPathChina
                    : chrome::kPrivacyPolicyOnlineURLPath);
  if (!locale.empty()) {
    base_url = google_util::AppendGoogleLocaleParam(base_url, locale);
  }
  if (color_scheme == PrivacyPolicyColorScheme::kDarkMode) {
    base_url =
        net::AppendOrReplaceQueryParameter(base_url, "color_scheme", "dark");
  }
  return base_url.spec();
}

}  // namespace privacy_sandbox
