// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
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
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
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
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
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
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "url/gurl.h"

namespace subresource_filter {

using subresource_filter::testing::TestRulesetPair;

namespace {

namespace proto = url_pattern_index::proto;

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "/subresource_filter/frame_set.html";

GURL GetURLWithFragment(const GURL& url, std::string_view fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

// This string comes from GetErrorStringForDisallowedLoad() in
// blink/renderer/core/loader/subresource_filter.cc
constexpr const char kBlinkDisallowChildFrameConsoleMessageFormat[] =
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  EXPECT_FALSE(console_observer.messages().empty());

  // The root frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ(kActivationWarningConsoleMessage,
            console_observer.GetMessageAt(0u));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

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

  ConfigureAsSubresourceFilterOnlyURL(url.DeprecatedGetOriginAsURL());
  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  EXPECT_FALSE(console_observer.messages().empty());

  // The root frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
}

// There should be no document-level de-/reactivation happening on the renderer
// side as a result of a same document navigation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DocumentActivationOutlivesSameDocumentNavigation) {
  GURL url(GetTestUrl("subresource_filter/frame_with_delayed_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Deactivation would already detected by the IsDynamicScriptElementLoaded
  // line alone. To ensure no reactivation, which would muddy up recorded
  // histograms, also set a ruleset that allows everything. If there was
  // reactivation, then this new ruleset would be picked up, once again causing
  // the IsDynamicScriptElementLoaded check to fail.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));
  EXPECT_FALSE(
      IsDynamicScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, SubFrameActivation) {
  std::string message_filter =
      base::StringPrintf(kBlinkDisallowChildFrameConsoleMessageFormat, "*");
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(message_filter);

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoad{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoad));

  tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);

  // Console message for child frame blocking should be displayed.
  EXPECT_TRUE(base::MatchPattern(
      console_observer.GetMessageAt(0u),
      base::StringPrintf(kBlinkDisallowChildFrameConsoleMessageFormat,
                         "*included_script.js")));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationDisabled_NoConsoleMessage) {
  std::string message_filter =
      base::StringPrintf(kBlinkDisallowChildFrameConsoleMessageFormat, "*");
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Console message for child frame blocking should not be displayed as
  // filtering is disabled.
  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationDryRun_NoConsoleMessage) {
  std::string message_filter =
      base::StringPrintf(kBlinkDisallowChildFrameConsoleMessageFormat, "*");
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Console message for child frame blocking should not be displayed as
  // filtering is enabled in dryrun mode.
  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       SubframeDocumentLoadFiltering) {
  base::HistogramTester histogram_tester;
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);

  // Disallow loading child frame documents that in turn would end up loading
  // included_script.js, unless the document is loaded from an allowlisted
  // domain. This enables the third part of this test disallowing a load only
  // after the first redirect.
  const char kAllowlistedDomain[] = "allowlisted.com";
  proto::UrlRule rule = testing::CreateSuffixRule("included_script.html");
  proto::UrlRule allowlist_rule = testing::CreateSuffixRule(kAllowlistedDomain);
  allowlist_rule.set_anchor_right(proto::ANCHOR_TYPE_NONE);
  allowlist_rule.set_semantics(proto::RULE_SEMANTICS_ALLOWLIST);
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules({rule, allowlist_rule}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_without_activation));
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  // No message should be displayed for navigating to URL without activation.
  EXPECT_TRUE(console_observer.messages().empty());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_activation));
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));

  // Console message should now be displayed.
  EXPECT_EQ(1u, console_observer.messages().size());

  ASSERT_TRUE(web_contents()->GetController().CanGoBack());
  content::LoadStopObserver back_navigation_stop_observer(web_contents());
  web_contents()->GetController().GoBack();
  back_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  ASSERT_TRUE(web_contents()->GetController().CanGoForward());
  content::LoadStopObserver forward_navigation_stop_observer(web_contents());
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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_activation));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), url_without_activation));
    ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
        kSubframeNames, kExpectScriptInFrameToLoad));
  }
}

// The page-level activation state on the browser-side should not be reset when
// a same document navigation starts in the root frame. Verify this by
// dynamically inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSameDocumentNavigation) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kActivationConsoleMessage);
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  ASSERT_TRUE(manager.WaitForNavigationFinished());

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {false, true});
}

// Disable the test as it's flaky on Win7 dbg.
// crbug.com/1068185
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
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
  browser()->OpenURL(content::OpenURLParams(GURL(blink::kChromeUICrashURL),
                                            content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/subresource_filter/docwrite_loads_disallowed_resource.html")));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/subresource_filter/window_stop_loads_disallowed_resource.html")));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
}

// Test that a frame with an aborted initial load due to a frame deletion does
// not cause a crash.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameDeletedDuringLoad_DoesNotCrash) {
  // Watches for title set by end of frame deletion script.
  content::TitleWatcher title_watcher(web_contents(), u"done");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/subresource_filter/delete_loading_frame.html")));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/subresource_filter/"
                                     "docwrite_creates_subframe.html")));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/subresource_filter/"
                                     "docwrite_creates_subframe.html")));

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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/subresource_filter/popup_disallowed_load_helper.html")));
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
  const GURL kInitialPopupUrl =
      embedded_test_server()->GetURL("b.com", "/title2.html");
  const GURL kInitialUrl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Activate only on the initial tab URL - title1.html.
  Configuration config(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));
  ConfigureAsPhishingURL(kInitialUrl);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  content::WebContents* initial_web_contents = web_contents();
  content::WebContents* popup_web_contents = nullptr;
  base::HistogramTester tester;

  // Open a popup to the `kInitialPopupUrl` and wait for it to load. This should
  // not activate.
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(initial_web_contents,
                       content::JsReplace("popup = window.open($1, 'name1');",
                                          kInitialPopupUrl)));
    // `popup_observer.GetWebContents()` waits until a new WebContents is
    // created.
    popup_web_contents = popup_observer.GetWebContents();

    // CAUTION - web_contents() now points to `popup_web_contents` - not
    // `initial_web_contents`!

    // Wait for popup to finish loading
    content::TitleWatcher title_watcher(popup_web_contents,
                                        u"Title Of Awesomeness");
    EXPECT_EQ(u"Title Of Awesomeness", title_watcher.WaitAndGetTitle());
  }

  ASSERT_NE(popup_web_contents, nullptr);
  ASSERT_EQ(initial_web_contents->GetLastCommittedURL(), kInitialUrl);
  ASSERT_EQ(popup_web_contents->GetLastCommittedURL(), kInitialPopupUrl);

  // Check that activation did not happen.
  tester.ExpectBucketCount(kPageLoadActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           0);
  tester.ExpectTotalCount(kPageLoadActivationStateDidInheritHistogram, 0);

  // Now have the initial tab navigate the popup to about:blank and wait for it
  // to finish.
  {
    content::TestNavigationObserver observer(popup_web_contents);
    ASSERT_TRUE(ExecJs(initial_web_contents,
                       "popup = window.open('about:blank', 'name1');"));
    observer.Wait();
  }

  // Now have the initial tab insert content into the popup. Ensure that we're
  // using the activation of the opener, i.e. the initial tab, so the content
  // should be blocked.
  {
    ASSERT_TRUE(ExecJs(initial_web_contents, R"SCRIPT(
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

    content::TitleWatcher title_watcher(popup_web_contents, u"failed");
    title_watcher.AlsoWaitForTitle(u"loaded");

    // Check the load was blocked.
    EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
  }

  // Check the new histograms agree that activation was inherited.
  tester.ExpectBucketCount(kPageLoadActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

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
  tester.ExpectUniqueSample(
      SubresourceFilterBrowserTest::kSubresourceLoadsTotalForPage, 6, 1);
  tester.ExpectUniqueSample(
      SubresourceFilterBrowserTest::kSubresourceLoadsEvaluatedForPage, 6, 1);
  tester.ExpectUniqueSample(
      SubresourceFilterBrowserTest::kSubresourceLoadsMatchedRulesForPage, 4, 1);
  tester.ExpectUniqueSample(
      SubresourceFilterBrowserTest::kSubresourceLoadsDisallowedForPage, 4, 1);
  tester.ExpectTotalCount(
      SubresourceFilterBrowserTest::kEvaluationTotalWallDurationForPage,
      time_recorded);
  tester.ExpectTotalCount(
      SubresourceFilterBrowserTest::kEvaluationTotalCPUDurationForPage,
      time_recorded);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // 5 subframes, each with an include.js, plus a top level include.js.
  int num_subresource_checks = 5 + 5 + 1;
  tester.ExpectTotalCount(SubresourceFilterBrowserTest::kEvaluationWallDuration,
                          time_recorded ? num_subresource_checks : 0);
  tester.ExpectTotalCount(SubresourceFilterBrowserTest::kEvaluationCPUDuration,
                          time_recorded ? num_subresource_checks : 0);
}

}  // namespace

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40236757): Flaky on Mac.
#define MAYBE_ExpectPerformanceHistogramsAreRecorded \
  DISABLED_ExpectPerformanceHistogramsAreRecorded
#else
#define MAYBE_ExpectPerformanceHistogramsAreRecorded \
  ExpectPerformanceHistogramsAreRecorded
#endif
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MAYBE_ExpectPerformanceHistogramsAreRecorded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ResetConfigurationToEnableOnPhishingSites(true /* measure_performance */);
  const GURL url = GetTestUrl(kTestFrameSetPath);
  ConfigureAsPhishingURL(url);

  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationEnabledOnReload) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {true, true});

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {false, false});
}

// Perform a hash change before the initial URL of a frame is navigated. Ensure
// we don't trip any CHECKs (crbug.com/1237409) and that filtering works as
// expected.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       SameDocumentBeforeInitialNavigation) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kFrameUrl(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));

  // Configure to filtering included_script.js
  {
    ConfigureAsPhishingURL(kInitialUrl);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetWithRules({testing::CreateSuffixRule("included_script.js")}));
    Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                         subresource_filter::ActivationScope::ALL_SITES);
    ResetConfiguration(std::move(config));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  content::TestNavigationManager navigation_manager(web_contents(), kFrameUrl);

  // Create an iframe that will navigate to the page with the filtered script.
  // However, perform a hash change that will cause a same document navigation
  // which will finish before the iframe `src` navigation.
  ASSERT_TRUE(ExecJs(web_contents(), content::JsReplace(R"SCRIPT(
      var i = document.createElement("iframe");
      i.src = $1;
      i.onload = () => {window.document.title = "loaded"};
      document.body.appendChild(i);
      i.contentWindow.location.hash = 'test';
    )SCRIPT",
                                                        kFrameUrl)));

  // Get the child RFH
  ASSERT_TRUE(navigation_manager.WaitForResponse());
  auto* child_rfh =
      navigation_manager.GetNavigationHandle()->GetRenderFrameHost();
  ASSERT_TRUE(child_rfh);
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_EQ(child_rfh->GetLastCommittedURL(), kFrameUrl);

  // Wait until the iframe is loaded.
  content::TitleWatcher title_watcher(web_contents(), u"loaded");
  EXPECT_EQ(u"loaded", title_watcher.WaitAndGetTitle());

  // Ensure the included_script.js script was filtered.
  EXPECT_FALSE(WasParsedScriptElementLoaded(child_rfh));
}
}  // namespace subresource_filter
