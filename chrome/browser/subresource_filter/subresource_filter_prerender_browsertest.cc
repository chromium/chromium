// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::Page;
using content::RenderFrameHost;
using content::WebContents;
using content::WebContentsConsoleObserver;
using content::test::PrerenderHostObserver;
using subresource_filter::testing::CreateSuffixRule;
using testing::_;
using testing::Mock;

namespace subresource_filter {

// TODO(bokan): These tests don't run on Android but it'd be good to test there
// as well as the UI differs. In particular, testing that prerender activation
// hides infobars.

// Tests -----------------------------------------------------------------------

// A very basic smoke test for prerendering; this test just activates on the
// main frame of a prerender. It currently doesn't check any behavior but
// passes if we don't crash.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       PrerenderingSmokeTest) {
  const GURL prerendering_url =
      embedded_test_server()->GetURL("/page_with_iframe.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Configure dry run filtering on all URLs.
  {
    ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
        "suffix-that-does-not-match-anything"));
    Configuration config(subresource_filter::mojom::ActivationLevel::kDryRun,
                         subresource_filter::ActivationScope::ALL_SITES);
    ResetConfiguration(std::move(config));
  }

  // We should get 2 DryRun page activations, one from the initial page and one
  // from the prerender.
  MockSubresourceFilterObserver observer(web_contents());
  EXPECT_CALL(observer, OnPageActivationComputed(_, HasActivationLevelDryRun()))
      .Times(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  prerender_helper_.AddPrerender(prerendering_url);
}

// Test that we correctly account for activation between the prerendering frame
// and primary pages.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       OnlyPrerenderingFrameActivated) {
  const GURL prerendering_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Configure the filter to run only on the prerendering URL.
  {
    ConfigureAsSubresourceFilterOnlyURL(prerendering_url);
    ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
        "suffix-that-does-not-match-anything"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial URL - ensure filtering is not activated.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelDisabled()));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  // Trigger a prerender to the prerendering URL - this URL should activate the
  // filter.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelEnabled()));
    prerender_helper_.AddPrerender(prerendering_url);
  }
}

// Test that we don't start filtering an unactivated primary page when a
// prerendering page becomes activated.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       UnactivatedPrimaryFrameNotFiltered) {
  const GURL prerendering_url = embedded_test_server()->GetURL("/empty.html");
  const GURL initial_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_delayed_script.html");

  // Configure filtering of `included_script.js` on the prerendering page only.
  {
    ConfigureAsSubresourceFilterOnlyURL(prerendering_url);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial page - it should not activate subresource
  // filtering.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelDisabled()));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));
  }

  // Trigger a prerender to a URL that does have subresource filtering enabled.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelEnabled()));
    prerender_helper_.AddPrerender(prerendering_url);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));
  }

  // Now dynamically try to load `included_script.js` in the primary frame.
  // Ensure it is not filtered.
  EXPECT_TRUE(
      IsDynamicScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
}

// Test that we don't start filtering an unactivated prerendering page when the
// primary page is activated.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       UnactivatedPrerenderingFrameNotFiltered) {
  const GURL prerendering_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Configure filtering of `included_script.js` on the initial URL only.
  {
    ConfigureAsSubresourceFilterOnlyURL(initial_url);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial page that should activate subresource filtering.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelEnabled()));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));
  }

  // Trigger a prerender to an unfiltered URL. The script element that would be
  // blocked on an activated URL should be allowed since the prerender URL
  // shouldn't enable activation.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelDisabled()));
    const content::FrameTreeNodeId host_id =
        prerender_helper_.AddPrerender(prerendering_url);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));

    // Expect that we didn't filter the script in the prerendering page since
    // only the primary page was activated.
    RenderFrameHost* prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(host_id);
    EXPECT_TRUE(WasParsedScriptElementLoaded(prerender_rfh));
  }
}

// Test that we can filter a subresource while inside a prerender. Ensure we
// don't display any UI while prerendered but once the prerender becomes
// primary we then show notifications.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       FilterWhilePrerendered) {
  const GURL prerendering_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Configure filtering of `included_script.js` only on the prerendering URL.
  {
    ConfigureAsSubresourceFilterOnlyURL(prerendering_url);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial URL. Activation is only enabled on the
  // prerendering URL so we expect no activation.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelDisabled()));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  // Trigger a prerender. Ensure it too is activated.
  RenderFrameHost* prerender_rfh = nullptr;
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelEnabled()));
    const content::FrameTreeNodeId host_id =
        prerender_helper_.AddPrerender(prerendering_url);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));

    prerender_rfh = prerender_helper_.GetPrerenderedMainFrameHost(host_id);

    // Expect that the disallowed script was blocked.
    EXPECT_FALSE(WasParsedScriptElementLoaded(prerender_rfh));

    // But ensure we haven't shown the notification UI yet since the page is
    // still prerendering.
    EXPECT_FALSE(
        AdsBlockedInContentSettings(web_contents()->GetPrimaryMainFrame()));
    EXPECT_FALSE(AdsBlockedInContentSettings(prerender_rfh));
  }

  // Makes the prerendering page primary (i.e. the user clicked on a link to
  // the prerendered URL), the UI should now be shown.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer, OnPageActivationComputed(_, _)).Times(0);
    prerender_helper_.NavigatePrimaryPage(prerendering_url);

    ASSERT_TRUE(prerender_rfh->IsInPrimaryMainFrame());
    EXPECT_TRUE(AdsBlockedInContentSettings(prerender_rfh));
  }
}

// Tests that console messages generated by the subresource filter for a
// prerendering page are correctly attributed to the prerendering page's render
// frame host. Note, this doesn't necessarily guarantee they won't be displayed
// in the primary page's console (in fact, this is the current behavior), but
// that's a more general problem of prerendering that will be fixed.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       ConsoleMessageFilterWhilePrerendered) {
  const GURL prerendering_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_delayed_script.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Watch for the subresource filtering activation and resource blocked
  // console messages.
  WebContentsConsoleObserver console_activation_observer(web_contents());
  console_activation_observer.SetPattern(kActivationConsoleMessage);
  WebContentsConsoleObserver console_blocked_observer(web_contents());
  console_blocked_observer.SetPattern(
      base::StringPrintf(kDisallowChildFrameConsoleMessageFormat, "*"));

  // Configure filtering of `included_script.js` only on the prerendering URL.
  {
    ConfigureAsSubresourceFilterOnlyURL(prerendering_url);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial URL and trigger the prerender.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  const content::FrameTreeNodeId host_id =
      prerender_helper_.AddPrerender(prerendering_url);
  RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  ASSERT_FALSE(IsDynamicScriptElementLoaded(prerender_rfh));

  EXPECT_EQ(console_activation_observer.messages().size(), 1ul);
  EXPECT_EQ(
      &console_activation_observer.messages().back().source_frame->GetPage(),
      &prerender_rfh->GetPage());

  EXPECT_EQ(console_blocked_observer.messages().size(), 1ul);
  EXPECT_EQ(&console_blocked_observer.messages().back().source_frame->GetPage(),
            &prerender_rfh->GetPage());
}

// Prerender a page, then navigate it. The prerender will be canceled. We check
// this here since this could change in the future and we'd want to ensure
// subresource filtering is correct in prerender navigations.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       NavigatePrerenderedPage) {
  const GURL prerendering_url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL prerendering_url2 =
      embedded_test_server()->GetURL("a.com", "/title2.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Navigate to the initial URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Trigger a prerendering of title1.html.
  const content::FrameTreeNodeId prerender_host_id =
      prerender_helper_.AddPrerender(prerendering_url1);

  // Now navigate the prerendered page to a cross-site page. Ensure the
  // prerender is canceled.
  // TODO(bokan): If this is ever changed, we should ensure this test checks
  // that we navigate the prerender between filtered and unfiltered pages, and
  // ensure subresources are correctly filtered (or not).
  PrerenderHostObserver host_observer(*web_contents(), prerender_host_id);
  prerender_helper_.NavigatePrerenderedPage(prerender_host_id,
                                            prerendering_url2);
  host_observer.WaitForDestroyed();
}

// Tests that a prerendering page that has filtering activated, will continue
// to filter subresources once made primary (i.e. once the user navigates to
// the prerendered URL).
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       FilteringPrerenderBecomesPrimary) {
  const GURL prerendering_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_delayed_script.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Configure filtering of `included_script.js` only on the prerendering URL.
  {
    ConfigureAsSubresourceFilterOnlyURL(prerendering_url);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Trigger a prerender. Ensure it is activated.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelEnabled()));
    prerender_helper_.AddPrerender(prerendering_url);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));
  }

  // Makes the prerendering page primary (i.e. the user clicked on a link to
  // the prerendered URL). Ensure a new request for a blocked resource will be
  // filtered.
  {
    prerender_helper_.NavigatePrimaryPage(prerendering_url);
    EXPECT_FALSE(
        IsDynamicScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  }
}

// Tests that a prerendering page that doesn't have filtering activated, will
// continue to be unfiltered when made primary (i.e. once the user navigates to
// the prerendered URL) from an activated initial URL.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       NonFilteringPrerenderBecomesPrimary) {
  const GURL prerendering_url = embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_delayed_script.html");
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");

  // Configure filtering of `included_script.js` only on the initial URL.
  {
    ConfigureAsSubresourceFilterOnlyURL(initial_url);
    ASSERT_NO_FATAL_FAILURE(
        SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
    Configuration config(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);
    ResetConfiguration(std::move(config));
  }

  // Navigate to the initial URL. Ensure it is activated.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelEnabled()));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));
  }

  // Trigger a prerender. Ensure it is not activated.
  {
    MockSubresourceFilterObserver observer(web_contents());
    EXPECT_CALL(observer,
                OnPageActivationComputed(_, HasActivationLevelDisabled()));
    prerender_helper_.AddPrerender(prerendering_url);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&observer));
  }

  // Make the prerendering page primary (i.e. the user clicked on a link to
  // the prerendered URL). Ensure a new request to `included_script.js` remains
  // unfiltered.
  {
    prerender_helper_.NavigatePrimaryPage(prerendering_url);
    ASSERT_EQ(prerendering_url, web_contents()->GetLastCommittedURL());
    EXPECT_TRUE(
        IsDynamicScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  }
}

// Very basic test that ad tagging works in a prerender.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       AdTaggingSmokeTest) {
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerendering_url =
      embedded_test_server()->GetURL("/ad_tagging/frame_factory.html");
  const GURL ad_url =
      embedded_test_server()->GetURL("/ad_tagging/frame_factory.html?1");

  SetRulesetWithRules({CreateSuffixRule("ad_script.js")});

  TestSubresourceFilterObserver observer(web_contents());

  RenderFrameHost* prerender_rfh = nullptr;

  // Load the initial page and trigger a prerender.
  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    const content::FrameTreeNodeId prerender_host_id =
        prerender_helper_.AddPrerender(prerendering_url);
    prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(prerender_host_id);
    ASSERT_NE(prerender_rfh, nullptr);
  }

  RenderFrameHost* ad_rfh = nullptr;

  // In the prerendering page, create a child frame from an ad script. Ensure it
  // is correctly tagged as an ad.
  {
    ad_rfh = CreateSrcFrameFromAdScript(prerender_rfh, ad_url);
    ASSERT_NE(ad_rfh, nullptr);
    EXPECT_TRUE(observer.GetIsAdFrame(ad_rfh->GetFrameTreeNodeId()));
    EXPECT_TRUE(EvidenceForFrameComprises(
        ad_rfh, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }

  // Make the prerendering page primary (i.e. the user clicked on a link to the
  // prerendered URL). Ensure ad tagging remains valid for the ad frame and its
  // frame tree node id.
  {
    prerender_helper_.NavigatePrimaryPage(prerendering_url);
    ASSERT_EQ(prerendering_url, web_contents()->GetLastCommittedURL());
    EXPECT_TRUE(observer.GetIsAdFrame(ad_rfh->GetFrameTreeNodeId()));
    EXPECT_TRUE(EvidenceForFrameComprises(
        ad_rfh, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }
}

// Tests that NavigationConsoleLogger works with a prerendered page by checking
// if a console message is added in LogMessageOnCommit() from NotifyResult()
// during navigation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPrerenderingBrowserTest,
                       NavigationConsoleLogger) {
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::BETTER_ADS);
  ResetConfiguration(std::move(config));

  {
    GURL url(GetTestUrl("/empty.html"));
    ConfigureURLWithWarning(url,
                            {safe_browsing::SubresourceFilterType::BETTER_ADS});
    content::WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kActivationWarningConsoleMessage);
    // Initial page loading adds a console message.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(console_observer.Wait());
    ASSERT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(kActivationWarningConsoleMessage,
              console_observer.GetMessageAt(0u));
  }
  {
    GURL prerender_url(GetTestUrl("/title1.html"));
    ConfigureURLWithWarning(prerender_url,
                            {safe_browsing::SubresourceFilterType::BETTER_ADS});
    content::WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kActivationWarningConsoleMessage);
    // Trigger a prerender.
    const content::FrameTreeNodeId host_id =
        prerender_helper_.AddPrerender(prerender_url);
    ASSERT_TRUE(console_observer.Wait());
    RenderFrameHost* prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(host_id);
    // The prerendering adds a console message.
    ASSERT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(kActivationWarningConsoleMessage,
              console_observer.GetMessageAt(0u));
    EXPECT_EQ(&console_observer.messages().back().source_frame->GetPage(),
              &prerender_rfh->GetPage());
    // Activate the prerendered page.
    prerender_helper_.NavigatePrimaryPage(prerender_url);
    // The prerendering activation doesn't add a console message.
    ASSERT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(kActivationWarningConsoleMessage,
              console_observer.GetMessageAt(0u));
  }
}

}  // namespace subresource_filter
