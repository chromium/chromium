// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_abandoned_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/gws_abandoned_page_load_metrics_observer_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
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
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {
std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  return http_response;
}
}  // namespace

class FromGwsAbandonedPageLoadMetricsObserverBrowserTest
    : public GWSAbandonedPageLoadMetricsObserverBrowserTest {
 public:
  FromGwsAbandonedPageLoadMetricsObserverBrowserTest() = default;
  ~FromGwsAbandonedPageLoadMetricsObserverBrowserTest() override = default;

 protected:
  GURL url_non_srp_redirect() {
    GURL url(current_test_server()->GetURL("a.test", "/redirect"));
    EXPECT_FALSE(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

  GURL GetTargetURLForMilestone(NavigationMilestone milestone) override {
    if (milestone ==
        NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
      return url_non_srp_redirect();
    }
    return url_non_srp_2();
  }

  std::unique_ptr<net::test_server::HttpResponse> DefaultRedirectHandler(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", url_non_srp_2().spec());
    return http_response;
  }

  void SetUpOnMainThread() override {
    current_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    current_test_server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/redirect",
        base::BindRepeating(
            &FromGwsAbandonedPageLoadMetricsObserverBrowserTest::
                DefaultRedirectHandler,
            base::Unretained(this))));
    GWSAbandonedPageLoadMetricsObserverBrowserTest::SetUpOnMainThread();
  }

  void ExpectEmptyAbandonedHistogramUntilCommit(
      ukm::TestAutoSetUkmRecorder& ukm_recorder) {
    // Only check if we met from `kDidCommit` onwards. We don't check the
    // loading milestones because in most tests when we do multiple navigations
    // one after another, the previous page hasn't reached all its loading
    // milestones, and we would log that as an abandonment.
    auto milestones = ukm_recorder.GetMetricsEntryValues(
        "Navigation.FromGoogleSearch.Abandoned", "LastMilestoneBeforeAbandon");
    for (auto milestone : milestones) {
      EXPECT_GE(milestone, static_cast<int>(NavigationMilestone::kDidCommit));
    }
  }

  void TestNavigationAbandonment(
      AbandonReason abandon_reason,
      NavigationMilestone abandon_milestone,
      GURL target_url,
      bool expect_milestone_successful,
      bool expect_committed,
      content::WebContents* web_contents,
      base::OnceCallback<void(content::NavigationHandle*)>
          after_nav_start_callback,
      std::optional<AbandonReason> abandon_after_hiding_reason,
      base::OnceCallback<void()> abandon_after_hiding_callback) override {
    SCOPED_TRACE(testing::Message()
                 << " Testing abandonment with reason "
                 << static_cast<int>(abandon_reason) << " on milestone "
                 << static_cast<int>(abandon_milestone));
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    // Navigate to a non-SRP page, to ensure we have a previous page. This is
    // important for testing hiding the WebContents or crashing the process.
    EXPECT_TRUE(content::NavigateToURL(web_contents, url_non_srp()));

    // Navigate to SRP so that we kick off the `FromGws` PLMOs.
    EXPECT_TRUE(content::NavigateToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), url_srp()));

    // Purge the previous ukms so that we have a clean record.
    ukm_recorder.Purge();

    // Navigate to a non-SRP, but pause it just after we reach the desired
    // milestone.
    content::TestNavigationManager navigation(web_contents, target_url);
    web_contents->GetController().LoadURL(target_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());
    if (abandon_milestone == NavigationMilestone::kNavigationStart) {
      EXPECT_EQ(navigation.WaitForRequestStart(), expect_milestone_successful);
    } else if (abandon_milestone == NavigationMilestone::kLoaderStart) {
      EXPECT_EQ(navigation.WaitForLoaderStart(), expect_milestone_successful);
    } else if (abandon_milestone ==
               NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
      EXPECT_EQ(navigation.WaitForRequestRedirected(),
                expect_milestone_successful);
    } else if (abandon_milestone ==
               NavigationMilestone::kNonRedirectResponseLoaderCallback) {
      EXPECT_EQ(navigation.WaitForResponse(), expect_milestone_successful);
    }
    // TODO(https://crbug.com/347706997): Test for abandonment after the commit
    // IPC is sent.

    std::move(after_nav_start_callback).Run(navigation.GetNavigationHandle());

    if (abandon_after_hiding_reason.has_value()) {
      EXPECT_EQ(abandon_reason, AbandonReason::kHidden);
      EXPECT_TRUE(navigation.WaitForResponse());
      std::move(abandon_after_hiding_callback).Run();
    }

    // Wait until the navigation finishes.
    EXPECT_TRUE(navigation.WaitForNavigationFinished());
    EXPECT_EQ(expect_committed, navigation.was_committed());

    // Navigate to a non-SRP page to flush metrics. Note that `web_contents`
    // might already be closed at this point. It doesn't matter which
    // WebContents we navigate for metrics flushing purposes, so we navigate
    // the active one.
    EXPECT_TRUE(content::NavigateToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), url_non_srp()));

    // There should be UKM entries corresponding to the navigation.
    auto ukm_entries =
        ukm_recorder.GetEntriesByName("Navigation.FromGoogleSearch.Abandoned");
    const ukm::mojom::UkmEntry* ukm_entry = ukm_entries[0].get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, url_non_srp_2());
    ukm_recorder.ExpectEntryMetric(ukm_entry, "AbandonReason",
                                   static_cast<int>(abandon_reason));
    ukm_recorder.ExpectEntryMetric(ukm_entry, "LastMilestoneBeforeAbandon",
                                   static_cast<int>(abandon_milestone));
    if (!abandon_after_hiding_reason.has_value() ||
        abandon_after_hiding_reason == AbandonReason::kHidden) {
      if (ukm_entries.size() == 2ul) {
        // If there is a second abandonment entry, it must be because the load
        // of the SRP page is interrupted by the flushing browser-initiated
        // navigation.
        const ukm::mojom::UkmEntry* ukm_entry2 = ukm_entries[1].get();
        ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry2, url_non_srp_2());
        ukm_recorder.ExpectEntryMetric(
            ukm_entry2, "AbandonReason",
            static_cast<int>(
                AbandonReason::kNewOtherNavigationBrowserInitiated));
        // The exact abandonment milestone might vary but it must be after the
        // navigation finished committing (kDidCommit and above).
        const int64_t* last_milestone = ukm_recorder.GetEntryMetric(
            ukm_entry2, "LastMilestoneBeforeAbandon");
        EXPECT_GE(*last_milestone, (int64_t)NavigationMilestone::kDidCommit);
      } else {
        EXPECT_EQ(ukm_entries.size(), 1ul);
      }
    } else {
      EXPECT_EQ(ukm_entries.size(), 2ul);
      const ukm::mojom::UkmEntry* ukm_entry2 = ukm_entries[1].get();
      ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry2, url_non_srp_2());
      ukm_recorder.ExpectEntryMetric(
          ukm_entry2, "AbandonReason",
          static_cast<int>(abandon_after_hiding_reason.value()));
      ukm_recorder.ExpectEntryMetric(
          ukm_entry2, "LastMilestoneBeforeAbandon",
          static_cast<int>(
              NavigationMilestone::kNonRedirectResponseLoaderCallback));
    }
  }
};

// Test that a successful navigation from SRP will log all the navigation
// milestones metrics and none of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       FromSearch) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));

  // Navigate to a new page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);
}

// Test that a successful navigation from a non-SRP page will not log any
// navigation milestones metrics nor any of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       FromNonSearch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);
}

IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       WithRedirect) {
  // Navigate to SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Navigate to a redirected non-SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_redirect(),
                                     url_non_srp_2()));

  // Navigate to a non-SRP page to flush.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);
}

// Test navigations that are cancelled by a new navigation, at various
// points during the navigation.  Note we are only testing with throttleable
// milestones for this test since the new navigation might take a while to
// arrive on the browser side, and the oldnavigation might have advanced if
// it's not actually paused.
// TODO(crbug.com/400273873): flaky on Linux with bfcache disabled builds. This
// will be fixed in https://crrev.com/c/6268599.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CancelledByNewNavigation DISABLED_CancelledByNewNavigation
#else
#define MAYBE_CancelledByNewNavigation CancelledByNewNavigation
#endif
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       MAYBE_CancelledByNewNavigation) {
  for (NavigationMilestone milestone : all_throttleable_milestones()) {
    for (AbandonReason reason :
         {AbandonReason::kNewReloadNavigation,
          AbandonReason::kNewHistoryNavigation,
          AbandonReason::kNewOtherNavigationBrowserInitiated,
          AbandonReason::kNewOtherNavigationRendererInitiated}) {
      TestNavigationAbandonment(
          reason, milestone, GetTargetURLForMilestone(milestone),
          /*expect_milestone_successful=*/true,
          /*expect_committed=*/false, web_contents(),
          base::BindLambdaForTesting(
              [&](content::NavigationHandle* navigation_handle) {
                // Navigate to a non-SRP page, which will trigger the
                // cancellation of the SRP navigation. The type of navigation is
                // determined by the `reason` to be tested.
                if (reason == AbandonReason::kNewReloadNavigation) {
                  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                                     "location.reload();"));
                } else if (reason == AbandonReason::kNewHistoryNavigation) {
                  web_contents()->GetController().GoBack();
                } else if (reason ==
                           AbandonReason::kNewOtherNavigationBrowserInitiated) {
                  web_contents()->GetController().LoadURL(
                      GURL("about:blank"), content::Referrer(),
                      ui::PAGE_TRANSITION_LINK, std::string());
                } else {
                  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                                     "location.href = 'about:blank';"));
                }
                EXPECT_TRUE(WaitForLoadStop(web_contents()));
              }),
          std::nullopt, base::OnceCallback<void()>());
    }
  }
}

// Test navigations that are cancelled by `content::WebContents::Stop()`
// (which can be triggered by e.g. the stop button), at various points during
// the navigation.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       CancelledByWebContentsStop) {
  for (NavigationMilestone milestone : all_testable_milestones()) {
    TestNavigationAbandonment(
        AbandonReason::kExplicitCancellation, milestone,
        GetTargetURLForMilestone(milestone),
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/false, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Stop the navigation to SRP.
              web_contents->Stop();
            },
            web_contents()),
        std::nullopt, base::OnceCallback<void()>());
  }
}

// Test navigations that are abandoned because the WebContents is hidden
// at various points during the navigation. Note that the navigation itself
// might continue to commit, but we will count it as "abandoned" as soon as it's
// hidden and stop recording navigation milestones metrics after that.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       TabHidden) {
  for (NavigationMilestone milestone : all_testable_milestones()) {
    // Make sure the WebContents is currently shown, before hiding it later.
    web_contents()->WasShown();

    TestNavigationAbandonment(
        AbandonReason::kHidden, milestone, GetTargetURLForMilestone(milestone),
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/true, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Hide the tab during navigation to SRP.
              web_contents->WasHidden();
            },
            web_contents()),
        std::nullopt, base::OnceCallback<void()>());
  }
}

// Similar to SearchTabHidden, but the navigation starts out with a non-SRP
// URL, that later redirects to SRP.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       Redirect_Hidden) {
  for (NavigationMilestone milestone : all_throttleable_milestones()) {
    // Make sure the WebContents is currently shown, before hiding it later.
    web_contents()->WasShown();

    TestNavigationAbandonment(
        AbandonReason::kHidden, milestone, url_non_srp_redirect(),
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/true, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Hide the tab during navigation to SRP.
              web_contents->WasHidden();
            },
            web_contents()),
        std::nullopt, base::OnceCallback<void()>());
  }
}

// Test that if a navigation was abandoned by hiding multiple times, only the
// first hiding will be logged.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       TabHiddenMultipleTimes) {
  // Make sure the WebContents is currently shown, before hiding it later.
  web_contents()->WasShown();

  TestNavigationAbandonment(
      AbandonReason::kHidden,
      // Test hiding at kNavigationStart, then stop the navigation on
      // kNonRedirectResponseLoaderCallback.
      NavigationMilestone::kNavigationStart, url_non_srp_2(),
      /*expect_milestone_successful=*/true,
      /*expect_committed=*/true, web_contents(),
      base::BindOnce(
          [](content::WebContents* web_contents,
             content::NavigationHandle* navigation_handle) {
            // Hide the tab during navigation to SRP.
            web_contents->WasHidden();
          },
          web_contents()),
      AbandonReason::kHidden,
      base::BindOnce(
          [](content::WebContents* web_contents) {
            // Stop the navigation to SRP.
            web_contents->WasShown();
            web_contents->WasHidden();
          },
          web_contents()));
}

// Test navigations that are cancelled by closing the WebContents at various
// points during the navigation. Note we are only testing with throttleable
// milestones for this teset since the close notification might take a while to
// arrive on the browser side, and the navigation might have advanced if it's
// not actually paused.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       CancelledByTabClose) {
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
        AbandonReason::kFrameRemoved, milestone,
        GetTargetURLForMilestone(milestone),
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
            popup_contents),
        std::nullopt, base::OnceCallback<void()>());
  }
}

// Test navigations that are cancelled by a NavigationThrottle at various
// points during the navigation.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       CancelledByNavigationThrottle) {
  for (content::NavigationThrottle::ThrottleAction action :
       {content::NavigationThrottle::CANCEL,
        content::NavigationThrottle::CANCEL_AND_IGNORE}) {
    for (content::TestNavigationThrottle::ResultSynchrony synchrony :
         {content::TestNavigationThrottle::SYNCHRONOUS,
          content::TestNavigationThrottle::ASYNCHRONOUS}) {
      for (NavigationMilestone milestone : all_throttleable_milestones()) {
        content::TestNavigationThrottleInserter throttle_inserter(
            web_contents(),
            base::BindLambdaForTesting([&](content::NavigationHandle* handle)
                                           -> std::unique_ptr<
                                               content::NavigationThrottle> {
              if (handle->GetURL() != url_non_srp_2() &&
                  handle->GetURL() != url_non_srp_redirect()) {
                return nullptr;
              }
              content::TestNavigationThrottle::ThrottleMethod method =
                  content::TestNavigationThrottle::WILL_START_REQUEST;
              if (milestone ==
                  NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
                method = content::TestNavigationThrottle::WILL_REDIRECT_REQUEST;
              } else if (milestone == NavigationMilestone::
                                          kNonRedirectResponseLoaderCallback) {
                method = content::TestNavigationThrottle::WILL_PROCESS_RESPONSE;
              }
              auto throttle =
                  std::make_unique<content::TestNavigationThrottle>(handle);
              throttle->SetResponse(method, synchrony, action);
              return throttle;
            }));
        TestNavigationAbandonment(
            AbandonReason::kInternalCancellation, milestone,
            GetTargetURLForMilestone(milestone),
            /*expect_milestone_successful=*/false,
            /*expect_committed=*/false, web_contents(),
            base::BindOnce(base::BindOnce(
                [](content::WebContents* web_contents,
                   content::NavigationHandle* navigation_handle) {},
                web_contents())),
            std::nullopt, base::OnceCallback<void()>());
      }
    }
  }
}

// Test navigations that are turned to commit an error page by
// a NavigationThrottle at various points during the navigation. Note that the
// navigation itself will commit, but since it's committing an error page
// instead of SRP, we will count it as "abandoned" as soon as it's turned into
// an error page, and stop recording navigation milestone metrics after that.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       TurnedToErrorPageByNavigationThrottle) {
  for (NavigationMilestone milestone : all_throttleable_milestones()) {
    content::TestNavigationThrottleInserter throttle_inserter(
        web_contents(),
        base::BindLambdaForTesting([&](content::NavigationHandle* handle)
                                       -> std::unique_ptr<
                                           content::NavigationThrottle> {
          if (handle->GetURL() != url_non_srp_2() &&
              handle->GetURL() != url_non_srp_redirect()) {
            return nullptr;
          }
          content::TestNavigationThrottle::ThrottleMethod method =
              content::TestNavigationThrottle::WILL_START_REQUEST;
          if (milestone ==
              NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
            method = content::TestNavigationThrottle::WILL_REDIRECT_REQUEST;
          } else if (milestone ==
                     NavigationMilestone::kNonRedirectResponseLoaderCallback) {
            method = content::TestNavigationThrottle::WILL_PROCESS_RESPONSE;
          }
          auto throttle =
              std::make_unique<content::TestNavigationThrottle>(handle);
          throttle->SetResponse(
              method, content::TestNavigationThrottle::SYNCHRONOUS,
              milestone ==
                      NavigationMilestone::kNonRedirectResponseLoaderCallback
                  ? content::NavigationThrottle::BLOCK_RESPONSE
                  : content::NavigationThrottle::BLOCK_REQUEST);
          return throttle;
        }));
    TestNavigationAbandonment(
        AbandonReason::kErrorPage, milestone,
        GetTargetURLForMilestone(milestone),
        /*expect_milestone_successful=*/false,
        /*expect_committed=*/true, web_contents(),
        base::BindOnce(
            base::BindOnce([](content::WebContents* web_contents,
                              content::NavigationHandle* navigation_handle) {},
                           web_contents())),
        std::nullopt, base::OnceCallback<void()>());
  }
}

// Test navigations that are cancelled because the renderer process picked
// for it crashed. Note that this is only checking the case where the crash
// happens after we get the final response, since the final RenderFrameHost for
// the navigation only start being exposed at that point.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       CancelledByRenderProcessGone) {
  TestNavigationAbandonment(
      AbandonReason::kRenderProcessGone,
      NavigationMilestone::kNonRedirectResponseLoaderCallback, url_non_srp_2(),

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
          web_contents()),
      std::nullopt, base::OnceCallback<void()>());
}
