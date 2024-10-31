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
