// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_cookie_access_policy.h"

#include <string_view>

#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

using PrivacySetting = net::NetworkDelegate::PrivacySetting;

struct CanAccessCookiesTestCase {
  std::string_view url;
  std::string_view site_for_cookies_url;
  bool accept_cookies;
  bool accept_third_party_cookies;
  PrivacySetting expected;
};

class AwCookieAccessPolicyTableTest
    : public testing::TestWithParam<CanAccessCookiesTestCase> {};

TEST_P(AwCookieAccessPolicyTableTest, CanAccessCookies) {
  const auto& test_case = GetParam();
  GURL url(test_case.url);
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromUrl(GURL(test_case.site_for_cookies_url));

  EXPECT_EQ(AwCookieAccessPolicy::CanAccessCookies(
                url, site_for_cookies, test_case.accept_cookies,
                test_case.accept_third_party_cookies),
            test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AwCookieAccessPolicyTableTest,
    testing::Values(
        // DisallowCookies
        CanAccessCookiesTestCase{"https://example.com", "https://example.com",
                                 /*accept_cookies=*/false,
                                 /*accept_third_party_cookies=*/false,
                                 PrivacySetting::kStateDisallowed},
        CanAccessCookiesTestCase{"https://example.com", "https://example.com",
                                 /*accept_cookies=*/false,
                                 /*accept_third_party_cookies=*/true,
                                 PrivacySetting::kStateDisallowed},

        // AllowThirdPartyCookies
        CanAccessCookiesTestCase{"https://other.com", "https://example.com",
                                 /*accept_cookies=*/true,
                                 /*accept_third_party_cookies=*/true,
                                 PrivacySetting::kStateAllowed},

        // FileSchemeAllowed
        CanAccessCookiesTestCase{
            "file:///path/to/file.html", "file:///path/to/file.html",
            /*accept_cookies=*/true, /*accept_third_party_cookies=*/false,
            PrivacySetting::kStateAllowed},

        // BlockThirdPartyCookies: First-party
        CanAccessCookiesTestCase{"https://example.com", "https://example.com",
                                 /*accept_cookies=*/true,
                                 /*accept_third_party_cookies=*/false,
                                 PrivacySetting::kStateAllowed},

        // BlockThirdPartyCookies: Third-party
        CanAccessCookiesTestCase{
            "https://other.com", "https://example.com",
            /*accept_cookies=*/true, /*accept_third_party_cookies=*/false,
            PrivacySetting::kPartitionedStateAllowedOnly}));

}  // namespace android_webview
