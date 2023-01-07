// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include "base/strings/pattern.h"
#include "base/test/bind.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using ::testing::_;
using ::testing::Mock;

namespace subresource_filter {

namespace {
// This string comes from GetErrorStringForDisallowedLoad() in
// blink/renderer/core/loader/subresource_filter.cc
constexpr const char kBlinkDisallowChildFrameConsoleMessageFormat[] =
    "Chrome blocked resource %s on this site because this site tends to show "
    "ads that interrupt, distract, mislead, or prevent user control. Learn "
    "more at https://www.chromestatus.com/feature/5738264052891648";
}  // namespace

// Tests that AddMessageToConsole() is not called from NavigationConsoleLogger
// with a fenced frame to ensure that it works only with the outermost main
// frame.
IN_PROC_BROWSER_TEST_F(SubresourceFilterFencedFrameBrowserTest,
                       NavigatesToURLWithWarning_NoMessageLogged) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*show ads*");

  GURL fenced_frame_url(GetTestUrl("/fenced_frames/title1.html"));
  ConfigureURLWithWarning(fenced_frame_url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::BETTER_ADS);
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(0u, console_observer.messages().size());

  // Load a fenced frame.
  ConfigureURLWithWarning(fenced_frame_url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});
  RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_EQ(0u, console_observer.messages().size());

  // Navigate the fenced frame again.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            fenced_frame_url);
  ASSERT_EQ(0u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterFencedFrameBrowserTest,
                       CollapseBlockedFencedFrame) {
  const GURL kTopLevelUrl(GetTestUrl("title1.html"));
  ConfigureAsPhishingURL(kTopLevelUrl);

  const GURL kUrlWithIncludedScript(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  const GURL kUrlWithAllowedScript(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));

  // Block documents that end in "included_script.html".
  proto::UrlRule rule = testing::CreateSuffixRule("included_script.html");
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules({rule}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTopLevelUrl));

  // Load an unblocked document into the fenced frame, ensure it isn't
  // collapsed.
  RenderFrameHost* fenced_frame_root =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), kUrlWithAllowedScript);
  EXPECT_EQ("300x150", EvalJs(web_contents()->GetPrimaryMainFrame(), R"JS(
    let ff = document.querySelector('fencedframe');
    `${ff.clientWidth}x${ff.clientHeight}`;
  )JS"));

  fenced_frame_root = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_root, kUrlWithIncludedScript,
      /*expected_error_code=*/net::ERR_ABORTED);

  EXPECT_EQ("0x0", EvalJs(web_contents()->GetPrimaryMainFrame(), R"JS(
    let ff = document.querySelector('fencedframe');
    `${ff.clientWidth}x${ff.clientHeight}`;
  )JS"));

  // Now navigate back to an unblocked document and verify the node uncollapses.
  fenced_frame_root = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_root, kUrlWithAllowedScript);

  EXPECT_EQ("300x150", EvalJs(web_contents()->GetPrimaryMainFrame(), R"JS(
    let ff = document.querySelector('fencedframe');
    `${ff.clientWidth}x${ff.clientHeight}`;
  )JS"));
}

// Test that filtering resources inside a fencedframe works when the outer page
// is activated.
IN_PROC_BROWSER_TEST_F(SubresourceFilterFencedFrameBrowserTest,
                       OutermostFrameActivation) {
  const std::string kMessageFilter =
      base::StringPrintf(kBlinkDisallowChildFrameConsoleMessageFormat, "*");
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kMessageFilter);

  const GURL kTopLevelUrl(GetTestUrl("title1.html"));
  ConfigureAsPhishingURL(kTopLevelUrl);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Navigate to a phishing page and load a page with ad script into a
  // fencedframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTopLevelUrl));
  RenderFrameHost* fenced_frame_root =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(),
          GetTestUrl("/subresource_filter/frame_with_included_script.html"));

  // Ensure the disallowed script was blocked.
  EXPECT_FALSE(WasParsedScriptElementLoaded(fenced_frame_root));

  // Ensure content settings has seen the ad as blocked (i.e. the UI was
  // shown).
  EXPECT_TRUE(
      AdsBlockedInContentSettings(web_contents()->GetPrimaryMainFrame()));
  EXPECT_FALSE(AdsBlockedInContentSettings(fenced_frame_root));

  // Console message for subframe blocking should be displayed.
  EXPECT_TRUE(base::MatchPattern(
      console_observer.GetMessageAt(0u),
      base::StringPrintf(kBlinkDisallowChildFrameConsoleMessageFormat,
                         "*included_script.js")));
}

// Tests that navigations of a fenced frame are correctly blocked when the
// outer page is activated and the fenced frame URL matches a blocklist or
// allowlist rule.
IN_PROC_BROWSER_TEST_F(SubresourceFilterFencedFrameBrowserTest,
                       FencedFrameLoadFiltering) {
  const GURL kTopLevelUrl(GetTestUrl("title1.html"));
  ConfigureAsPhishingURL(kTopLevelUrl);

  const GURL kUrlWithIncludedScript(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  const GURL kUrlWithAllowedScript(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  const std::string kAllowlistedDomain = "allowlisted.example";
  const GURL kAllowlistedUrlWithIncludedScript(embedded_test_server()->GetURL(
      kAllowlistedDomain,
      "/subresource_filter/frame_with_included_script.html"));

  // Block documents that end in "included_script.html", unless the document is
  // loaded from an allowlisted domain. This enables the part of this test
  // disallowing a load only after a redirect.
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {testing::CreateSuffixRule("included_script.html"),
       testing::CreateAllowlistSubstringRule(kAllowlistedDomain)}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTopLevelUrl));

  // Loading a document into a fenced frame that's on the blocklist should
  // cancel the load.
  RenderFrameHost* fenced_frame_root =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), kUrlWithIncludedScript,
          /*expected_error_code=*/net::ERR_ABORTED);
  EXPECT_FALSE(WasParsedScriptElementLoaded(fenced_frame_root));

  // Now navigate the fenced frame to a non-blocked document and ensure that
  // the load is successful and the frame gets restored (no longer collapsed).
  fenced_frame_root = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_root, kUrlWithAllowedScript);
  EXPECT_TRUE(WasParsedScriptElementLoaded(fenced_frame_root));

  // Navigate to a URL that has the blocked suffix but whose domain is
  // allowlisted. This should not be filtered.
  fenced_frame_root = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_root, kAllowlistedUrlWithIncludedScript);
  EXPECT_TRUE(WasParsedScriptElementLoaded(fenced_frame_root));
  EXPECT_EQ(kAllowlistedUrlWithIncludedScript,
            fenced_frame_root->GetLastCommittedURL());

  // Finally, navigate the fenced frame to an allowlisted URL that redirects to
  // the blocked page and verify that the navigation is cancelled.
  const GURL kRedirectingUrl(embedded_test_server()->GetURL(
      kAllowlistedDomain, "/server-redirect?" + kUrlWithIncludedScript.spec()));
  fenced_frame_root = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_root, kRedirectingUrl,
      /*expected_error_code=*/net::ERR_ABORTED);

  EXPECT_FALSE(WasParsedScriptElementLoaded(fenced_frame_root));
  EXPECT_EQ(kUrlWithIncludedScript, fenced_frame_root->GetLastCommittedURL());
}

// Same as above test but tests filtering of navigations occurring inside
// subframes embedded within in a fenced frame.
IN_PROC_BROWSER_TEST_F(SubresourceFilterFencedFrameBrowserTest,
                       LoadFilteringNestedInFencedFrame) {
  const GURL kTopLevelUrl(GetTestUrl("title1.html"));
  ConfigureAsPhishingURL(kTopLevelUrl);

  const GURL kFencedFrameUrl(
      GetTestUrl("subresource_filter/included_script_in_iframe.html"));
  const GURL kUrlWithIncludedScript(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  const GURL kUrlWithAllowedScript(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  const std::string kAllowlistedDomain = "allowlisted.example";
  const GURL kAllowlistedUrlWithIncludedScript(embedded_test_server()->GetURL(
      kAllowlistedDomain,
      "/subresource_filter/frame_with_included_script.html"));

  // Block documents that end in "included_script.html", unless the document is
  // loaded from an allowlisted domain. This enables the third part of this
  // test disallowing a load only after the first redirect.
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {testing::CreateSuffixRule("included_script.html"),
       testing::CreateAllowlistSubstringRule(kAllowlistedDomain)}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTopLevelUrl));
  RenderFrameHost* fenced_frame_root =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), kFencedFrameUrl);
  RenderFrameHost* subframe = content::FrameMatchingPredicate(
      fenced_frame_root->GetPage(),
      base::BindRepeating(&content::FrameMatchesName, "subframe"));

  // The fenced frame document initially has an iframe with
  // frame_with_included_script.html so it should be blocked.
  EXPECT_FALSE(WasParsedScriptElementLoaded(subframe));
  EXPECT_TRUE(subframe->IsErrorDocument());

  // Now navigate the subframe to a non-blocked document and ensure that
  // the load is successful and the frame gets restored (no longer collapsed).
  subframe = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      subframe, kUrlWithAllowedScript);
  EXPECT_TRUE(WasParsedScriptElementLoaded(subframe));
  EXPECT_FALSE(subframe->IsErrorDocument());
  EXPECT_EQ(kUrlWithAllowedScript, subframe->GetLastCommittedURL());

  // Navigate to a URL that has the blocked suffix but whose domain is
  // allowlisted. This should not be filtered.
  subframe = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      subframe, kAllowlistedUrlWithIncludedScript);
  EXPECT_TRUE(WasParsedScriptElementLoaded(subframe));
  EXPECT_EQ(kAllowlistedUrlWithIncludedScript, subframe->GetLastCommittedURL());

  // Finally, navigate the subframe to an allowlisted URL that redirects to
  // the blocked page and verify that the navigation is cancelled.
  const GURL kRedirectingUrl(embedded_test_server()->GetURL(
      kAllowlistedDomain, "/server-redirect?" + kUrlWithIncludedScript.spec()));
  subframe = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      subframe, kRedirectingUrl, /*expected_error_code=*/net::ERR_ABORTED);

  EXPECT_FALSE(WasParsedScriptElementLoaded(subframe));
  EXPECT_TRUE(subframe->IsErrorDocument());
  EXPECT_EQ(kUrlWithIncludedScript, subframe->GetLastCommittedURL());
}

}  // namespace subresource_filter
