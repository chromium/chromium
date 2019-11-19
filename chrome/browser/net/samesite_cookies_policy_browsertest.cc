// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
const char kURL[] = "http://example.com";
}  // namespace

// Test fixture that enables (if param is true) and disables (if param is false)
// SameSite-by-default and Cookies-without-SameSite-must-be-Secure, to test the
// policies that override those features, under both conditions.
class SameSiteCookiesPolicyTest : public PolicyTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  SameSiteCookiesPolicyTest() {
    std::vector<base::Feature> samesite_features = {
        net::features::kSameSiteByDefaultCookies,
        net::features::kCookiesWithoutSameSiteMustBeSecure};
    if (AreSameSiteFeaturesEnabled()) {
      feature_list_.InitWithFeatures(samesite_features /* enabled */, {});
    } else {
      feature_list_.InitWithFeatures({}, samesite_features /* disabled */);
    }
  }

  ~SameSiteCookiesPolicyTest() = default;

 protected:
  bool AreSameSiteFeaturesEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       DefaultLegacyCookieAccessSettingIsAllow) {
  PolicyMap policies;
  // Set a policy to allow Legacy access for all cookies.
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabled,
            std::make_unique<base::Value>(1));
  UpdateProviderPolicy(policies);

  GURL url(kURL);
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, url).empty());

  // Set a cookie from a same-site context. The cookie does not specify
  // SameSite, so it may default to Lax if the SameSite features are enabled.
  // Since the context used is same-site, it should always work.
  EXPECT_TRUE(content::SetCookie(
      profile, url, "samesite-unspecified=1",
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX));
  EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));

  // Overwrite the cookie from a cross-site context. Because we have a policy
  // that allows Legacy access for all domains, this will work even if the
  // SameSite features are enabled. (It works regardless, if they are disabled.)
  EXPECT_TRUE(content::SetCookie(
      profile, url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
  // Cookie has the new value because we were able to successfully overwrite it.
  EXPECT_EQ("samesite-unspecified=2", content::GetCookies(profile, url));
  // Fetching the cookies from a cross-site context also works because of the
  // policy.
  EXPECT_EQ(
      "samesite-unspecified=2",
      content::GetCookies(
          profile, url, net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       DefaultLegacyCookieAccessSettingIsBlock) {
  PolicyMap policies;
  // Set a policy to block Legacy access for all cookies.
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabled,
            std::make_unique<base::Value>(2));
  UpdateProviderPolicy(policies);

  GURL url(kURL);
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, url).empty());

  // Set a cookie from a same-site context. The cookie does not specify
  // SameSite, so it may default to Lax if the SameSite features are enabled.
  // Since the context used is same-site, it should always work.
  EXPECT_TRUE(content::SetCookie(
      profile, url, "samesite-unspecified=1",
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX));
  EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));

  // Overwrite the cookie from a cross-site context. Because we have a policy
  // that blocks Legacy access for all domains, this will not work even if the
  // SameSite features are disabled. (It doesn't work regardless, if they are
  // enabled.)
  EXPECT_FALSE(content::SetCookie(
      profile, url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
  // Cookie still has the previous value because re-setting it failed.
  EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));
  // Fetching the unspecified-samesite cookie from a cross-site context does not
  // work because of the policy.
  EXPECT_EQ("", content::GetCookies(
                    profile, url,
                    net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       AllowLegacyCookieAccessForDomain) {
  GURL legacy_allowed_domain_url(kURL);
  GURL other_domain_url("http://other-domain.example");

  // Set a policy to allow Legacy cookie access for one domain only.
  auto policy_value = std::make_unique<base::Value>(base::Value::Type::LIST);
  policy_value->Append(legacy_allowed_domain_url.host());

  PolicyMap policies;
  // Set a policy to allow Legacy access for the given domain only.
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabledForDomainList,
            std::move(policy_value));
  UpdateProviderPolicy(policies);

  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, legacy_allowed_domain_url).empty());
  ASSERT_TRUE(content::GetCookies(profile, other_domain_url).empty());

  // Set a cookie from a same-site context. The cookie does not specify
  // SameSite, so it may default to Lax if the SameSite features are enabled.
  // Since the context used is same-site, it should always work.
  EXPECT_TRUE(content::SetCookie(
      profile, legacy_allowed_domain_url, "samesite-unspecified=1",
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX));
  EXPECT_EQ("samesite-unspecified=1",
            content::GetCookies(profile, legacy_allowed_domain_url));
  // Do the same on the other domain...
  EXPECT_TRUE(content::SetCookie(
      profile, other_domain_url, "samesite-unspecified=1",
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX));
  EXPECT_EQ("samesite-unspecified=1",
            content::GetCookies(profile, other_domain_url));

  // Overwrite the cookie from a cross-site context. Because we have a policy
  // that allows Legacy access for one domain but not the other, this will work
  // on the policy-specified domain even if SameSite features are enabled, but
  // it will not work for the other domain. (It works regardless, if they are
  // disabled.)
  EXPECT_TRUE(content::SetCookie(
      profile, legacy_allowed_domain_url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
  EXPECT_EQ("samesite-unspecified=2",
            content::GetCookies(profile, legacy_allowed_domain_url));
  EXPECT_EQ("samesite-unspecified=2",
            content::GetCookies(
                profile, legacy_allowed_domain_url,
                net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
  // For the domain that is not Legacy by policy, we expect it to work only if
  // the SameSite features are disabled.
  if (AreSameSiteFeaturesEnabled()) {
    EXPECT_FALSE(content::SetCookie(
        profile, other_domain_url, "samesite-unspecified=2",
        net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
    EXPECT_EQ("samesite-unspecified=1",
              content::GetCookies(profile, other_domain_url));
    EXPECT_EQ("", content::GetCookies(
                      profile, other_domain_url,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
  } else {
    EXPECT_TRUE(content::SetCookie(
        profile, other_domain_url, "samesite-unspecified=2",
        net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
    EXPECT_EQ("samesite-unspecified=2",
              content::GetCookies(profile, other_domain_url));
    EXPECT_EQ("samesite-unspecified=2",
              content::GetCookies(
                  profile, other_domain_url,
                  net::CookieOptions::SameSiteCookieContext::CROSS_SITE));
  }
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         SameSiteCookiesPolicyTest,
                         ::testing::Bool());

}  // namespace policy
