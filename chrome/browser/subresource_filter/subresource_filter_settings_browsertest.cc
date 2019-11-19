// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions2";

class SubresourceFilterSettingsBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  void SetUp() override {
    EXPECT_CALL(provider_, IsInitializationComplete(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    SubresourceFilterBrowserTest::SetUp();
  }

  void UpdatePolicy(const policy::PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

 private:
  policy::MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       ContentSettingsWhitelist_DoNotActivate) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);

  // Simulate an explicity whitelisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::ADS, std::string(), CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // No message for whitelisted url.
  EXPECT_TRUE(console_observer.message().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       ContentSettingsWhitelistGlobal_DoNotActivate) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);

  // Simulate globally allowing ads via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::ADS,
                                         CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // No message for loads that are not activated.
  EXPECT_TRUE(console_observer.message().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       DrivenByEnterprisePolicy) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);

  // Disable Ads blocking via enterprise policy.
  policy::PolicyMap policy;
  policy.Set(policy::key::kAdsSettingForIntrusiveAdsSites,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             std::make_unique<base::Value>(CONTENT_SETTING_ALLOW), nullptr);
  UpdatePolicy(policy);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // No message for whitelisted url.
  EXPECT_TRUE(console_observer.message().empty());

  // Since the policy change can take effect without browser restart, verify
  // that blocking ads via policy here should start blocking ads.
  policy.Set(policy::key::kAdsSettingForIntrusiveAdsSites,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             std::make_unique<base::Value>(CONTENT_SETTING_BLOCK), nullptr);
  UpdatePolicy(policy);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       ContentSettingsAllowWithNoPageActivation_DoNotActivate) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));

  // Do not configure as phishing URL.

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // Simulate allowing the subresource filter via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::ADS, std::string(), CONTENT_SETTING_BLOCK);

  // Setting the site to "allow" should not activate filtering.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       ContentSettingsWhitelistViaReload_DoNotActivate) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // Whitelist via a reload.
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  ChromeSubresourceFilterClient::FromWebContents(web_contents())
      ->OnReloadRequested();
  navigation_observer.Wait();

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       ContentSettingsWhitelistViaReload_WhitelistIsByDomain) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // Whitelist via a reload.
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  ChromeSubresourceFilterClient::FromWebContents(web_contents())
      ->OnReloadRequested();
  navigation_observer.Wait();

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // Another navigation to the same domain should be whitelisted too.
  ui_test_utils::NavigateToURL(
      browser(),
      GetTestUrl("subresource_filter/frame_with_included_script.html?query"));
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // A cross site blacklisted navigation should stay activated, however.
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(a_url);
  ui_test_utils::NavigateToURL(browser(), a_url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

// Test the "smart" UI, aka the logic to hide the UI on subsequent same-domain
// navigations, until a certain time threshold has been reached. This is an
// android-only feature.
IN_PROC_BROWSER_TEST_F(SubresourceFilterSettingsBrowserTest,
                       DoNotShowUIUntilThresholdReached) {
  settings_manager()->set_should_use_smart_ui_for_testing(true);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_with_included_script.html"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  // Test utils only support one blacklisted site at a time.
  // TODO(csharrison): Add support for more than one URL.
  ConfigureAsPhishingURL(a_url);

  ChromeSubresourceFilterClient* client =
      ChromeSubresourceFilterClient::FromWebContents(web_contents());
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  base::SimpleTestClock* raw_clock = test_clock.get();
  settings_manager()->set_clock_for_testing(std::move(test_clock));

  base::HistogramTester histogram_tester;

  // First load should trigger the UI.
  ui_test_utils::NavigateToURL(browser(), a_url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_TRUE(client->did_show_ui_for_navigation());

  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUISuppressed, 0);

  // Second load should not trigger the UI, but should still filter content.
  ui_test_utils::NavigateToURL(browser(), a_url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_EQ(client->did_show_ui_for_navigation(), false);

  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUISuppressed, 1);

  ConfigureAsPhishingURL(b_url);

  // Load to another domain should trigger the UI.
  ui_test_utils::NavigateToURL(browser(), b_url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_TRUE(client->did_show_ui_for_navigation());

  ConfigureAsPhishingURL(a_url);

  // Fast forward the clock, and a_url should trigger the UI again.
  raw_clock->Advance(
      SubresourceFilterContentSettingsManager::kDelayBeforeShowingInfobarAgain);
  ui_test_utils::NavigateToURL(browser(), a_url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_TRUE(client->did_show_ui_for_navigation());

  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUISuppressed, 1);
}

}  // namespace subresource_filter
