// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
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
// Schemeful Same-Site to test the Legacy SameSite cookie policy, which controls
// behavior of SameSite-by-default, Cookies-without-SameSite-must-be-Secure, and
// Schemeful Same-Site.
class SameSiteCookiesPolicyTest : public PolicyTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  SameSiteCookiesPolicyTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (IsSchemefulSameSiteEnabled()) {
      // No need to explicitly enable, since it's enabled by default.
      feature_list_.Init();
    } else {
      feature_list_.InitAndDisableFeature(net::features::kSchemefulSameSite);
    }
  }

  ~SameSiteCookiesPolicyTest() override = default;

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(http_server_.Start());
  }

  GURL GetURL(const std::string& host, const std::string& path, bool secure) {
    if (secure) {
      return https_server_.GetURL(host, path);
    }

    return http_server_.GetURL(host, path);
  }

  GURL GetURL(const std::string& host, bool secure) {
    return GetURL(host, "/", secure);
  }

  content::RenderFrameHost* GetChildFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  void NavigateToHttpPageWithFrame(const std::string& host) {
    GURL main_url(http_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameToHttps(const std::string& host, const std::string& path) {
    GURL page = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  std::string GetFrameContent(content::RenderFrameHost* frame) {
    return storage::test::GetFrameContent(frame);
  }

  bool IsSchemefulSameSiteEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::test_server::EmbeddedTestServer http_server_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       AllowLegacyCookieAccessForDomain) {
  GURL legacy_allowed_domain_url(kURL);
  GURL other_domain_url("http://other-domain.example");

  // Set a policy to allow Legacy cookie access for one domain only.
  base::Value::List policy_value;
  policy_value.Append(legacy_allowed_domain_url.host());

  PolicyMap policies;
  // Set a policy to allow Legacy access for the given domain only.
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabledForDomainList,
            base::Value(std::move(policy_value)));
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

  // Setting the cookie from a cross-site context on the domain without the
  // policy does not work, because it defaults to Lax.
  EXPECT_FALSE(content::SetCookie(
      profile, other_domain_url, "samesite-unspecified=2",
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE)));
  // If we get the cookie from a same-site context, the old value is still
  // present.
  EXPECT_EQ("samesite-unspecified=1",
            content::GetCookies(profile, other_domain_url));
  // The cookie cannot be accessed at all from a cross-site context.
  EXPECT_EQ("",
            content::GetCookies(profile, other_domain_url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));

  // Setting a Lax cookie from a downgraded context only works if Schemeful
  // Same-Site is not enabled.
  EXPECT_EQ(!IsSchemefulSameSiteEnabled(),
            content::SetCookie(profile, other_domain_url,
                               "samesite-lax=1; SameSite=Lax",
                               net::CookieOptions::SameSiteCookieContext(
                                   net::CookieOptions::SameSiteCookieContext::
                                       ContextType::SAME_SITE_LAX,
                                   net::CookieOptions::SameSiteCookieContext::
                                       ContextType::CROSS_SITE)));
  // We should be able to get the cookie which was previously added.
  EXPECT_EQ(IsSchemefulSameSiteEnabled()
                ? "samesite-unspecified=1"
                : "samesite-unspecified=1; samesite-lax=1",
            content::GetCookies(profile, other_domain_url));
  // But no cookies should be returned for a downgrade to an insufficient
  // context, since SameSite-by-default is active which requires a minimum of
  // a Lax context.
  EXPECT_EQ(IsSchemefulSameSiteEnabled()
                ? ""
                : "samesite-unspecified=1; samesite-lax=1",
            content::GetCookies(profile, other_domain_url,
                                net::CookieOptions::SameSiteCookieContext(
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::SAME_SITE_LAX,
                                    net::CookieOptions::SameSiteCookieContext::
                                        ContextType::CROSS_SITE)));
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       AllowCrossSchemeFrameLegacyCookies) {
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL("a.test", true), CONTENT_SETTING_ALLOW);
  PolicyMap policies;
  // Set a policy to force legacy access for our cookies.
  base::Value::List policy_value;
  policy_value.Append(GURL("http://a.test").host());
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabledForDomainList,
            base::Value(std::move(policy_value)));
  PolicyTest::UpdateProviderPolicy(policies);

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", false),
                     "strictcookie=1;SameSite=Strict");

  // Construct a cross-scheme same domain iframe (Main frame http://a.test,
  // iframe https://a.test).
  //
  // Start by navigating to an insecure page with an iframe.
  NavigateToHttpPageWithFrame("a.test");
  EXPECT_EQ(content::GetCookies(browser()->profile(),
                                GetURL("a.test", false /* secure */)),
            "strictcookie=1");

  // Then navigate the frame to a secure page and check to see if the cookie is
  // sent.
  NavigateFrameToHttps("a.test", "/echoheader?cookie");
  // The legacy cookie should have been sent.
  EXPECT_EQ(GetFrameContent(GetChildFrame()), "strictcookie=1");
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       DisallowCrossSchemeFrameNonLegacyCookies) {
  // Don't set a policy, this results in the cookies having behavior dependent
  // on the base::Feature state.

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", false),
                     "strictcookie=1;SameSite=Strict");

  // Construct a cross-scheme same domain iframe (Main frame http://a.test,
  // iframe https://a.test).
  //
  // Start by navigating to an insecure page with an iframe. The cookie will
  // always be present because it is a same-schemeful-site context.
  NavigateToHttpPageWithFrame("a.test");
  EXPECT_EQ(content::GetCookies(browser()->profile(),
                                GetURL("a.test", false /* secure */)),
            "strictcookie=1");

  // Then navigate the frame to a secure page and check to see if the cookie is
  // sent.
  NavigateFrameToHttps("a.test", "/echoheader?cookie");
  // The cookie will be sent only if Schemeful Same-Site is not active.
  EXPECT_EQ(GetFrameContent(GetChildFrame()),
            IsSchemefulSameSiteEnabled() ? "None" : "strictcookie=1");
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       AllowStrictOnCrossSchemeNavigation) {
  PolicyMap policies;
  // Set a policy to force legacy access for our cookies.
  base::Value::List policy_value;
  policy_value.Append(GURL("http://a.test").host());
  SetPolicy(&policies, key::kLegacySameSiteCookieBehaviorEnabledForDomainList,
            base::Value(std::move(policy_value)));
  PolicyTest::UpdateProviderPolicy(policies);

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", true),
                     "strictcookie=1;SameSite=Strict");

  // Just go somewhere on http://a.test. Doesn't matter where.
  NavigateToHttpPageWithFrame("a.test");

  GURL secure_echo_url = GetURL("a.test", "/echoheader?cookie", true);
  ASSERT_TRUE(
      NavigateToURLFromRenderer(GetPrimaryMainFrame(), secure_echo_url));

  EXPECT_EQ(GetFrameContent(GetPrimaryMainFrame()), "strictcookie=1");
}

IN_PROC_BROWSER_TEST_P(SameSiteCookiesPolicyTest,
                       DisallowStrictOnCrossSchemeNavigation) {
  // Don't set a policy, this results in the cookies having behavior dependent
  // on the base::Feature state.

  // Set a cookie that will only be sent with legacy behavior.
  content::SetCookie(browser()->profile(), GetURL("a.test", false),
                     "strictcookie=1;SameSite=Strict");

  // Just go somewhere on http://a.test. Doesn't matter where.
  NavigateToHttpPageWithFrame("a.test");

  GURL secure_echo_url = GetURL("a.test", "/echoheader?cookie", true);
  ASSERT_TRUE(
      NavigateToURLFromRenderer(GetPrimaryMainFrame(), secure_echo_url));

  EXPECT_EQ(GetFrameContent(GetPrimaryMainFrame()),
            IsSchemefulSameSiteEnabled() ? "None" : "strictcookie=1");
}

INSTANTIATE_TEST_SUITE_P(All, SameSiteCookiesPolicyTest, ::testing::Bool());

}  // namespace policy
