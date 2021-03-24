// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
const char kURL[] = "http://example.com";
}  // namespace

// Test fixture that enables (if param is true) and disables (if param is false)
// SameSite-by-default, Cookies-without-SameSite-must-be-Secure, and Schemeful
// Same-Site to test the policies that override those features, under both
// conditions.
class SameSiteCookiesPolicyTest : public PolicyTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  SameSiteCookiesPolicyTest() {
    std::vector<base::Feature> samesite_features = {
        net::features::kSameSiteByDefaultCookies,
        net::features::kCookiesWithoutSameSiteMustBeSecure,
        net::features::kSchemefulSameSite};
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

class SchemefulSameSiteCookiesPolicyIntegrationTest
    : public policy::PolicyTest {
 protected:
  SchemefulSameSiteCookiesPolicyIntegrationTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(http_server_.Start());
  }

  GURL GetURL(const std::string& host, const std::string& path, bool secure) {
    if (secure)
      return https_server_.GetURL(host, path);

    return http_server_.GetURL(host, path);
  }

  GURL GetURL(const std::string& host, bool secure) {
    return GetURL(host, "/", secure);
  }

  content::RenderFrameHost* GetChildFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetMainFrame(), 0);
  }

  content::RenderFrameHost* GetMainFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetMainFrame();
  }

  void NavigateToHttpPageWithFrame(const std::string& host) {
    GURL main_url(http_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);
  }

  void NavigateFrameToHttps(const std::string& host, const std::string& path) {
    GURL page = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  void ExpectFrameContent(content::RenderFrameHost* frame,
                          const std::string& expected) {
    storage::test::ExpectFrameContent(frame, expected);
  }

  net::test_server::EmbeddedTestServer http_server_;
  net::test_server::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchemefulSameSiteCookiesPolicyIntegrationTest);
};

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       DefaultLegacyCookieAccessSettingIsAllow) {
  PolicyMap policies;
  // Set a policy to allow Legacy access for all cookies.
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabled,
            base::Value(1));
  UpdateProviderPolicy(policies);

  GURL url(kURL);
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, url).empty());

  // Set a cookie from a same-site context. The cookie does not specify
  // SameSite, so it may default to Lax if the SameSite features are enabled.
  // Since the context used is same-site, it should always work.
  EXPECT_TRUE(content::SetCookie(profile, url, "samesite-unspecified=1",
                                 net::CookieOptions::SameSiteCookieContext(
                                     net::CookieOptions::SameSiteCookieContext::
                                         ContextType::SAME_SITE_LAX)));
  EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));

  // Overwrite the cookie from a cross-site context. Because we have a policy
  // that allows Legacy access for all domains, this will work even if the
  // SameSite features are enabled. (It works regardless, if they are disabled.)
  EXPECT_TRUE(content::SetCookie(
      profile, url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE)));
  // Cookie has the new value because we were able to successfully overwrite it.
  EXPECT_EQ("samesite-unspecified=2", content::GetCookies(profile, url));
  // Fetching the cookies from a cross-site context also works because of the
  // policy.
  EXPECT_EQ("samesite-unspecified=2",
            content::GetCookies(profile, url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));

  // When Schemeful Same-Site is enabled a context downgrade to an insufficient
  // context should still be allowed with legacy access. This'll always work if
  // Schemeful Same-Site is disabled because the schemeless context is Lax
  // which is sufficient.
  EXPECT_TRUE(content::SetCookie(
      profile, url, "samesite-lax=1; SameSite=Lax",
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE)));
  // Similarly when we try to get the cookie.
  EXPECT_THAT(
      content::GetCookies(profile, url,
                          net::CookieOptions::SameSiteCookieContext(
                              net::CookieOptions::SameSiteCookieContext::
                                  ContextType::SAME_SITE_LAX,
                              net::CookieOptions::SameSiteCookieContext::
                                  ContextType::CROSS_SITE)),
      testing::HasSubstr("samesite-lax=1"));
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       DefaultLegacyCookieAccessSettingIsBlock) {
  PolicyMap policies;
  // Set a policy to block Legacy access for all cookies.
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabled,
            base::Value(2));
  UpdateProviderPolicy(policies);

  GURL url(kURL);
  Profile* profile = browser()->profile();

  // No cookies at startup
  ASSERT_TRUE(content::GetCookies(profile, url).empty());

  // Set a cookie from a same-site context. The cookie does not specify
  // SameSite, so it may default to Lax if the SameSite features are enabled.
  // Since the context used is same-site, it should always work.
  EXPECT_TRUE(content::SetCookie(profile, url, "samesite-unspecified=1",
                                 net::CookieOptions::SameSiteCookieContext(
                                     net::CookieOptions::SameSiteCookieContext::
                                         ContextType::SAME_SITE_LAX)));
  EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));

  // Overwrite the cookie from a cross-site context. Because we have a policy
  // that blocks Legacy access for all domains, this will not work even if the
  // SameSite features are disabled. (It doesn't work regardless, if they are
  // enabled.)
  EXPECT_FALSE(content::SetCookie(
      profile, url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE)));
  // Cookie still has the previous value because re-setting it failed.
  EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));
  // Fetching the unspecified-samesite cookie from a cross-site context does not
  // work because of the policy.
  EXPECT_EQ("",
            content::GetCookies(profile, url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));

  // When Schemeful Same-Site is enabled a context downgrade to an insufficient
  // context should always be blocked. If Schemeful Same-Site is disabled then
  // this shouldn't be blocked.
  // Similarly when we try to get the cookie.
  if (AreSameSiteFeaturesEnabled()) {
    EXPECT_FALSE(
        content::SetCookie(profile, url, "samesite-lax=1; SameSite=Lax",
                           net::CookieOptions::SameSiteCookieContext(
                               net::CookieOptions::SameSiteCookieContext::
                                   ContextType::SAME_SITE_LAX,
                               net::CookieOptions::SameSiteCookieContext::
                                   ContextType::CROSS_SITE)));
    // We should be able to get the cookie which was previously added.
    EXPECT_EQ("samesite-unspecified=1", content::GetCookies(profile, url));
    // But no cookies should be returned for a downgrade to an insufficient
    // context, since SameSite-by-default is active which requires a minimum of
    // a Lax context.
    EXPECT_EQ(
        "", content::GetCookies(profile, url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::SAME_SITE_LAX,
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));
  } else {
    EXPECT_TRUE(
        content::SetCookie(profile, url, "samesite-lax=1; SameSite=Lax",
                           net::CookieOptions::SameSiteCookieContext(
                               net::CookieOptions::SameSiteCookieContext::
                                   ContextType::SAME_SITE_LAX,
                               net::CookieOptions::SameSiteCookieContext::
                                   ContextType::CROSS_SITE)));
    EXPECT_THAT(
        content::GetCookies(profile, url,
                            net::CookieOptions::SameSiteCookieContext(
                                net::CookieOptions::SameSiteCookieContext::
                                    ContextType::SAME_SITE_LAX,
                                net::CookieOptions::SameSiteCookieContext::
                                    ContextType::CROSS_SITE)),
        testing::HasSubstr("samesite-lax=1"));
  }
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       AllowLegacyCookieAccessForDomain) {
  GURL legacy_allowed_domain_url(kURL);
  GURL other_domain_url("http://other-domain.example");

  // Set a policy to allow Legacy cookie access for one domain only.
  base::Value policy_value(base::Value::Type::LIST);
  policy_value.Append(legacy_allowed_domain_url.host());

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
  EXPECT_TRUE(content::SetCookie(profile, legacy_allowed_domain_url,
                                 "samesite-unspecified=1",
                                 net::CookieOptions::SameSiteCookieContext(
                                     net::CookieOptions::SameSiteCookieContext::
                                         ContextType::SAME_SITE_LAX)));
  EXPECT_EQ("samesite-unspecified=1",
            content::GetCookies(profile, legacy_allowed_domain_url));
  // Do the same on the other domain...
  EXPECT_TRUE(content::SetCookie(profile, other_domain_url,
                                 "samesite-unspecified=1",
                                 net::CookieOptions::SameSiteCookieContext(
                                     net::CookieOptions::SameSiteCookieContext::
                                         ContextType::SAME_SITE_LAX)));
  EXPECT_EQ("samesite-unspecified=1",
            content::GetCookies(profile, other_domain_url));

  // Overwrite the cookie from a cross-site context. Because we have a policy
  // that allows Legacy access for one domain but not the other, this will work
  // on the policy-specified domain even if SameSite features are enabled, but
  // it will not work for the other domain. (It works regardless, if they are
  // disabled.)
  EXPECT_TRUE(content::SetCookie(
      profile, legacy_allowed_domain_url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE)));
  EXPECT_EQ("samesite-unspecified=2",
            content::GetCookies(profile, legacy_allowed_domain_url));
  EXPECT_EQ("samesite-unspecified=2",
            content::GetCookies(profile, legacy_allowed_domain_url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));
  // When Schemeful Same-Site is enabled a context downgrade to an insufficient
  // context should still be allowed with legacy access. This'll always work if
  // Schemeful Same-Site is disabled because the schemeless context is Lax
  // which is sufficient.
  EXPECT_TRUE(content::SetCookie(
      profile, legacy_allowed_domain_url, "samesite-lax=1; SameSite=Lax",
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE)));
  // Similarly when we try to get the cookie.
  EXPECT_THAT(
      content::GetCookies(profile, legacy_allowed_domain_url,
                          net::CookieOptions::SameSiteCookieContext(
                              net::CookieOptions::SameSiteCookieContext::
                                  ContextType::SAME_SITE_LAX,
                              net::CookieOptions::SameSiteCookieContext::
                                  ContextType::CROSS_SITE)),
      testing::HasSubstr("samesite-lax=1"));

  // For the domain that is not Legacy by policy, we expect it to work only if
  // the SameSite features are disabled.
  if (AreSameSiteFeaturesEnabled()) {
    EXPECT_FALSE(
        content::SetCookie(profile, other_domain_url, "samesite-unspecified=2",
                           net::CookieOptions::SameSiteCookieContext(
                               net::CookieOptions::SameSiteCookieContext::
                                   ContextType::CROSS_SITE)));
    EXPECT_EQ("samesite-unspecified=1",
              content::GetCookies(profile, other_domain_url));
    EXPECT_EQ(
        "", content::GetCookies(profile, other_domain_url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));
    EXPECT_FALSE(content::SetCookie(
        profile, other_domain_url, "samesite-lax=1; SameSite=Lax",
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::
                SAME_SITE_LAX,
            net::CookieOptions::SameSiteCookieContext::ContextType::
                CROSS_SITE)));
    // We should be able to get the cookie which was previously added.
    EXPECT_EQ("samesite-unspecified=1",
              content::GetCookies(profile, other_domain_url));
    // But no cookies should be returned for a downgrade to an insufficient
    // context, since SameSite-by-default is active which requires a minimum of
    // a Lax context.
    EXPECT_EQ(
        "", content::GetCookies(profile, other_domain_url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::SAME_SITE_LAX,
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));
  } else {
    EXPECT_TRUE(
        content::SetCookie(profile, other_domain_url, "samesite-unspecified=2",
                           net::CookieOptions::SameSiteCookieContext(
                               net::CookieOptions::SameSiteCookieContext::
                                   ContextType::CROSS_SITE)));
    EXPECT_EQ("samesite-unspecified=2",
              content::GetCookies(profile, other_domain_url));
    EXPECT_EQ(
        "samesite-unspecified=2",
        content::GetCookies(profile, other_domain_url,
                            net::CookieOptions::SameSiteCookieContext(
                                net::CookieOptions::SameSiteCookieContext::
                                    ContextType::CROSS_SITE)));

    EXPECT_TRUE(content::SetCookie(
        profile, other_domain_url, "samesite-lax=1; SameSite=Lax",
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::
                SAME_SITE_LAX,
            net::CookieOptions::SameSiteCookieContext::ContextType::
                CROSS_SITE)));
    EXPECT_THAT(
        content::GetCookies(profile, other_domain_url,
                            net::CookieOptions::SameSiteCookieContext(
                                net::CookieOptions::SameSiteCookieContext::
                                    ContextType::SAME_SITE_LAX,
                                net::CookieOptions::SameSiteCookieContext::
                                    ContextType::CROSS_SITE)),
        testing::HasSubstr("samesite-lax=1"));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SameSiteCookiesPolicyTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_F(SchemefulSameSiteCookiesPolicyIntegrationTest,
                       AllowCrossSchemeFrameLegacyCookies) {
  PolicyMap policies;
  // Set a policy to force legacy access for all cookies.
  PolicyTest::SetPolicy(&policies,
                        policy::key::kLegacySameSiteCookieBehaviorEnabled,
                        base::Value(1));
  PolicyTest::UpdateProviderPolicy(policies);

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", false),
                     "strictcookie=1;SameSite=Strict");

  // Construct a cross-scheme same domain iframe (Main frame http://a.test,
  // iframe https://a.test).
  //
  // Start by navigating to an insecure page with an iframe.
  NavigateToHttpPageWithFrame("a.test");
  storage::test::ExpectCookiesOnHost(browser()->profile(),
                                     GetURL("a.test", false /* secure */),
                                     "strictcookie=1");

  // Then navigate the frame to a secure page and check to see if the cookie is
  // sent.
  NavigateFrameToHttps("a.test", "/echoheader?cookie");
  // The legacy cookie should have been sent.
  ExpectFrameContent(GetChildFrame(), "strictcookie=1");
}

IN_PROC_BROWSER_TEST_F(SchemefulSameSiteCookiesPolicyIntegrationTest,
                       DisallowCrossSchemeFrameNonLegacyCookies) {
  PolicyMap policies;
  // Set a policy to force non-legacy access for all cookies.
  PolicyTest::SetPolicy(&policies,
                        policy::key::kLegacySameSiteCookieBehaviorEnabled,
                        base::Value(2));
  PolicyTest::UpdateProviderPolicy(policies);

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", false),
                     "strictcookie=1;SameSite=Strict");

  // Construct a cross-scheme same domain iframe (Main frame http://a.test,
  // iframe https://a.test).
  //
  // Start by navigating to an insecure page with an iframe.
  NavigateToHttpPageWithFrame("a.test");
  storage::test::ExpectCookiesOnHost(browser()->profile(),
                                     GetURL("a.test", false /* secure */),
                                     "strictcookie=1");

  // Then navigate the frame to a secure page and check to see if the cookie is
  // sent.
  NavigateFrameToHttps("a.test", "/echoheader?cookie");
  // The non-legacy cookie should not have been sent.
  ExpectFrameContent(GetChildFrame(), "None");
}

IN_PROC_BROWSER_TEST_F(SchemefulSameSiteCookiesPolicyIntegrationTest,
                       AllowStrictOnCrossSchemeNavigation) {
  PolicyMap policies;
  // Set a policy to force legacy access for all cookies.
  PolicyTest::SetPolicy(&policies,
                        policy::key::kLegacySameSiteCookieBehaviorEnabled,
                        base::Value(1));
  PolicyTest::UpdateProviderPolicy(policies);

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", true),
                     "strictcookie=1;SameSite=Strict");

  // Just go somewhere on http://a.test. Doesn't matter where.
  NavigateToHttpPageWithFrame("a.test");

  GURL secure_echo_url = GetURL("a.test", "/echoheader?cookie", true);
  ASSERT_TRUE(NavigateToURLFromRenderer(GetMainFrame(), secure_echo_url));

  ExpectFrameContent(GetMainFrame(), "strictcookie=1");
}

IN_PROC_BROWSER_TEST_F(SchemefulSameSiteCookiesPolicyIntegrationTest,
                       DisallowStrictOnCrossSchemeNavigation) {
  PolicyMap policies;
  // Set a policy to force non-legacy access for all cookies.
  PolicyTest::SetPolicy(&policies,
                        policy::key::kLegacySameSiteCookieBehaviorEnabled,
                        base::Value(2));
  PolicyTest::UpdateProviderPolicy(policies);

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", false),
                     "strictcookie=1;SameSite=Strict");

  // Just go somewhere on http://a.test. Doesn't matter where.
  NavigateToHttpPageWithFrame("a.test");

  GURL secure_echo_url = GetURL("a.test", "/echoheader?cookie", true);
  ASSERT_TRUE(NavigateToURLFromRenderer(GetMainFrame(), secure_echo_url));

  ExpectFrameContent(GetMainFrame(), "None");
}

}  // namespace policy
