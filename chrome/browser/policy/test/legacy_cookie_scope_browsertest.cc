// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/browser/browsing_data/browsing_data_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace policy {

class LegacyCookieScopePolicyBrowserTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  LegacyCookieScopePolicyBrowserTest() {
    if (AreFeaturesEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {net::features::kEnablePortBoundCookies,
           net::features::kEnableSchemeBoundCookies},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {net::features::kEnablePortBoundCookies,
               net::features::kEnableSchemeBoundCookies});
    }
  }

  bool AreFeaturesEnabled() const { return GetParam(); }

 protected:
  GURL example_port_80_ = GURL("https://www.example.com:80/");

  GURL example_port_800_ = GURL("https://www.example.com:800/");

  GURL example_port_8000_ = GURL("https://www.example.com:8000/");

  GURL example_port_10_ = GURL("https://www.example.com:10/");

  GURL foo_port_80_ = GURL("https://www.foo.com:80/");

  GURL foo_port_800_ = GURL("https://www.foo.com:800/");

  GURL foo_port_8000_ = GURL("https://www.foo.com:8000/");

  GURL foo_port_10_ = GURL("https://www.foo.com:10/");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(LegacyCookieScopePolicyBrowserTest,
                       TestLegacyCookieScopeEnabled) {
  Profile* profile = browser()->profile();
  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, example_port_80_).empty());

  // Add cookies for example.com and foo.com that alias each other.
  ASSERT_TRUE(
      content::SetCookie(profile, example_port_80_, "example_cookie=1"));

  ASSERT_TRUE(
      content::SetCookie(profile, example_port_800_, "example_cookie=2"));

  ASSERT_TRUE(
      content::SetCookie(profile, example_port_8000_, "example_cookie=3"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_80_, "foo_cookie=1"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_800_, "foo_cookie=2"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_8000_, "foo_cookie=3"));

  auto cookies = content::GetAllCookies(profile);
  // When the feature is enabled, aliasing cookies will not be deleted.
  // Otherwise they are not aliasing and just overwrite.
  EXPECT_EQ(cookies.size(), AreFeaturesEnabled() ? 6u : 2u);

  PolicyMap policies;
  // Enables the policy, which will delete all aliasing cookies under each
  // domain once enabled.
  SetPolicy(&policies, key::kLegacyCookieScopeEnabled, base::Value(1));
  UpdateProviderPolicy(policies);

  // Adding an aliasing cookie after setting the policy triggers aliasing cookie
  // deletion for that domain.
  ASSERT_TRUE(
      content::SetCookie(profile, example_port_10_, "example_cookie=4"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_10_, "foo_cookie=4"));

  cookies = content::GetAllCookies(profile);
  // Only 2 cookies should remain, the last cookie for each domain.
  EXPECT_EQ(cookies.size(), 2u);
  // Only the last cookie should be present, all aliases should be deleted.
  EXPECT_EQ(cookies[0].Name(), "example_cookie");
  EXPECT_EQ(cookies[0].Value(), "4");
  EXPECT_EQ(cookies[1].Name(), "foo_cookie");
  EXPECT_EQ(cookies[1].Value(), "4");
}

IN_PROC_BROWSER_TEST_P(LegacyCookieScopePolicyBrowserTest,
                       TestLegacyCookieScopeEnabledForDomainList) {
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, example_port_80_).empty());

  // Add cookies for example.com and foo.com that alias each other.
  ASSERT_TRUE(
      content::SetCookie(profile, example_port_80_, "example_cookie=1"));

  ASSERT_TRUE(
      content::SetCookie(profile, example_port_800_, "example_cookie=2"));

  ASSERT_TRUE(
      content::SetCookie(profile, example_port_8000_, "example_cookie=3"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_80_, "foo_cookie=1"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_800_, "foo_cookie=2"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_8000_, "foo_cookie=3"));

  auto cookies = content::GetAllCookies(profile);

  // When the feature is enabled, aliasing cookies will not be deleted.
  // Otherwise they are not aliasing and just overwrite.
  EXPECT_EQ(cookies.size(), AreFeaturesEnabled() ? 6u : 2u);

  base::Value::List policy_value;
  // Set the policy to enable legacy cookie scope for the domains in the list.
  policy_value.Append("example.com");
  policy_value.Append("foo.com");

  PolicyMap policies;
  SetPolicy(&policies, key::kLegacyCookieScopeEnabledForDomainList,
            base::Value(std::move(policy_value)));
  UpdateProviderPolicy(policies);

  // Adding aliasing cookie after setting policy so all previous aliases should
  // be deleted.
  ASSERT_TRUE(
      content::SetCookie(profile, example_port_10_, "example_cookie=4"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_10_, "foo_cookie=4"));

  cookies = content::GetAllCookies(profile);
  // Only the last cookie should be present, all aliases should be deleted.
  EXPECT_EQ(cookies.size(), 2u);

  EXPECT_EQ(cookies[0].Name(), "example_cookie");
  EXPECT_EQ(cookies[0].Value(), "4");
  EXPECT_EQ(cookies[1].Name(), "foo_cookie");
  EXPECT_EQ(cookies[1].Value(), "4");
}

IN_PROC_BROWSER_TEST_P(LegacyCookieScopePolicyBrowserTest,
                       TestLegacyCookieScopeEnabledForSingleDomain) {
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, example_port_80_).empty());

  // Add cookies for example.com and foo.com that alias each other.
  ASSERT_TRUE(
      content::SetCookie(profile, example_port_80_, "example_cookie=1"));

  ASSERT_TRUE(
      content::SetCookie(profile, example_port_800_, "example_cookie=2"));

  ASSERT_TRUE(
      content::SetCookie(profile, example_port_8000_, "example_cookie=3"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_80_, "foo_cookie=1"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_800_, "foo_cookie=2"));

  ASSERT_TRUE(content::SetCookie(profile, foo_port_8000_, "foo_cookie=3"));

  auto cookies = content::GetAllCookies(profile);

  if (AreFeaturesEnabled()) {
    EXPECT_EQ(cookies.size(), 6u);
  } else {
    EXPECT_EQ(cookies.size(), 2u);
  }

  base::Value::List policy_value;
  // Set the policy to enable legacy cookie scope for the example.com
  // Aliases under foo.com should not be deleted due to this.
  policy_value.Append("example.com");

  PolicyMap policies;
  SetPolicy(&policies, key::kLegacyCookieScopeEnabledForDomainList,
            base::Value(std::move(policy_value)));
  UpdateProviderPolicy(policies);

  // Adding aliasing cookie after setting policy so all previous aliases should
  // be deleted.
  ASSERT_TRUE(
      content::SetCookie(profile, example_port_10_, "example_cookie=4"));
  // foo_cookie=4 and all of it's aliases should not be deleted.
  ASSERT_TRUE(content::SetCookie(profile, foo_port_10_, "foo_cookie=4"));

  cookies = content::GetAllCookies(profile);
  // Separate cookies by name.
  std::vector<net::CanonicalCookie> example_cookies;
  std::vector<net::CanonicalCookie> foo_cookies;
  for (const auto& cookie : cookies) {
    if (cookie.Name() == "example_cookie") {
      example_cookies.push_back(cookie);
    } else if (cookie.Name() == "foo_cookie") {
      foo_cookies.push_back(cookie);
    }
  }

  // Check the number of cookies for each name.
  // Only one example_cookie should remain.
  EXPECT_EQ(example_cookies.size(), 1u);
  EXPECT_EQ(example_cookies[0].Value(), "4");
  if (AreFeaturesEnabled()) {
    // All foo_cookie should remain.
    EXPECT_EQ(foo_cookies.size(), 4u);
  } else {
    // Without port-bound cookies, these are not aliasing and just overwrite.
    EXPECT_EQ(foo_cookies.size(), 1u);
    EXPECT_EQ(foo_cookies[0].Value(), "4");
  }
}

IN_PROC_BROWSER_TEST_P(
    LegacyCookieScopePolicyBrowserTest,
    TestLegacyCookieScopeEnabledForDomainListPartitionedCookies) {
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, example_port_80_).empty());

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://www.example.com"));

  // Add cookies for example.com and foo.com that alias each other.
  ASSERT_TRUE(content::SetCookie(
      profile, example_port_80_,
      "example_cookie=1;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, example_port_800_,
      "example_cookie=2;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, example_port_8000_,
      "example_cookie=3;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_80_, "foo_cookie=1;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_800_, "foo_cookie=2;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_8000_, "foo_cookie=3;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  auto cookies = content::GetAllCookies(profile);

  // When the feature is enabled, aliasing cookies will not be deleted.
  // Otherwise they are not aliasing and just overwrite.
  EXPECT_EQ(cookies.size(), AreFeaturesEnabled() ? 6u : 2u);

  base::Value::List policy_value;
  // Set the policy to enable legacy cookie scope for the domains in the list.
  policy_value.Append("example.com");
  policy_value.Append("foo.com");

  PolicyMap policies;
  SetPolicy(&policies, key::kLegacyCookieScopeEnabledForDomainList,
            base::Value(std::move(policy_value)));
  UpdateProviderPolicy(policies);

  // Adding aliasing cookie after setting policy so all previous aliases should
  // be deleted.
  ASSERT_TRUE(content::SetCookie(
      profile, example_port_10_,
      "example_cookie=4;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_10_, "foo_cookie=4;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  cookies = content::GetAllCookies(profile);
  // Only the last cookie should be present, all aliases should be deleted.
  EXPECT_EQ(cookies.size(), 2u);

  EXPECT_EQ(cookies[0].Name(), "example_cookie");
  EXPECT_EQ(cookies[0].Value(), "4");
  EXPECT_TRUE(cookies[0].IsPartitioned());
  EXPECT_EQ(cookies[1].Name(), "foo_cookie");
  EXPECT_EQ(cookies[1].Value(), "4");
  EXPECT_TRUE(cookies[1].IsPartitioned());
}

IN_PROC_BROWSER_TEST_P(LegacyCookieScopePolicyBrowserTest,
                       TestLegacyCookieScopeEnabledPartitionedCookies) {
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, example_port_80_).empty());

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://www.example.com"));

  // Add cookies for example.com and foo.com that alias each other.
  ASSERT_TRUE(content::SetCookie(
      profile, example_port_80_,
      "example_cookie=1;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, example_port_800_,
      "example_cookie=2;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, example_port_8000_,
      "example_cookie=3;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_80_, "foo_cookie=1;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_800_, "foo_cookie=2;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_8000_, "foo_cookie=3;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  auto cookies = content::GetAllCookies(profile);

  // When the feature is enabled, aliasing cookies will not be deleted.
  // Otherwise they are not aliasing and just overwrite.
  EXPECT_EQ(cookies.size(), AreFeaturesEnabled() ? 6u : 2u);

  base::Value::List policy_value;
  // Set the policy to enable legacy cookie scope for the domains in the list.
  policy_value.Append("example.com");
  policy_value.Append("foo.com");

  PolicyMap policies;
  SetPolicy(&policies, key::kLegacyCookieScopeEnabled, base::Value(1));
  UpdateProviderPolicy(policies);

  // Adding aliasing cookie after setting policy so all previous aliases should
  // be deleted.
  ASSERT_TRUE(content::SetCookie(
      profile, example_port_10_,
      "example_cookie=4;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  ASSERT_TRUE(content::SetCookie(
      profile, foo_port_10_, "foo_cookie=4;SameSite=None;Secure;Partitioned",
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

  cookies = content::GetAllCookies(profile);
  // Only the last cookie should be present, all aliases should be deleted.
  EXPECT_EQ(cookies.size(), 2u);

  EXPECT_EQ(cookies[0].Name(), "example_cookie");
  EXPECT_EQ(cookies[0].Value(), "4");
  EXPECT_TRUE(cookies[0].IsPartitioned());

  EXPECT_EQ(cookies[1].Name(), "foo_cookie");
  EXPECT_EQ(cookies[1].Value(), "4");
  EXPECT_TRUE(cookies[1].IsPartitioned());
}

INSTANTIATE_TEST_SUITE_P(All,
                         LegacyCookieScopePolicyBrowserTest,
                         testing::Bool());

}  // namespace policy
