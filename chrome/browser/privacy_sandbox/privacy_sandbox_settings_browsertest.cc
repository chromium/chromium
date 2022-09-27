// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

class PrivacySandboxSettingsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_.RegisterRequestHandler(
        base::BindRepeating(&PrivacySandboxSettingsBrowserTest::HandleRequest,
                            base::Unretained(this)));

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const GURL& url = request.GetURL();

    if (url.path() == "/clear_site_data_header_cookies") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->AddCustomHeader("Clear-Site-Data", "\"cookies\"");
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(std::string());
      return std::move(response);
    }

    // Use the default handler for unrelated requests.
    return nullptr;
  }

  void ClearAllCookies() {
    content::BrowsingDataRemover* remover =
        browser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_COOKIES,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
    observer.BlockUntilCompletion();
  }

  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

// Test that cookie clearings triggered by "Clear browsing data" will trigger
// an update to topics-data-accessible-since and invoke the corresponding
// observer method.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest, ClearAllCookies) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTopicsDataAccessibleSinceUpdated());

  ClearAllCookies();

  EXPECT_NE(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

// Test that cookie clearings triggered by Clear-Site-Data header won't trigger
// an update to topics-data-accessible-since or invoke the corresponding
// observer method.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest,
                       ClearSiteDataCookies) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTopicsDataAccessibleSinceUpdated()).Times(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("a.test", "/clear_site_data_header_cookies")));

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest,
                       SettingsAreNotOverridden) {
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting));
}

class PrivacySandboxSettingsAdsApisFlagBrowserTest
    : public PrivacySandboxSettingsBrowserTest {
 public:
  PrivacySandboxSettingsAdsApisFlagBrowserTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePrivacySandboxAdsApis);
  }
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsAdsApisFlagBrowserTest,
                       FollowsOverrideBehavior) {
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // The flag should enable this feature.
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting));
}
