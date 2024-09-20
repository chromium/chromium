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
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
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
using page_load_metrics::PageLoadMetricsTestWaiter;
using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;
using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;

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
    // `NavigationMilestone::kLargestContentfulPaint` isn't included in this
    // list, because LCP is collected only at the end of the page lifecycle, the
    // browser has to navigate to another page for testing.
    return {NavigationMilestone::kNavigationStart,
            NavigationMilestone::kLoaderStart,
            NavigationMilestone::kFirstRedirectedRequestStart,
            NavigationMilestone::kFirstRedirectResponseStart,
            NavigationMilestone::kFirstRedirectResponseLoaderCallback,
            NavigationMilestone::kNonRedirectedRequestStart,
            NavigationMilestone::kNonRedirectResponseStart,
            NavigationMilestone::kNonRedirectResponseLoaderCallback,
            NavigationMilestone::kCommitSent,
            NavigationMilestone::kCommitReceived,
            NavigationMilestone::kDidCommit,
            NavigationMilestone::kParseStart,
            NavigationMilestone::kFirstContentfulPaint,
            NavigationMilestone::kDOMContentLoaded,
            NavigationMilestone::kLoadEventStarted};
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

  std::string GetTimeToAbandonFromNavigationStart(NavigationMilestone milestone,
                                                  std::string suffix = "") {
    return internal::kAbandonedPageLoadMetricsHistogramPrefix +
           AbandonedPageLoadMetricsObserver::
               GetTimeToAbandonFromNavigationStartWithoutPrefixSuffix(
                   milestone) +
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

  std::unique_ptr<PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiterForLoading() {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
    waiter->AddPageExpectation(
        PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
    waiter->AddPageExpectation(
        PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
    return waiter;
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  }

  void NavigateToUntrackedUrl() {
    ASSERT_TRUE(
        content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));
  }
};

// Test that a successful navigation will log all the navigation milestones
// metrics and none of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       NoAbandonment) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 0);
  ExpectEmptyNavigationAbandonment();

  // Navigate to `url_a`.
  auto waiter = CreatePageLoadMetricsTestWaiterForLoading();
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  waiter->Wait();

  // There should be a new entry for all the navigation and loading milestones
  // metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);
  histogram_tester().ExpectTotalCount(
      std::string(internal::kAbandonedPageLoadMetricsHistogramPrefix) +
          internal::kRendererProcessCreatedBeforeNavHistogramName,
      1);

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyNavigationAbandonment();
  EXPECT_TRUE(ukm_recorder.GetEntriesByName("AbandonedSRPNavigation").empty());

  // LCP is collected only at the end of the page lifecycle. Navigate to
  // flush.
  NavigateToUntrackedUrl();
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kLargestContentfulPaint),
      1);
}

// Test that a successful history navigation will log all the navigation
// milestones metrics and none of the abandonment metrics, except if the history
// navigation is served from BFCache. In that case, no metric will be recorded.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       HistoryNavigation) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title1.html"));
  // 1) Navigate to A.
  auto waiter1 = CreatePageLoadMetricsTestWaiterForLoading();
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  waiter1->Wait();
  content::RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  // There should be a new entry for all the navigation and loading milestones
  // metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);

  // 2) Navigate to B.
  auto waiter2 = CreatePageLoadMetricsTestWaiterForLoading();
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));
  waiter2->Wait();
  content::RenderFrameHostWrapper rfh_b(web_contents()->GetPrimaryMainFrame());

  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 2);

  // Test non-BFCache-restore history navigation. Ensure that the history
  // navigation won't restore from BFCache, by flushing the BFCache.
  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A without restoring from BFCache.
  auto waiter3 = CreatePageLoadMetricsTestWaiterForLoading();
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  waiter3->Wait();

  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 3);

  // 4) Navigate forward to B, potentially restoring from BFCache.
  auto waiter4 = CreatePageLoadMetricsTestWaiterForLoading();
  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    rfh_b->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  waiter4->Wait();

  // If the forward navigation was a BFCache restore, no navigation milestone
  // metrics will be logged. Otherwise, all milestones will be logged.
  int expected_count =
      (content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 3 : 4);
  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/false, expected_count);
  histogram_tester().ExpectTotalCount(
      std::string(internal::kAbandonedPageLoadMetricsHistogramPrefix) +
          internal::kRendererProcessCreatedBeforeNavHistogramName,
      expected_count);

  // No abandonment happened, so no abandonment metrics was logged.
  ExpectEmptyNavigationAbandonment();
  EXPECT_TRUE(ukm_recorder.GetEntriesByName("AbandonedSRPNavigation").empty());
}

// Test navigations that are cancelled by `content::WebContents::Stop()` (which
// can be triggered by e.g. the stop button), at various points during the
// navigation.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByWebContentsStop) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  for (NavigationMilestone abandon_milestone : all_testable_milestones()) {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
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

    EXPECT_TRUE(
        ukm_recorder.GetEntriesByName("AbandonedSRPNavigation").empty());

    histogram_tester.ExpectTotalCount(
        std::string(internal::kAbandonedPageLoadMetricsHistogramPrefix) +
            internal::kRendererProcessCreatedBeforeNavHistogramName,
        0);
    histogram_tester.ExpectTotalCount(
        std::string(internal::kAbandonedPageLoadMetricsHistogramPrefix) +
            internal::kRendererProcessInitHistogramName,
        0);

    // There should be new entries for the navigation milestone metrics up until
    // the abandonment, but no entries for milestones after that.
    for (auto milestone : all_milestones()) {
      if (abandon_milestone < milestone ||
          (milestone >= NavigationMilestone::kFirstRedirectedRequestStart &&
           milestone <=
               NavigationMilestone::kFirstRedirectResponseLoaderCallback)) {
        histogram_tester.ExpectTotalCount(GetMilestoneHistogramName(milestone),
                                          0);
      } else {
        histogram_tester.ExpectTotalCount(GetMilestoneHistogramName(milestone),
                                          1);
      }
    }

    // There should be a new entry for exactly one of the abandonment
    // histograms, indicating that the navigation is abandoned just after
    // `abandon_milestone` because of the `WebContents::Stop()` call.
    EXPECT_THAT(
        histogram_tester.GetTotalCountsForPrefix(
            GetLastMilestoneBeforeAbandonHistogramName()),
        testing::ElementsAre(
            testing::Pair(GetLastMilestoneBeforeAbandonHistogramName(), 1),
            testing::Pair(GetLastMilestoneBeforeAbandonHistogramName(
                              AbandonReason::kExplicitCancellation),
                          1)));

    for (auto milestone : all_milestones()) {
      if (abandon_milestone == milestone) {
        // Check that the milestone to abandonment time is recorded.
        EXPECT_THAT(
            histogram_tester.GetTotalCountsForPrefix(
                GetMilestoneToAbandonHistogramName(milestone)),
            testing::ElementsAre(
                testing::Pair(GetMilestoneToAbandonHistogramName(milestone), 1),
                testing::Pair(
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

        // Check that the abandonment time from navigation start is recorded.
        histogram_tester.ExpectTotalCount(
            GetTimeToAbandonFromNavigationStart(milestone), 1);
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

IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       AbandonedByTabHidden) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  base::HistogramTester histogram_tester;

  // Navigate to `url_a`, but pause it just after we reach the desired
  // milestone.
  content::TestNavigationManager navigation(web_contents(), url_a);
  web_contents()->GetController().LoadURL(
      url_a, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(navigation.WaitForLoaderStart());

  // Hide the tab during navigation (non-terminal).
  web_contents()->WasHidden();
  EXPECT_TRUE(navigation.WaitForResponse());

  // Stop the navigation to SRP (terminal), wait until the navigation finishes.
  web_contents()->Stop();
  EXPECT_TRUE(navigation.WaitForNavigationFinished());

  // Ensure the record containing the hidden suffix.
  histogram_tester.ExpectTotalCount(
      GetTimeToAbandonFromNavigationStart(
          NavigationMilestone::kNonRedirectResponseLoaderCallback,
          internal::kSuffixTabWasHiddenStaysHidden),
      1);
}

// crbug.com/355352905: The test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(AbandonedPageLoadMetricsObserverBrowserTest,
                       DISABLED_TabHidden) {
  base::HistogramTester histogram_tester;
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));

  // TODO(crbug.com/347706997): Build the test case that hides the current tab
  // for each loading milestone so that we make the abandon histogram testable
  // for all loading miletsones.
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  waiter->Wait();

  // Hide the tab during navigation.
  web_contents()->WasHidden();

  // The series of loading milestones is not a fixed order, but the last
  // milestone of this test page is always first contentful paint milestone.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  GetMilestoneToAbandonHistogramName(
                      NavigationMilestone::kFirstContentfulPaint)),
              testing::ElementsAre(
                  testing::Pair(GetMilestoneToAbandonHistogramName(
                                    NavigationMilestone::kFirstContentfulPaint,
                                    AbandonReason::kHidden),
                                1)));

  // There should be a new entry for all the navigation and loading milestones
  // metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);
}
