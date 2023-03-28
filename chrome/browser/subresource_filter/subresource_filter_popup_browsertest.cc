// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using safe_browsing::SubresourceFilterType;
using safe_browsing::SubresourceFilterLevel;

namespace subresource_filter {

namespace {

// Checks console messages.
void RoundTripAndVerifyLogMessages(
    const content::WebContentsConsoleObserver& observer,
    content::WebContents* web_contents,
    std::set<std::string> messages_expected,
    std::set<std::string> messages_not_expected) {
  // Round trip to the renderer to ensure the message would have gotten sent.
  EXPECT_TRUE(content::ExecJs(web_contents, "var a = 1;"));

  for (size_t i = 0u; i < observer.messages().size(); ++i) {
    std::string message = observer.GetMessageAt(i);
    if (base::Contains(messages_expected, message)) {
      messages_expected.erase(message);
      continue;
    }
    if (base::Contains(messages_not_expected, message))
      ADD_FAILURE() << "Saw anti-expected message: " << message;
  }
  EXPECT_THAT(messages_expected, ::testing::IsEmpty())
      << "Missing expected messages.";
}

}  // namespace

// Tests that subresource_filter interacts well with the abusive enforcement in
// chrome/browser/ui/blocked_content/safe_browsing_triggered_popup_blocker.
class SubresourceFilterPopupBrowserTest
    : public SubresourceFilterListInsertingBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                         subresource_filter::ActivationScope::ACTIVATION_LIST,
                         subresource_filter::ActivationList::BETTER_ADS);
    ResetConfiguration(std::move(config));
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  }
  void ConfigureAsAbusiveAndBetterAds(const GURL& url,
                                      SubresourceFilterLevel abusive_level,
                                      SubresourceFilterLevel bas_level) {
    safe_browsing::ThreatMetadata metadata;
    metadata.subresource_filter_match[SubresourceFilterType::ABUSIVE] =
        abusive_level;
    metadata.subresource_filter_match[SubresourceFilterType::BETTER_ADS] =
        bas_level;

    database_helper()->AddFullHashToDbAndFullHashCache(
        url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
  }

  bool AreDisallowedRequestsBlocked() {
    std::string script = base::StringPrintf(
        R"(
      var script = document.createElement('script');
      script.src = '%s';
      script.type = 'text/javascript';
      new Promise(resolve => {
        script.onload = () => { resolve(true); }
        script.onerror = () => { resolve(false); }
        document.head.appendChild(script);
      });
    )",
        embedded_test_server()
            ->GetURL("/subresource_filter/included_script.js")
            .spec()
            .c_str());
    return !content::EvalJs(web_contents(), script).ExtractBool();
  }
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       NoConfiguration_AllowCreatingNewWindows) {
  ResetConfiguration(Configuration::MakePresetForLiveRunOnPhishingSites());
  base::HistogramTester tester;
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  // Only configure |a_url| as a phishing URL.
  ConfigureAsPhishingURL(a_url);

  // Navigate to a_url, should not trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(web_contents, "openWindow()"));
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Navigate again to trigger histogram logging. Make sure the navigation
  // happens in the original WebContents.
  browser()->tab_strip_model()->ToggleSelectionAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(web_contents));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
}

class SubresourceFilterPopupBrowserTestWithParam
    : public SubresourceFilterPopupBrowserTest,
      public ::testing::WithParamInterface<
          bool /* enable_adblock_on_abusive_sites */> {
 public:
  SubresourceFilterPopupBrowserTestWithParam() {
    const bool enable_adblock_on_abusive_sites = GetParam();
    feature_list_.InitWithFeatureState(
        subresource_filter::kFilterAdsOnAbusiveSites,
        enable_adblock_on_abusive_sites);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SubresourceFilterPopupBrowserTestWithParam,
                       BlockCreatingNewWindows) {
  base::HistogramTester tester;
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  GURL b_url(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  // Configure as abusive and BAS warn.
  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::ENFORCE /* abusive_level */,
      SubresourceFilterLevel::WARN /* bas_level */);

  // Navigate to a_url, should trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(false, content::EvalJs(web_contents, "openWindow()"));
  tester.ExpectTotalCount(kSubresourceFilterActionsHistogram, 0);
  // Make sure the popup UI was shown.
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Block again.
  EXPECT_EQ(false, content::EvalJs(web_contents, "openWindow()"));

  const bool enable_adblock_on_abusive_sites = GetParam();
  EXPECT_EQ(enable_adblock_on_abusive_sites, AreDisallowedRequestsBlocked());

  // Navigate to |b_url|, which should successfully open the popup.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));
  EXPECT_EQ(true, content::EvalJs(web_contents, "openWindow()"));
  // Popup UI should not be shown.
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       BlockCreatingNewWindows_LogsToConsole) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(blocked_content::kAbusiveEnforceMessage);
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::ENFORCE /* abusive_level */,
      SubresourceFilterLevel::ENFORCE /* bas_level */);

  // Navigate to a_url, should trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  EXPECT_EQ(false, content::EvalJs(web_contents(), "openWindow()"));
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(blocked_content::kAbusiveEnforceMessage,
            console_observer.GetMessageAt(0u));

  EXPECT_TRUE(AreDisallowedRequestsBlocked());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       WarningDoNotBlockCreatingNewWindows_LogsToConsole) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));

  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::WARN /* abusive_level */,
      SubresourceFilterLevel::WARN /* bas_level */);

  content::WebContentsConsoleObserver console_observer(web_contents());

  // Navigate to a_url, should log a warning and not trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));

  RoundTripAndVerifyLogMessages(
      console_observer, web_contents(),
      {kActivationWarningConsoleMessage, blocked_content::kAbusiveWarnMessage},
      {});

  EXPECT_EQ(true, content::EvalJs(web_contents(), "openWindow()"));
  EXPECT_FALSE(AreDisallowedRequestsBlocked());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       WarnAbusiveAndBetterAds_LogsToConsole) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::WARN /* abusive_level */,
      SubresourceFilterLevel::WARN /* bas_level */);

  // Allow popups on |a_url|.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      a_url, a_url, ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  content::WebContentsConsoleObserver console_observer(web_contents());

  // Navigate to a_url, should not trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  EXPECT_EQ(true, content::EvalJs(web_contents(), "openWindow()"));

  EXPECT_FALSE(AreDisallowedRequestsBlocked());

  RoundTripAndVerifyLogMessages(
      console_observer, web_contents(),
      {blocked_content::kAbusiveWarnMessage, kActivationWarningConsoleMessage},
      {});
}

// Allowlisted sites should not have console logging.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       AllowCreatingNewWindows_NoLogToConsole) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  // Enforce BAS, warn abusive.
  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::WARN /* abusive_level */,
      SubresourceFilterLevel::ENFORCE /* bas_level */);

  // Allow popups on |a_url|.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      a_url, a_url, ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  content::WebContentsConsoleObserver console_observer(web_contents());

  // Navigate to a_url, should not trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  EXPECT_TRUE(AreDisallowedRequestsBlocked());

  EXPECT_EQ(true, content::EvalJs(web_contents(), "openWindow()"));

  // On the new window, requests should be blocked due to the popup inheriting
  // the activation state.
  EXPECT_TRUE(AreDisallowedRequestsBlocked());

  RoundTripAndVerifyLogMessages(console_observer, web_contents(),
                                {kActivationConsoleMessage},
                                {blocked_content::kAbusiveEnforceMessage});
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterPopupBrowserTestWithParam,
                       BlockOpenURLFromTab) {
  base::HistogramTester tester;
  const char kWindowOpenPath[] =
      "/subresource_filter/window_open_spoof_click.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  GURL b_url(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::ENFORCE /* abusive_level */,
      SubresourceFilterLevel::WARN /* bas_level */);

  // Navigate to a_url, should trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecJs(web_contents, "openWindow()"));
  tester.ExpectTotalCount(kSubresourceFilterActionsHistogram, 0);

  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));
  const bool enable_adblock_on_abusive_sites = GetParam();
  EXPECT_EQ(enable_adblock_on_abusive_sites, AreDisallowedRequestsBlocked());

  // Navigate to |b_url|, which should successfully open the popup.

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(web_contents, "openWindow()"));
  navigation_observer.Wait();

  // Popup UI should not be shown.
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(AreDisallowedRequestsBlocked());
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterPopupBrowserTestWithParam,
                       BlockOpenURLFromTabInIframe) {
  const char popup_path[] = "/subresource_filter/iframe_spoof_click_popup.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", popup_path));
  ConfigureAsAbusiveAndBetterAds(
      a_url, SubresourceFilterLevel::ENFORCE /* abusive_level */,
      SubresourceFilterLevel::WARN /* bas_level */);

  // Navigate to a_url, should not trigger the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(web_contents, "openWindow()"));
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));
  const bool enable_adblock_on_abusive_sites = GetParam();
  EXPECT_EQ(enable_adblock_on_abusive_sites, AreDisallowedRequestsBlocked());
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterPopupBrowserTestWithParam,
                       TraditionalWindowOpen_NotBlocked) {
  GURL url(GetTestUrl("/title2.html"));
  ConfigureAsAbusiveAndBetterAds(
      url, SubresourceFilterLevel::ENFORCE /* abusive_level */,
      SubresourceFilterLevel::WARN /* bas_level */);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl("/title1.html")));

  // Should not trigger the popup blocker because internally opens the tab with
  // a user gesture.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
  const bool enable_adblock_on_abusive_sites = GetParam();
  EXPECT_EQ(enable_adblock_on_abusive_sites, AreDisallowedRequestsBlocked());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SubresourceFilterPopupBrowserTestWithParam,
                         ::testing::Values(false, true));

}  // namespace subresource_filter
