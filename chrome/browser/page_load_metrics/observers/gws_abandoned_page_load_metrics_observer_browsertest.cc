// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_abandoned_page_load_metrics_observer.h"

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

namespace {

const char kSRPDomain[] = "www.google.com";
const char kSRPPath[] = "/search?q=";
const char kSRPRedirectPath[] = "/custom?redirect&q=";

std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><body>SRP Content</body></html>");
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> SRPRedirectHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", kSRPPath);
  return http_response;
}

}  // namespace

class GWSAbandonedPageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  GWSAbandonedPageLoadMetricsObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &GWSAbandonedPageLoadMetricsObserverBrowserTest::
                GetActiveWebContents,
            base::Unretained(this))) {}

  ~GWSAbandonedPageLoadMetricsObserverBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 protected:
  enum class NavigationMilestone {
    NAVIGATION_START = 0,
    LOADER_START = 1,
    FIRST_REDIRECT_RESPONSE = 2,
    NON_REDIRECT_RESPONSE = 3,
    COMMIT_SENT = 4,
  };

  std::vector<NavigationMilestone> all_testable_milestones() {
    return {NavigationMilestone::NAVIGATION_START,
            NavigationMilestone::LOADER_START,
            NavigationMilestone::FIRST_REDIRECT_RESPONSE,
            NavigationMilestone::NON_REDIRECT_RESPONSE};
  }

  std::vector<NavigationMilestone> all_throttleable_milestones() {
    return {NavigationMilestone::NAVIGATION_START,
            NavigationMilestone::FIRST_REDIRECT_RESPONSE,
            NavigationMilestone::NON_REDIRECT_RESPONSE};
  }

  GURL url_srp() {
    return embedded_test_server()->GetURL(kSRPDomain, kSRPPath);
  }
  GURL url_srp_redirect() {
    return embedded_test_server()->GetURL(kSRPDomain, kSRPRedirectPath);
  }
  GURL url_non_srp() {
    return embedded_test_server()->GetURL("a.test", "/title1.html");
  }
  GURL url_non_srp_2() {
    return embedded_test_server()->GetURL("b.test", "/title2.html");
  }

  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/custom",
                            base::BindRepeating(SRPRedirectHandler)));
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    Start();
  }

  void ExpectTotalCountForAllNavigationMilestones(bool include_redirect,
                                                  int count) {
    histogram_tester().ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStart, count);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToLoaderStart, count);
    histogram_tester().ExpectTotalCount(
        internal::
            kHistogramGWSLeakageNavigationStartToFirstRedirectedRequestStart,
        include_redirect ? count : 0);
    histogram_tester().ExpectTotalCount(
        internal::
            kHistogramGWSLeakageNavigationStartToFirstRedirectResponseStart,
        include_redirect ? count : 0);
    histogram_tester().ExpectTotalCount(
        internal::
            kHistogramGWSLeakageNavigationStartToNonRedirectedRequestStart,
        count);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToNonRedirectResponseStart,
        count);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToCommitSent, count);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToDidCommit, count);
  }

  void ExpectEmptyNavigationAbandonment() {
    EXPECT_TRUE(histogram_tester()
                    .GetTotalCountsForPrefix(
                        internal::kHistogramGWSLeakageNavigationStartToAbandon)
                    .empty());
    EXPECT_TRUE(histogram_tester()
                    .GetTotalCountsForPrefix(
                        internal::kHistogramGWSLeakageLoaderStartToAbandon)
                    .empty());
    EXPECT_TRUE(
        histogram_tester()
            .GetTotalCountsForPrefix(
                internal::
                    kHistogramGWSLeakageFirstRedirectResponseStartToAbandon)
            .empty());
    EXPECT_TRUE(
        histogram_tester()
            .GetTotalCountsForPrefix(
                internal::kHistogramGWSLeakageNonRedirectResponseStartToAbandon)
            .empty());
    EXPECT_TRUE(histogram_tester()
                    .GetTotalCountsForPrefix(
                        internal::kHistogramGWSLeakageCommitSentToAbandon)
                    .empty());
  }

  void TestNavigationAbandonment(
      std::string abandon_reason,
      NavigationMilestone abandon_milestone,
      bool expect_milestone_successful,
      bool expect_committed,
      content::WebContents* web_contents,
      base::OnceCallback<void(content::NavigationHandle*)>
          after_nav_start_callback) {
    SCOPED_TRACE(testing::Message()
                 << " Testing abandonment with reason " << abandon_reason
                 << " on milestone " << ((int)abandon_milestone));
    // Use a newly created HistogramTester, to prevent getting samples that are
    // recorded for previous navigations.
    base::HistogramTester histogram_tester;

    // Navigate to a non-SRP page, to ensure we have a previous page. This is
    // important for testing hiding the WebContents or crashing the process.
    EXPECT_TRUE(content::NavigateToURL(web_contents, url_non_srp()));

    // Navigate to SRP, but pause it just after we reach the desired milestone.
    GURL target_url =
        (abandon_milestone == NavigationMilestone::FIRST_REDIRECT_RESPONSE)
            ? url_srp_redirect()
            : url_srp();
    content::TestNavigationManager navigation(web_contents, target_url);
    web_contents->GetController().LoadURL(target_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());
    if (abandon_milestone == NavigationMilestone::NAVIGATION_START) {
      EXPECT_EQ(navigation.WaitForRequestStart(), expect_milestone_successful);
    } else if (abandon_milestone == NavigationMilestone::LOADER_START) {
      EXPECT_EQ(navigation.WaitForLoaderStart(), expect_milestone_successful);
    } else if (abandon_milestone ==
               NavigationMilestone::FIRST_REDIRECT_RESPONSE) {
      EXPECT_EQ(navigation.WaitForRequestRedirected(),
                expect_milestone_successful);
    } else if (abandon_milestone ==
               NavigationMilestone::NON_REDIRECT_RESPONSE) {
      EXPECT_EQ(navigation.WaitForResponse(), expect_milestone_successful);
    }

    std::move(after_nav_start_callback).Run(navigation.GetNavigationHandle());

    // Wait until the SRP navigation finishes.
    EXPECT_TRUE(navigation.WaitForNavigationFinished());
    EXPECT_EQ(expect_committed, navigation.was_committed());

    // Navigate to a non-SRP page to flush metrics. Note that `web_contents`
    // might already be closed at this point. It doesn't matter which
    // WebContents we navigate for metrics flushing purposes, so we navigate
    // the active one.
    EXPECT_TRUE(content::NavigateToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), url_non_srp_2()));

    // There should be new entries for the navigation milestone metrics up until
    // the response, but no entries for milestones after that.
    histogram_tester.ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToLoaderStart,
        abandon_milestone == NavigationMilestone::NAVIGATION_START ? 0 : 1);
    histogram_tester.ExpectTotalCount(
        internal::
            kHistogramGWSLeakageNavigationStartToFirstRedirectedRequestStart,
        abandon_milestone == NavigationMilestone::FIRST_REDIRECT_RESPONSE ? 1
                                                                          : 0);
    histogram_tester.ExpectTotalCount(
        internal::
            kHistogramGWSLeakageNavigationStartToFirstRedirectResponseStart,
        abandon_milestone == NavigationMilestone::FIRST_REDIRECT_RESPONSE ? 1
                                                                          : 0);
    histogram_tester.ExpectTotalCount(
        internal::
            kHistogramGWSLeakageNavigationStartToNonRedirectedRequestStart,
        abandon_milestone >= NavigationMilestone::NON_REDIRECT_RESPONSE ? 1
                                                                        : 0);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToNonRedirectResponseStart,
        abandon_milestone >= NavigationMilestone::NON_REDIRECT_RESPONSE ? 1
                                                                        : 0);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToCommitSent,
        abandon_milestone >= NavigationMilestone::COMMIT_SENT ? 1 : 0);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramGWSLeakageNavigationStartToDidCommit, 0);

    // There should be a new entry for exactly one of the abandonment metric,
    // indicating that the SRP navigation is abandoned just after
    // `abandon_milestone` because of `abandon_reason`.

    if (abandon_milestone == NavigationMilestone::NAVIGATION_START) {
      EXPECT_THAT(
          histogram_tester.GetTotalCountsForPrefix(
              internal::kHistogramGWSLeakageNavigationStartToAbandon),
          testing::ElementsAre(testing::Pair(
              std::string(
                  internal::kHistogramGWSLeakageNavigationStartToAbandon) +
                  abandon_reason,
              1)));
    } else {
      EXPECT_TRUE(
          histogram_tester
              .GetTotalCountsForPrefix(
                  internal::kHistogramGWSLeakageNavigationStartToAbandon)
              .empty());
    }

    if (abandon_milestone == NavigationMilestone::LOADER_START) {
      EXPECT_THAT(
          histogram_tester.GetTotalCountsForPrefix(
              internal::kHistogramGWSLeakageLoaderStartToAbandon),
          testing::ElementsAre(testing::Pair(
              std::string(internal::kHistogramGWSLeakageLoaderStartToAbandon) +
                  abandon_reason,
              1)));
    } else {
      EXPECT_TRUE(histogram_tester
                      .GetTotalCountsForPrefix(
                          internal::kHistogramGWSLeakageLoaderStartToAbandon)
                      .empty());
    }

    if (abandon_milestone == NavigationMilestone::FIRST_REDIRECT_RESPONSE) {
      EXPECT_THAT(
          histogram_tester.GetTotalCountsForPrefix(
              internal::
                  kHistogramGWSLeakageFirstRedirectResponseStartToAbandon),
          testing::ElementsAre(testing::Pair(
              std::string(
                  internal::
                      kHistogramGWSLeakageFirstRedirectResponseStartToAbandon) +
                  abandon_reason,
              1)));
    } else {
      EXPECT_TRUE(
          histogram_tester
              .GetTotalCountsForPrefix(
                  internal::
                      kHistogramGWSLeakageFirstRedirectResponseStartToAbandon)
              .empty());
    }

    if (abandon_milestone == NavigationMilestone::NON_REDIRECT_RESPONSE) {
      EXPECT_THAT(
          histogram_tester.GetTotalCountsForPrefix(
              internal::kHistogramGWSLeakageNonRedirectResponseStartToAbandon),
          testing::ElementsAre(testing::Pair(
              std::string(
                  internal::
                      kHistogramGWSLeakageNonRedirectResponseStartToAbandon) +
                  abandon_reason,
              1)));
    } else {
      EXPECT_TRUE(
          histogram_tester
              .GetTotalCountsForPrefix(
                  internal::
                      kHistogramGWSLeakageNonRedirectResponseStartToAbandon)
              .empty());
    }

    // TODO(https://crbug.com/347706997): Test for abandonment after the commit
    // IPC is sent.
    EXPECT_TRUE(histogram_tester
                    .GetTotalCountsForPrefix(
                        internal::kHistogramGWSLeakageCommitSentToAbandon)
                    .empty());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Test that a successful navigation to SRP will log all the navigation
// milestones metrics and none of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest, Search) {
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 0);
  ExpectEmptyNavigationAbandonment();

  // SRP Navigation #1: Navigate to SRP.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  // Navigate to a non-SRP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));

  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyNavigationAbandonment();
}

// Test that a successful navigation to a non-SRP page will not log any
// navigation milestones metrics nor any of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       NonSearch) {
  // Navigate to a non-SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // Navigate to another non-SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));

  // There should be no entry for the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 0);

  // There should be no entry for the navigation abandonment metrics.
  ExpectEmptyNavigationAbandonment();
}

// Test that a successful history navigation to SRP will log all the navigation
// milestones metrics and none of the abandonment metrics, except if the history
// navigation is served from BFCache. In that case, no metric will be recorded.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       HistoryNavigationToSrp) {
  // SRP Navigation #1: Navigate to SRP.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));
  content::RenderFrameHostWrapper rfh_srp_1(
      web_contents()->GetPrimaryMainFrame());

  // Navigate to a non-SRP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));

  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyNavigationAbandonment();

  // Test non-BFCache-restore history navigation. Ensure that the history
  // navigation won't restore from BFCache, by flushing the BFCache.
  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(rfh_srp_1.WaitUntilRenderFrameDeleted());

  // SRP Navigation #2: Go back to SRP without restoring from BFCache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  content::RenderFrameHostWrapper rfh_srp_2(
      web_contents()->GetPrimaryMainFrame());

  // Navigate to non-SRP page to flush the metrics.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 2);
  ExpectEmptyNavigationAbandonment();

  // SRP Navigation #3: Go back to SRP, potentially restoring from BFCache.
  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    rfh_srp_2->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate to non-SRP page to flush the metrics.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // If the navigation is a BFCache restore, no new entry is added to the
  // navigation milestones histograms.
  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/false,
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 2 : 3);
  ExpectEmptyNavigationAbandonment();
}

// Test that no metric will be recorded for prerender navigation and activation
// to SRP.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       PrerenderToSrp) {
  GURL url_srp_prerender(embedded_test_server()->GetURL(
      kSRPDomain, std::string(kSRPPath) + "prerender"));
  // Navigate to an initial SRP page.]
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);
  ExpectEmptyNavigationAbandonment();

  // Start a prerender to SRP.
  prerender_helper().AddPrerender(url_srp_prerender);

  // There should be only 1 entry for all the navigation milestones metrics, for
  // the initial SRP navigation.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);
  ExpectEmptyNavigationAbandonment();

  // Activate the prerendered SRP on the initial WebContents.
  content::TestActivationManager activation_manager(web_contents(),
                                                    url_srp_prerender);
  ASSERT_TRUE(
      content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", url_srp_prerender)));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // Navigate to a non-SRP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));

  // There should be no new entry for the navigation milestones and abandonment
  // metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);
  ExpectEmptyNavigationAbandonment();
}

// Test SRP navigations that are cancelled by a new navigation, at various
// points during the navigation.
// TODO(https://crbug.com/347706997): Record the type of the new navigation.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByNewNavigation) {
  for (NavigationMilestone milestone : all_testable_milestones()) {
    TestNavigationAbandonment(
        internal::kAbandonReasonNewNavigation, milestone,
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/false, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Navigate to a non-SRP page, which will trigger the cancellation
              // of the SRP navigation.
              web_contents->GetController().LoadURL(
                  GURL("about:blank"), content::Referrer(),
                  ui::PAGE_TRANSITION_LINK, std::string());
              EXPECT_TRUE(WaitForLoadStop(web_contents));
            },
            web_contents()));
  }
}

// Test SRP navigations that are cancelled by `content::WebContents::Stop()`
// (which can be triggered by e.g. the stop button), at various points during
// the navigation.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByWebContentsStopn) {
  for (NavigationMilestone milestone : all_testable_milestones()) {
    TestNavigationAbandonment(
        internal::kAbandonReasonExplicitCancellation, milestone,
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/false, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Stop the navigation to SRP.
              web_contents->Stop();
            },
            web_contents()));
  }
}

// Test SRP navigations that are abandoned because the WebContents is hidden
// at various points during the navigation. Note that the navigation itself
// might continue to commit, but we will count it as "abandoned" as soon as it's
// hidden and stop recording navigation milestones metrics after that.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchTabHidden) {
  for (NavigationMilestone milestone : all_testable_milestones()) {
    // Make sure the WebContents is currently shown, before hiding it later.
    web_contents()->WasShown();

    TestNavigationAbandonment(
        internal::kAbandonReasonHidden, milestone,
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/true, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Hide the tab during navigation to SRP.
              web_contents->WasHidden();
            },
            web_contents()));
  }
}

// Test SRP navigations that are cancelled by closing the WebContents at various
// points during the navigation. Note we are only testing with throttleable
// milestones for this teset since the close notification might take a while to
// arrive on the browser side, and the navigation might have advanced if it's
// not actually paused.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByTabClose) {
  for (NavigationMilestone milestone : all_throttleable_milestones()) {
    // Create a popup to do the navigation in, so that we can close the
    // WebContents without closing the whole browsr.
    content::TestNavigationObserver popup_observer(url_non_srp());
    popup_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       content::JsReplace("window.open($1)", url_non_srp())));
    popup_observer.Wait();
    content::WebContents* popup_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    TestNavigationAbandonment(
        internal::kAbandonReasonFrameRemoved, milestone,
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/false, popup_contents,
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              content::WebContentsDestroyedWatcher destroyed_watcher(
                  web_contents);
              EXPECT_TRUE(ExecJs(web_contents, "window.close();"));
              destroyed_watcher.Wait();
            },
            popup_contents));
  }
}

// Test SRP navigations that are cancelled by a NavigationThrottle at various
// points during the navigation.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByNavigationThrottle) {
  for (content::NavigationThrottle::ThrottleAction action :
       {content::NavigationThrottle::CANCEL,
        content::NavigationThrottle::CANCEL_AND_IGNORE}) {
    for (content::TestNavigationThrottle::ResultSynchrony synchrony :
         {content::TestNavigationThrottle::SYNCHRONOUS,
          content::TestNavigationThrottle::ASYNCHRONOUS}) {
      for (NavigationMilestone milestone : all_throttleable_milestones()) {
        content::TestNavigationThrottleInserter throttle_inserter(
            web_contents(),
            base::BindLambdaForTesting(
                [&](content::NavigationHandle* handle)
                    -> std::unique_ptr<content::NavigationThrottle> {
                  if (handle->GetURL() != url_srp() &&
                      handle->GetURL() != url_srp_redirect()) {
                    return nullptr;
                  }
                  content::TestNavigationThrottle::ThrottleMethod method =
                      content::TestNavigationThrottle::WILL_START_REQUEST;
                  if (milestone ==
                      NavigationMilestone::FIRST_REDIRECT_RESPONSE) {
                    method =
                        content::TestNavigationThrottle::WILL_REDIRECT_REQUEST;
                  } else if (milestone ==
                             NavigationMilestone::NON_REDIRECT_RESPONSE) {
                    method =
                        content::TestNavigationThrottle::WILL_PROCESS_RESPONSE;
                  }
                  auto throttle =
                      std::make_unique<content::TestNavigationThrottle>(handle);
                  throttle->SetResponse(method, synchrony, action);
                  return throttle;
                }));
        TestNavigationAbandonment(
            internal::kAbandonReasonInternalCancellation, milestone,
            /*expect_milestone_successful=*/false,
            /*expect_committed=*/false, web_contents(),
            base::BindOnce(base::BindOnce(
                [](content::WebContents* web_contents,
                   content::NavigationHandle* navigation_handle) {},
                web_contents())));
      }
    }
  }
}

// Test SRP navigations that are turned to commit an error page by
// a NavigationThrottle at various points during the navigation. Note that the
// navigation itself will commit, but since it's committing an error page
// instead of SRP, we will count it as "abandoned" as soon as it's turned into
// an error page, and stop recording navigation milestone metrics after that.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchTurnedToErrorPageByNavigationThrottle) {
  for (NavigationMilestone milestone : all_throttleable_milestones()) {
    content::TestNavigationThrottleInserter throttle_inserter(
        web_contents(),
        base::BindLambdaForTesting(
            [&](content::NavigationHandle* handle)
                -> std::unique_ptr<content::NavigationThrottle> {
              if (handle->GetURL() != url_srp() &&
                  handle->GetURL() != url_srp_redirect()) {
                return nullptr;
              }
              content::TestNavigationThrottle::ThrottleMethod method =
                  content::TestNavigationThrottle::WILL_START_REQUEST;
              if (milestone == NavigationMilestone::FIRST_REDIRECT_RESPONSE) {
                method = content::TestNavigationThrottle::WILL_REDIRECT_REQUEST;
              } else if (milestone ==
                         NavigationMilestone::NON_REDIRECT_RESPONSE) {
                method = content::TestNavigationThrottle::WILL_PROCESS_RESPONSE;
              }
              auto throttle =
                  std::make_unique<content::TestNavigationThrottle>(handle);
              throttle->SetResponse(
                  method, content::TestNavigationThrottle::SYNCHRONOUS,
                  milestone == NavigationMilestone::NON_REDIRECT_RESPONSE
                      ? content::NavigationThrottle::BLOCK_RESPONSE
                      : content::NavigationThrottle::BLOCK_REQUEST);
              return throttle;
            }));
    TestNavigationAbandonment(
        internal::kAbandonReasonErrorPage, milestone,
        /*expect_milestone_successful=*/false,
        /*expect_committed=*/true, web_contents(),
        base::BindOnce(
            base::BindOnce([](content::WebContents* web_contents,
                              content::NavigationHandle* navigation_handle) {},
                           web_contents())));
  }
}

// Test SRP navigations that are cancelled because the renderer process picked
// for it crashed. Note that this is only checking the case where the crash
// happens after we get the final response, since the final RenderFrameHost for
// the navigation only start being exposed at that point.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByRenderProcessGone) {
  TestNavigationAbandonment(
      internal::kAbandonReasonRenderProcessGone,
      NavigationMilestone::NON_REDIRECT_RESPONSE,
      /*expect_milestone_successful=*/true,
      /*expect_committed=*/false, web_contents(),
      base::BindOnce(
          [](content::WebContents* web_contents,
             content::NavigationHandle* navigation_handle) {
            content::RenderProcessHost* srp_rph =
                navigation_handle->GetRenderFrameHost()->GetProcess();
            content::RenderProcessHostWatcher crash_observer(
                srp_rph,
                content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
            srp_rph->Shutdown(0);
            crash_observer.Wait();
          },
          web_contents()));
}

// TODO(https://crbug.com/347706997): Test backgrounded case.
