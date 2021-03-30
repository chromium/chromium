// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/safe_browsing/core/db/v4_test_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

using subresource_filter::testing::TestRulesetPair;

namespace {

namespace proto = url_pattern_index::proto;

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "/subresource_filter/frame_set.html";

// Names of DocumentLoad histograms.
constexpr const char kDocumentLoadActivationLevel[] =
    "SubresourceFilter.DocumentLoad.ActivationState";

constexpr const char kSubresourceLoadsTotalForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.Total";
constexpr const char kSubresourceLoadsEvaluatedForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.Evaluated";
constexpr const char kSubresourceLoadsMatchedRulesForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules";
constexpr const char kSubresourceLoadsDisallowedForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.Disallowed";

// Names of the performance measurement histograms.
constexpr const char kEvaluationTotalWallDurationForPage[] =
    "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalWallDuration";
constexpr const char kEvaluationTotalCPUDurationForPage[] =
    "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalCPUDuration";
constexpr const char kEvaluationWallDuration[] =
    "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration";
constexpr const char kEvaluationCPUDuration[] =
    "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration";

constexpr const char kActivationDecision[] =
    "SubresourceFilter.PageLoad.ActivationDecision";

// Names of navigation chain patterns histogram.
const char kActivationListHistogram[] =
    "SubresourceFilter.PageLoad.ActivationList";

const char kPageLoadActivationStateHistogram[] =
    "SubresourceFilter.PageLoad.ActivationState";
const char kPageLoadActivationStateDidInheritHistogram[] =
    "SubresourceFilter.PageLoad.ActivationState.DidInherit";

// Other histograms.
const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions2";

GURL GetURLWithFragment(const GURL& url, base::StringPiece fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

// This string comes from GetErrorStringForDisallowedLoad() in
// blink/renderer/core/loader/subresource_filter.cc
constexpr const char kBlinkDisallowSubframeConsoleMessageFormat[] =
    "Chrome blocked resource %s on this site because this site tends to show "
    "ads that interrupt, distract, mislead, or prevent user control. Learn "
    "more at https://www.chromestatus.com/feature/5738264052891648";

}  // namespace

// Tests -----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(SubresourceFilterListInsertingBrowserTest,
                       MainFrameActivation_SubresourceFilterList) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kActivationConsoleMessage);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::SUBRESOURCE_FILTER);
  ResetConfiguration(std::move(config));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_FALSE(console_observer.messages().empty());

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterListInsertingBrowserTest,
                       MainFrameActivationWithWarning_BetterAdsList) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*show ads*");
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureURLWithWarning(url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::BETTER_ADS);
  ResetConfiguration(std::move(config));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ(kActivationWarningConsoleMessage,
            console_observer.GetMessageAt(0u));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ(kActivationWarningConsoleMessage,
            console_observer.GetMessageAt(1u));
}

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterListInsertingBrowserTest,
    ExpectRedirectPatternHistogramsAreRecordedForSubresourceFilterOnlyRedirectMatch) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  const std::string initial_host("a.com");
  const std::string redirected_host("b.com");

  GURL redirect_url(embedded_test_server()->GetURL(
      redirected_host, "/subresource_filter/frame_with_included_script.html"));
  GURL url(embedded_test_server()->GetURL(
      initial_host, "/server-redirect?" + redirect_url.spec()));

  ConfigureAsSubresourceFilterOnlyURL(url.GetOrigin());
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectUniqueSample(kActivationListHistogram,
                            static_cast<int>(ActivationList::NONE), 1);
}

// Normally, the subresource filter list is only sync'd in chrome branded
// builds.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       SubresourceFilterListNeedsBranding) {
  bool has_list = database_helper()->HasListSynced(
      safe_browsing::GetUrlSubresourceFilterId());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(has_list);
#else
  EXPECT_FALSE(has_list);
#endif
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, MainFrameActivation) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kActivationConsoleMessage);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_FALSE(console_observer.messages().empty());

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

// There should be no document-level de-/reactivation happening on the renderer
// side as a result of a same document navigation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DocumentActivationOutlivesSameDocumentNavigation) {
  GURL url(GetTestUrl("subresource_filter/frame_with_delayed_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Deactivation would already detected by the IsDynamicScriptElementLoaded
  // line alone. To ensure no reactivation, which would muddy up recorded
  // histograms, also set a ruleset that allows everything. If there was
  // reactivation, then this new ruleset would be picked up, once again causing
  // the IsDynamicScriptElementLoaded check to fail.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));
  EXPECT_FALSE(IsDynamicScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, SubFrameActivation) {
  std::string message_filter =
      base::StringPrintf(kBlinkDisallowSubframeConsoleMessageFormat, "*");
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(message_filter);

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoad{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoad));

  tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);

  // Console message for subframe blocking should be displayed.
  EXPECT_TRUE(base::MatchPattern(
      console_observer.GetMessageAt(0u),
      base::StringPrintf(kBlinkDisallowSubframeConsoleMessageFormat,
                         "*included_script.js")));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationDisabled_NoConsoleMessage) {
  std::string message_filter =
      base::StringPrintf(kBlinkDisallowSubframeConsoleMessageFormat, "*");
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(message_filter);

  Configuration config(
      subresource_filter::mojom::ActivationLevel::kDisabled,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Console message for subframe blocking should not be displayed as filtering
  // is disabled.
  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationDryRun_NoConsoleMessage) {
  std::string message_filter =
      base::StringPrintf(kBlinkDisallowSubframeConsoleMessageFormat, "*");
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(message_filter);

  Configuration config(
      subresource_filter::mojom::ActivationLevel::kDryRun,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Console message for subframe blocking should not be displayed as filtering
  // is enabled in dryrun mode.
  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       SubframeDocumentLoadFiltering) {
  base::HistogramTester histogram_tester;
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);

  // Disallow loading subframe documents that in turn would end up loading
  // included_script.js, unless the document is loaded from an allowlisted
  // domain. This enables the third part of this test disallowing a load only
  // after the first redirect.
  const char kAllowlistedDomain[] = "allowlisted.com";
  proto::UrlRule rule = testing::CreateSuffixRule("included_script.html");
  proto::UrlRule allowlist_rule = testing::CreateSuffixRule(kAllowlistedDomain);
  allowlist_rule.set_anchor_right(proto::ANCHOR_TYPE_NONE);
  allowlist_rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules({rule, allowlist_rule}));

  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that the navigation gets blocked and the frame
  // collapsed.
  GURL disallowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowlistedDomain,
      "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  ASSERT_TRUE(frame);
  EXPECT_EQ(disallowed_subdocument_url, frame->GetLastCommittedURL());
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       HistoryNavigationActivation) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kActivationConsoleMessage);
  GURL url_with_activation(GetTestUrl(kTestFrameSetPath));
  GURL url_without_activation(
      embedded_test_server()->GetURL("a.com", kTestFrameSetPath));
  ConfigureAsPhishingURL(url_with_activation);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoadWithoutActivation{
      true, true, true};
  const std::vector<bool> kExpectScriptInFrameToLoadWithActivation{false, true,
                                                                   false};

  ui_test_utils::NavigateToURL(browser(), url_without_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  // No message should be displayed for navigating to URL without activation.
  EXPECT_TRUE(console_observer.messages().empty());

  ui_test_utils::NavigateToURL(browser(), url_with_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));

  // Console message should now be displayed.
  EXPECT_EQ(1u, console_observer.messages().size());

  ASSERT_TRUE(web_contents()->GetController().CanGoBack());
  content::WindowedNotificationObserver back_navigation_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  web_contents()->GetController().GoBack();
  back_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  ASSERT_TRUE(web_contents()->GetController().CanGoForward());
  content::WindowedNotificationObserver forward_navigation_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  web_contents()->GetController().GoForward();
  forward_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FailedProvisionalLoadInMainframe) {
  GURL url_with_activation_but_dns_error(
      "http://host-with-dns-lookup-failure/");
  GURL url_with_activation_but_not_existent(GetTestUrl("non-existent.html"));
  GURL url_without_activation(GetTestUrl(kTestFrameSetPath));

  ConfigureAsPhishingURL(url_with_activation_but_dns_error);
  ConfigureAsPhishingURL(url_with_activation_but_not_existent);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoad{true, true, true};

  for (const auto& url_with_activation :
       {url_with_activation_but_dns_error,
        url_with_activation_but_not_existent}) {
    SCOPED_TRACE(url_with_activation);

    ui_test_utils::NavigateToURL(browser(), url_with_activation);
    ui_test_utils::NavigateToURL(browser(), url_without_activation);
    ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
        kSubframeNames, kExpectScriptInFrameToLoad));
  }
}

// The page-level activation state on the browser-side should not be reset when
// a same document navigation starts in the main frame. Verify this by
// dynamically inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSameDocumentNavigation) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kActivationConsoleMessage);
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("one");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));

  EXPECT_EQ(1u, console_observer.messages().size());
}

// If a navigation starts but aborts before commit, page level activation should
// remain unchanged.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesAbortedNavigation) {
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("one");
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  // Start a new navigation, but abort it right away.
  GURL aborted_url = GURL("https://abort-me.com");
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), aborted_url);

  NavigateParams params(browser(), aborted_url, ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
  ASSERT_TRUE(manager.WaitForRequestStart());
  browser()->tab_strip_model()->GetActiveWebContents()->Stop();

  // Will return false if the navigation was successfully aborted.
  ASSERT_FALSE(manager.WaitForResponse());
  manager.WaitForNavigationFinished();

  // Now, dynamically insert a frame and expect that it is still activated.
  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, DynamicFrame) {
  GURL url(GetTestUrl("subresource_filter/frame_set.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PRE_MainFrameActivationOnStartup) {
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MainFrameActivationOnStartup) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  // Verify that the ruleset persisted in the previous session is used for this
  // page load right after start-up.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PromptShownAgainOnNextNavigation) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl(kTestFrameSetPath));
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  // Check that the bubble is not shown again for this navigation.
  EXPECT_FALSE(IsDynamicScriptElementLoaded(FindFrameByName("five")));
  tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  // Check that bubble is shown for new navigation. Must be cross site to avoid
  // triggering smart UI on Android.
  ConfigureAsPhishingURL(a_url);
  ui_test_utils::NavigateToURL(browser(), a_url);
  tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 2);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       CrossSiteSubFrameActivationWithoutAllowlist) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsPhishingURL(a_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "c", "d"}, {false, false, false});
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       CrossSiteSubFrameActivationWithAllowlist) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsPhishingURL(a_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("included_script.js"),
                           testing::CreateAllowlistRuleForDocument("c.com")}));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {false, true});
}

// Disable the test as it's flaky on Win7 dbg.
// crbug.com/1068185
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_RendererDebugURL_NoLeakedThrottlePtrs \
  DISABLED_RendererDebugURL_NoLeakedThrottlePtrs
#else
#define MAYBE_RendererDebugURL_NoLeakedThrottlePtrs \
  RendererDebugURL_NoLeakedThrottlePtrs
#endif

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MAYBE_RendererDebugURL_NoLeakedThrottlePtrs) {
  // Allow crashes caused by the navigation to kChromeUICrashURL below.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes(
      browser()->tab_strip_model()->GetActiveWebContents());

  // We have checks in the throttle manager that we don't improperly leak
  // activation state throttles. It would be nice to test things directly but it
  // isn't very feasible right now without exposing a bunch of internal guts of
  // the throttle manager.
  //
  // This test should crash the *browser process* with CHECK failures if the
  // component is faulty. The CHECK assumes that the crash URL and other
  // renderer debug URLs do not create a navigation throttle. See
  // crbug.com/736658.
  content::RenderProcessHostWatcher crash_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  browser()->OpenURL(content::OpenURLParams(
      GURL(content::kChromeUICrashURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  crash_observer.Wait();
}

// Test that resources in frames with an aborted initial load due to a doc.write
// are still disallowed.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameWithDocWriteAbortedLoad_ResourceStillDisallowed) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/subresource_filter/docwrite_loads_disallowed_resource.html"));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
}

// Test that resources in frames with an aborted initial load due to a
// window.stop are still disallowed.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameWithWindowStopAbortedLoad_ResourceStillDisallowed) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/subresource_filter/window_stop_loads_disallowed_resource.html"));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
}

// Test that a frame with an aborted initial load due to a frame deletion does
// not cause a crash.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameDeletedDuringLoad_DoesNotCrash) {
  // Watches for title set by end of frame deletion script.
  content::TitleWatcher title_watcher(web_contents(), u"done");
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/subresource_filter/delete_loading_frame.html"));

  // Wait for the script to complete.
  EXPECT_EQ(u"done", title_watcher.WaitAndGetTitle());
}

// Test that an allowed resource in the child of a frame with its initial load
// aborted due to a doc.write is not blocked.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    ChildOfFrameWithAbortedLoadLoadsAllowedResource_ResourceLoaded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource.
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/subresource_filter/"
                                     "docwrite_creates_subframe.html"));

  content::RenderFrameHost* frame = FindFrameByName("grandchild");

  EXPECT_TRUE(ExecJs(frame, R"SCRIPT(
      let image = document.createElement('img');
      image.src = 'pixel.png';
      image.onload = function() {
        top.document.title='loaded';
      };
      image.onerror = function() {
        top.document.title='failed';
      };
      document.body.appendChild(image);
  )SCRIPT"));

  // Check the load wasn't blocked.
  EXPECT_EQ(u"loaded", title_watcher.WaitAndGetTitle());
}

// Test that a disallowed resource in the child of a frame with its initial load
// aborted due to a doc.write is blocked.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    ChildOfFrameWithAbortedLoadLoadsDisallowedResource_ResourceBlocked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource.
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/subresource_filter/"
                                     "docwrite_creates_subframe.html"));

  content::RenderFrameHost* frame = FindFrameByName("grandchild");

  EXPECT_TRUE(ExecJs(frame, R"SCRIPT(
      let image = document.createElement('img');
      image.src = 'pixel.png?ad=true';
      image.onload = function() {
        top.document.title='loaded';
      };
      image.onerror = function() {
        top.document.title='failed';
      };
      document.body.appendChild(image);
  )SCRIPT"));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PopupsInheritActivation_ResourcesBlocked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  const std::vector<std::string> test_case_scripts = {
      // Popup to URL
      "window.open('/subresource_filter/popup.html');",

      // Popup to empty URL
      "popupLoadsDisallowedResource('');",

      // Child of popup to empty URL
      "popupLoadsDisallowedResourceAsDescendant('');",

      // Popup to about:blank URL. about:blank popups behave differently to
      // popups with an empty URL, so we test them separately.
      "popupLoadsDisallowedResource('about:blank');",

      // Child of popup to about:blank URL
      "popupLoadsDisallowedResourceAsDescendant('about:blank');",

      // Popup with doc.write-aborted load
      "popupLoadsDisallowedResource('http://b.com/slow?100');",

      // TODO(alexmt): Enable this test case. Currently disabled as there is no
      // guarantee that the descendant's navigation starts after the parent's
      // navigation ends (see crbug.com/1101569).
      // Child of popup with doc.write-aborted load
      // "popupLoadsDisallowedResourceAsDescendant('http://b.com/slow?100');",

  };

  for (const auto& test_case_script : test_case_scripts) {
    content::WebContentsAddedObserver popup_observer;
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/subresource_filter/popup_disallowed_load_helper.html"));
    ASSERT_TRUE(ExecJs(web_contents(), test_case_script));
    content::TitleWatcher title_watcher(popup_observer.GetWebContents(),
                                        u"failed");
    title_watcher.AlsoWaitForTitle(u"loaded");

    // Check the load was blocked.
    EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PopupNavigatesBackToAboutBlank_FilterChecked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  content::WebContentsAddedObserver popup_observer;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  base::HistogramTester tester;
  ASSERT_TRUE(ExecJs(web_contents(),
                     content::JsReplace("popup = window.open($1, 'name1');",
                                        embedded_test_server()->GetURL(
                                            "b.com", "/title2.html"))));

  {
    content::TitleWatcher title_watcher(popup_observer.GetWebContents(),
                                        u"Title Of Awesomeness");
    // Wait for popup to finish loading
    EXPECT_EQ(u"Title Of Awesomeness", title_watcher.WaitAndGetTitle());
  }

  // Check histograms agree that activation was not inherited.
  tester.ExpectBucketCount(kPageLoadActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
  tester.ExpectTotalCount(kPageLoadActivationStateDidInheritHistogram, 0);

  ASSERT_TRUE(
      ExecJs(web_contents(), "popup = window.open('about:blank', 'name1');"));

  ASSERT_TRUE(ExecJs(web_contents(), R"SCRIPT(
    // Get reference to popup without changing its location.
    popup = window.open('', 'name1');
    doc = popup.document;
    doc.open();
    doc.write(
      "<html><body>Rewritten. <img src='/ad_tagging/pixel.png?ad=true' " +
      "onload='window.document.title = \"loaded\";' " +
      "onerror='window.document.title = \"failed\";'></body></html>");
    doc.close();
    )SCRIPT"));

  content::TitleWatcher title_watcher(popup_observer.GetWebContents(),
                                      u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());

  // Check the new histograms agree that activation was inherited.
  tester.ExpectBucketCount(kPageLoadActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           2);
  tester.ExpectBucketCount(kPageLoadActivationStateDidInheritHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
}

// Test that resources in a popup with an aborted initial load due to a
// doc.write are still blocked when disallowed, even if the opener is
// immediately closed after writing.
// TODO(alexmt): Fix test flakiness and then reenable.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    DISABLED_PopupWithDocWriteAbortedLoadAndOpenerClosed_FilterChecked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad_script.js"),
                           testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  content::WebContents* original_web_contents = web_contents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  base::HistogramTester tester;
  content::WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(original_web_contents, R"SCRIPT(
    popup = window.open('http://b.com/slow?100');
    window.onunload = function(e){
      doc = popup.document;
      doc.open();
      doc.write(
        "<html><body>Rewritten. <img src='/ad_tagging/pixel.png?ad=true' " +
        "onload='window.document.title = \"loaded\";' " +
        "onerror='window.document.title = \"failed\";'></body></html>");
      doc.close();
    };
    )SCRIPT"));
  original_web_contents->ClosePage();

  content::TitleWatcher title_watcher(popup_observer.GetWebContents(),
                                      u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());

  // Check histograms agree that activation was inherited.
  tester.ExpectBucketCount(kPageLoadActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
  tester.ExpectBucketCount(kPageLoadActivationStateDidInheritHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
}

// Tests checking how histograms are recorded. ---------------------------------

namespace {

void ExpectHistogramsAreRecordedForTestFrameSet(
    const base::HistogramTester& tester,
    bool expect_performance_measurements) {
  const bool time_recorded =
      expect_performance_measurements && ScopedThreadTimers::IsSupported();

  // The following histograms are generated on the browser side.
  tester.ExpectUniqueSample(kSubresourceLoadsTotalForPage, 6, 1);
  tester.ExpectUniqueSample(kSubresourceLoadsEvaluatedForPage, 6, 1);
  tester.ExpectUniqueSample(kSubresourceLoadsMatchedRulesForPage, 4, 1);
  tester.ExpectUniqueSample(kSubresourceLoadsDisallowedForPage, 4, 1);
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, time_recorded);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, time_recorded);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // 5 subframes, each with an include.js, plus a top level include.js.
  int num_subresource_checks = 5 + 5 + 1;
  tester.ExpectTotalCount(kEvaluationWallDuration,
                          time_recorded ? num_subresource_checks : 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration,
                          time_recorded ? num_subresource_checks : 0);

  tester.ExpectUniqueSample(
      kDocumentLoadActivationLevel,
      static_cast<base::Histogram::Sample>(mojom::ActivationLevel::kEnabled),
      6);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ExpectPerformanceHistogramsAreRecorded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ResetConfigurationToEnableOnPhishingSites(true /* measure_performance */);
  const GURL url = GetTestUrl(kTestFrameSetPath);
  ConfigureAsPhishingURL(url);

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  ExpectHistogramsAreRecordedForTestFrameSet(
      tester, true /* expect_performance_measurements */);
}

class SubresourceFilterBrowserTestWithoutAdTagging
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterBrowserTestWithoutAdTagging() {
    feature_list_.InitAndDisableFeature(kAdTagging);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test only makes sense when AdTagging is disabled.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTestWithoutAdTagging,
                       ExpectHistogramsNotRecordedWhenFilteringNotActivated) {
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ResetConfigurationToEnableOnPhishingSites(true /* measure_performance */);

  const GURL url = GetTestUrl(kTestFrameSetPath);
  // Note: The |url| is not configured to be fishing.

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // The following histograms are generated only when filtering is activated.
  tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 0);
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, 0);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, 0);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // But they still should not be recorded as the filtering is not activated.
  tester.ExpectTotalCount(kEvaluationWallDuration, 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration, 0);

  // Although SubresourceFilterAgents still record the activation decision.
  tester.ExpectUniqueSample(
      kDocumentLoadActivationLevel,
      static_cast<base::Histogram::Sample>(mojom::ActivationLevel::kDisabled),
      6);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationEnabledOnReload) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  tester.ExpectTotalCount(kActivationDecision, 2);
  tester.ExpectBucketCount(kActivationDecision,
                           static_cast<int>(ActivationDecision::ACTIVATED), 2);
}

// If no ruleset is available, the VerifiedRulesetDealer considers it a
// "invalid" or "corrupt" case, and any VerifiedRuleset::Handle's vended from it
// will be useless for their entire lifetime.
//
// At first glance, this will be a problem, since the throttle manager attempts
// to keep its handle in scope for as long as possible (to avoid un-mapping and
// re-mapping the underlying file).
//
// However, in reality the throttle manager is robust to this. After every
// navigation we destroy the handle if it is "no longer in use". Since a corrupt
// or invalid ruleset will never be "in use" (i.e. activate any frame), we
// destroy the handle after every navigation / frame destruction.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       NewRulesetSameTab_ActivatesSuccessfully) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsPhishingURL(a_url);
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {true, true});

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {false, false});
}

}  // namespace subresource_filter
