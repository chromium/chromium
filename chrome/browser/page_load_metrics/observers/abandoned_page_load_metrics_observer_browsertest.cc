// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"

#include <vector>

#include "base/test/bind.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "net/test/embedded_test_server/request_handler_util.h"

using AbandonReason = AbandonedPageLoadMetricsObserver::AbandonReason;
using NavigationMilestone =
    AbandonedPageLoadMetricsObserver::NavigationMilestone;

class AbandonedPageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  AbandonedPageLoadMetricsObserverBrowserTest() = default;

  ~AbandonedPageLoadMetricsObserverBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 protected:
  std::vector<NavigationMilestone> all_milestones() {
    return {NavigationMilestone::kNavigationStart,
            NavigationMilestone::kLoaderStart,
            NavigationMilestone::kFirstRedirectedRequestStart,
            NavigationMilestone::kFirstRedirectResponseStart,
            NavigationMilestone::kFirstRedirectResponseLoaderCallback,
            NavigationMilestone::kNonRedirectedRequestStart,
            NavigationMilestone::kNonRedirectResponseStart,
            NavigationMilestone::kNonRedirectResponseLoaderCallback,
            NavigationMilestone::kCommitSent,
            NavigationMilestone::kDidCommit};
  }
  std::vector<NavigationMilestone> all_testable_milestones() {
    return {NavigationMilestone::kNavigationStart,
            NavigationMilestone::kLoaderStart,
            NavigationMilestone::kNonRedirectResponseLoaderCallback};
  }

  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    Start();
  }

  std::string GetMilestoneToAbandonHistogramName(
      NavigationMilestone milestone,
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "") {
    return internal::kAbandonedPageLoadMetricsHistogramPrefix +
           AbandonedPageLoadMetricsObserver::
               GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
                   milestone, abandon_reason) +
           suffix;
  }

  std::string GetMilestoneHistogramName(NavigationMilestone milestone,
                                        std::string suffix = "") {
    return internal::kAbandonedPageLoadMetricsHistogramPrefix +
           AbandonedPageLoadMetricsObserver::
               GetMilestoneHistogramNameWithoutPrefixSuffix(milestone) +
           suffix;
  }

  std::string GetAbandonReasonAtMilestoneHistogramName(
      NavigationMilestone milestone,
      std::string suffix = "") {
    return internal::kAbandonedPageLoadMetricsHistogramPrefix +
           AbandonedPageLoadMetricsObserver::
               GetAbandonReasonAtMilestoneHistogramNameWithoutPrefixSuffix(
                   milestone) +
           suffix;
  }

  std::string GetLastMilestoneBeforeAbandonHistogramName(
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "") {
    return internal::kAbandonedPageLoadMetricsHistogramPrefix +
           AbandonedPageLoadMetricsObserver::
               GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
                   abandon_reason) +
           suffix;
  }

  void ExpectTotalCountForAllNavigationMilestones(
      bool include_redirect,
      int count,
      std::string histogram_suffix = "") {
    for (auto milestone : all_milestones()) {
      SCOPED_TRACE(
          testing::Message()
          << " ExpectTotalCountForAllNavigationMilestones on milestone "
          << ((int)milestone) << " with suffix " << histogram_suffix);
      bool is_redirect =
          (milestone ==
               NavigationMilestone::kFirstRedirectResponseLoaderCallback ||
           milestone == NavigationMilestone::kFirstRedirectResponseStart ||
           milestone == NavigationMilestone::kFirstRedirectedRequestStart);
      histogram_tester().ExpectTotalCount(
          GetMilestoneHistogramName(milestone, histogram_suffix),
          (!is_redirect || include_redirect) ? count : 0);
    }
  }

  void ExpectEmptyNavigationAbandonment() {
    for (auto milestone : all_milestones()) {
      SCOPED_TRACE(testing::Message()
                   << " ExpectEmptyNavigationAbandonment on milestone "
                   << ((int)milestone));
      EXPECT_TRUE(histogram_tester()
                      .GetTotalCountsForPrefix(
                          GetMilestoneToAbandonHistogramName(milestone))
                      .empty());
    }
  }
};

// Test that a successful navigation will log all the navigation milestones
// metrics and none of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       NoAbandonment) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 0);
  ExpectEmptyNavigationAbandonment();

  // Navigate to `url_a`.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));

  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyNavigationAbandonment();
}

// Test that a successful history navigation will log all the navigation
// milestones metrics and none of the abandonment metrics, except if the history
// navigation is served from BFCache. In that case, no metric will be recorded.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       HistoryNavigation) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title1.html"));
  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));
  content::RenderFrameHostWrapper rfh_b(web_contents()->GetPrimaryMainFrame());

  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 2);

  // Test non-BFCache-restore history navigation. Ensure that the history
  // navigation won't restore from BFCache, by flushing the BFCache.
  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A without restoring from BFCache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 3);

  // 4) Navigate forward to B, potentially restoring from BFCache.
  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    rfh_b->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // If the forward navigation was a BFCache restore, no navigation milestone
  // metrics will be logged. Otherwise, all milestones will be logged.
  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/false,
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 3 : 4);

  // No abandonment happened, so no abandonment metrics was logged.
  ExpectEmptyNavigationAbandonment();
}

// Test navigations that are cancelled by `content::WebContents::Stop()` (which
// can be triggered by e.g. the stop button), at various points during the
// navigation.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByWebContentsStop) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  for (NavigationMilestone abandon_milestone : all_testable_milestones()) {
    base::HistogramTester histogram_tester;

    // Navigate to `url_a`, but pause it just after we reach the desired
    // milestone.
    content::TestNavigationManager navigation(web_contents(), url_a);
    web_contents()->GetController().LoadURL(
        url_a, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    if (abandon_milestone == NavigationMilestone::kNavigationStart) {
      EXPECT_TRUE(navigation.WaitForRequestStart());
    } else if (abandon_milestone == NavigationMilestone::kLoaderStart) {
      EXPECT_TRUE(navigation.WaitForLoaderStart());
    } else if (abandon_milestone ==
               NavigationMilestone::kNonRedirectResponseLoaderCallback) {
      EXPECT_TRUE(navigation.WaitForResponse());
    }

    web_contents()->Stop();

    // Wait until the navigation finishes.
    EXPECT_TRUE(navigation.WaitForNavigationFinished());
    EXPECT_FALSE(navigation.was_committed());

    // There should be new entries for the navigation milestone metrics up until
    // the abandonment, but no entries for milestones after that.
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(NavigationMilestone::kNavigationStart), 1);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(NavigationMilestone::kLoaderStart),
        abandon_milestone == NavigationMilestone::kNavigationStart ? 0 : 1);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(
            NavigationMilestone::kFirstRedirectedRequestStart),
        0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(
            NavigationMilestone::kFirstRedirectResponseStart),
        0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(
            NavigationMilestone::kFirstRedirectResponseLoaderCallback),
        0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(
            NavigationMilestone::kNonRedirectedRequestStart),
        abandon_milestone >= NavigationMilestone::kNonRedirectResponseStart
            ? 1
            : 0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(
            NavigationMilestone::kNonRedirectResponseStart),
        abandon_milestone >= NavigationMilestone::kNonRedirectResponseStart
            ? 1
            : 0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(
            NavigationMilestone::kNonRedirectResponseLoaderCallback),
        abandon_milestone >=
                NavigationMilestone::kNonRedirectResponseLoaderCallback
            ? 1
            : 0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(NavigationMilestone::kCommitSent),
        abandon_milestone >= NavigationMilestone::kCommitSent ? 1 : 0);
    histogram_tester.ExpectTotalCount(
        GetMilestoneHistogramName(NavigationMilestone::kDidCommit), 0);

    // There should be a new entry for exactly one of the abandonment
    // histograms, indicating that the navigation is abandoned just after
    // `abandon_milestone` because of the `WebContents::Stop()` call.
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    GetLastMilestoneBeforeAbandonHistogramName()),
                testing::ElementsAre(
                    testing::Pair(GetLastMilestoneBeforeAbandonHistogramName(
                                      AbandonReason::kExplicitCancellation),
                                  1)));

    for (auto milestone : all_milestones()) {
      if (abandon_milestone == milestone) {
        // Check that the milestone to abandonment time is recorded.
        EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                        GetMilestoneToAbandonHistogramName(milestone)),
                    testing::ElementsAre(testing::Pair(
                        GetMilestoneToAbandonHistogramName(
                            milestone, AbandonReason::kExplicitCancellation),
                        1)));
        // Check that the abandonment reason at the milestone is recorded.
        EXPECT_THAT(
            histogram_tester.GetTotalCountsForPrefix(
                GetAbandonReasonAtMilestoneHistogramName(milestone)),
            testing::ElementsAre(testing::Pair(
                GetAbandonReasonAtMilestoneHistogramName(milestone), 1)));

        histogram_tester.ExpectUniqueSample(
            GetAbandonReasonAtMilestoneHistogramName(milestone),
            AbandonReason::kExplicitCancellation, 1);

      } else {
        EXPECT_TRUE(histogram_tester
                        .GetTotalCountsForPrefix(
                            GetMilestoneToAbandonHistogramName(milestone))
                        .empty());
        EXPECT_TRUE(histogram_tester
                        .GetTotalCountsForPrefix(
                            GetAbandonReasonAtMilestoneHistogramName(milestone))
                        .empty());
      }
    }
  }
}
