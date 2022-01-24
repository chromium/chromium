// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

class MockPrivacySandboxObserver : public PrivacySandboxSettings::Observer {
 public:
  MOCK_METHOD1(OnFlocDataAccessibleSinceUpdated, void(bool));
};

}  // namespace

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

  PrivacySandboxSettings* privacy_sandbox_settings() {
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
// an update to floc-data-accessible-since and invoke the corresponding observer
// method.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest, ClearAllCookies) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());

  MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(false));

  ClearAllCookies();

  EXPECT_NE(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}

// Test that cookie clearings triggered by Clear-Site-Data header won't trigger
// an update to floc-data-accessible-since or invoke the corresponding observer
// method.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest,
                       ClearSiteDataCookies) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());

  MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(testing::_)).Times(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("a.test", "/clear_site_data_header_cookies")));

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}

// Check that the observer is called appropriately in response to a user
// resetting the floc id.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest, UserResetFlocID) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());

  MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(true)).Times(2);

  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));

  privacy_sandbox_settings()->ResetFlocId(/*user_initiated=*/true);

  EXPECT_NE(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));

  privacy_sandbox_settings()->ResetFlocId(/*user_initiated=*/false);
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));
}

class PrivacySandboxSettingsBrowserPolicyTest
    : public PrivacySandboxSettingsBrowserTest {
 public:
  PrivacySandboxSettingsBrowserPolicyTest() {
    policy_provider()->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        policy_provider());

    policy::PolicyMap third_party_cookies_blocked_policy;
    third_party_cookies_blocked_policy.Set(
        policy::key::kBlockThirdPartyCookies, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(true),
        /*external_data_fetcher=*/nullptr);
    policy_provider()->UpdateChromePolicy(third_party_cookies_blocked_policy);
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Reconciliation should not run while 3P or all cookies are disabled by
// policy, but should run if the policy is changed or removed.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserPolicyTest,
                       DelayedReconciliationCookieSettingsManaged) {
  privacy_sandbox_settings();

  // Policies set in the test constructor should have prevented reconciliation
  // from running immediately.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Check that applying a different policy which also results in 3P cookies
  // being blocked does not result in reconciliation running.
  policy::PolicyMap all_cookies_blocked_policy;
  all_cookies_blocked_policy.Set(
      policy::key::kDefaultCookiesSetting, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(CONTENT_SETTING_BLOCK),
      /*external_data_fetcher=*/nullptr);
  policy_provider()->UpdateChromePolicy(all_cookies_blocked_policy);
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Apply policy which allows third party cookies and ensure that
  // reconciliation runs.
  policy::PolicyMap third_party_cookies_allowed_policy;
  third_party_cookies_allowed_policy.Set(
      policy::key::kBlockThirdPartyCookies, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(false),
      /*external_data_fetcher=*/nullptr);
  policy_provider()->UpdateChromePolicy(third_party_cookies_allowed_policy);
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}
