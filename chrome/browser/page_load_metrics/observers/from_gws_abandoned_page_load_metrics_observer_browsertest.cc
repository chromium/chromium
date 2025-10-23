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
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/features.h"
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
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {
std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(R"(
    <html>
      <body>
        SRP Content
        <script>
          function createAttributionSrcAnchor({
            id,
            url,
            attributionsrc,
          } = {}) {
            const anchor = document.createElement('a');
            anchor.href = url;
            anchor.target = '_top';
            anchor.setAttribute('attributionsrc', attributionsrc);
            anchor.width = 100;
            anchor.height = 100;
            anchor.id = id;

            anchor.innerText = 'This is link';

            document.body.appendChild(anchor);
            return anchor;
          }

          function simulateClickWithButton(target, button) {
            if (typeof target === 'string')
              target = document.getElementById(target);

            let evt = new MouseEvent('click', {'button': button});
            return target.dispatchEvent(evt);
          }
          function createAndClickAttributionSrcAnchor(params) {
            const anchor = createAttributionSrcAnchor(params);
            simulateClickWithButton(anchor, 0 /* left click */);
            return anchor;
          }
        </script>
      </body>
    </html>
  )");
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

  GURL url_non_srp_error() {
    GURL url(current_test_server()->GetURL("a.test", "/error"));
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

  std::unique_ptr<net::test_server::HttpResponse> DefaultNetErrorHandler(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
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
    current_test_server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/error",
        base::BindRepeating(
            &FromGwsAbandonedPageLoadMetricsObserverBrowserTest::
                DefaultNetErrorHandler,
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

  void CheckTimingInformationMetrics(
      ukm::TestAutoSetUkmRecorder& ukm_recorder,
      NavigationMilestone abandon_milestone,
      GURL target_url,
      std::optional<GURL> recorded_url = std::nullopt,
      std::optional<std::string> impression_name = std::nullopt,
      int entry_index = 0,
      std::optional<uint32_t> category_id = std::nullopt) {
    bool has_redirect = (target_url == url_non_srp_redirect());
    // There should be UKM entries corresponding to the navigation.
    auto ukm_entries = ukm_recorder.GetEntriesByName(
        "Navigation.FromGoogleSearch.TimingInformation");

    if (!recorded_url.has_value()) {
      recorded_url = url_non_srp_2();
    }
    const ukm::mojom::UkmEntry* ukm_entry = ukm_entries[entry_index].get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, recorded_url.value());

    bool post_commit =
        (abandon_milestone >= NavigationMilestone::kDidCommit &&
         abandon_milestone <= NavigationMilestone::kLastEssentialLoadingEvent);
    ukm_recorder.ExpectEntryMetric(ukm_entry, "IsCommitted", post_commit);

    ukm_recorder.ExpectEntryMetric(ukm_entry, "HasImpression",
                                   impression_name.has_value());
    if (impression_name.has_value()) {
      ukm_recorder.ExpectEntryMetric(ukm_entry, "IsEmptyAttributionSrc",
                                     impression_name->empty());
    }

    if (category_id.has_value()) {
      ukm_recorder.ExpectEntryMetric(ukm_entry, "Category",
                                     category_id.value());
    } else {
      EXPECT_FALSE(ukm_recorder.EntryHasMetric(ukm_entry, "Category"));
    }

    int expected_redirects = 0;
    if (has_redirect) {
      if (abandon_milestone >
          NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
        expected_redirects = 1;
      }
      // TODO(crbug.com/390216631): Add second redirect milestone checks.
    }
    ukm_recorder.ExpectEntryMetric(ukm_entry, "RedirectCount",
                                   expected_redirects);

    for (auto milestone : all_milestones()) {
      if (abandon_milestone < milestone ||
          (!has_redirect &&
           milestone == NavigationMilestone::kFirstRedirectedRequestStart)) {
        EXPECT_FALSE(ukm_recorder.EntryHasMetric(
            ukm_entry,
            AbandonedPageLoadMetricsObserver::NavigationMilestoneToString(
                milestone) +
                "Time"));
      } else if (milestone ==
                     NavigationMilestone::kFirstRedirectResponseStart ||
                 milestone == NavigationMilestone::
                                  kFirstRedirectResponseLoaderCallback) {
        ukm_recorder.ExpectEntryMetric(
            ukm_entry, "FirstRedirectResponseReceived", has_redirect);
      } else if (milestone == NavigationMilestone::kNonRedirectResponseStart ||
                 milestone ==
                     NavigationMilestone::kNonRedirectResponseLoaderCallback) {
        EXPECT_TRUE(ukm_recorder.EntryHasMetric(ukm_entry,
                                                "NonRedirectResponseReceived"));
      } else {
        EXPECT_EQ(
            milestone != NavigationMilestone::kNavigationStart,
            ukm_recorder.EntryHasMetric(
                ukm_entry,
                AbandonedPageLoadMetricsObserver::NavigationMilestoneToString(
                    milestone) +
                    "Time"));
      }
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

    auto ukm_entries =
        ukm_recorder.GetEntriesByName("Navigation.FromGoogleSearch.Abandoned");
    // Remove any irrelevant entries from the list of UKM entries by filtering
    // with the URL. This is required to avoid the flakiness when abandoning
    // the navigation with reload on early milestone such as NavigationStart.
    // It seems to be triggering a timing bug which when navigating from
    // a.com to b.com, the early stage reload winds back to a.com even though
    // the navigation to b.com has started.
    ukm_entries.erase(
        std::remove_if(ukm_entries.begin(), ukm_entries.end(),
                       [&ukm_recorder, this](auto entry) {
                         auto* src = ukm_recorder.GetSourceForSourceId(
                             entry->source_id);
                         return !src || src->url() != url_non_srp_2();
                       }),
        ukm_entries.end());

    // There should be UKM entries corresponding to the navigation.
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
    // Finally check the on finish metrics.
    CheckTimingInformationMetrics(ukm_recorder, abandon_milestone, target_url);
  }
};

// Test that a successful navigation from SRP will log all the navigation
// milestones metrics and none of the abandonment metrics.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       FromSearch) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_non_srp_2()));

  // Navigate to a new page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);

  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                url_non_srp_2());
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
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       CancelledByNewNavigation) {
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

// Test that if the non-terminal abandonment will record the TimingInformation
// metrics, and the `OnComplete` will record them again.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TabHiddenBeforeCommitAndFinishNavigation \
  DISABLED_TabHiddenBeforeCommitAndFinishNavigation
#else
#define MAYBE_TabHiddenBeforeCommitAndFinishNavigation \
  TabHiddenBeforeCommitAndFinishNavigation
#endif
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       MAYBE_TabHiddenBeforeCommitAndFinishNavigation) {
  // Make sure the WebContents is currently shown, before hiding it later.
  web_contents()->WasShown();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Navigate to a non-SRP page, to ensure we have a previous page. This is
  // important for testing hiding the WebContents or crashing the process.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // Navigate to SRP so that we kick off the `FromGws` PLMOs.
  EXPECT_TRUE(content::NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), url_srp()));

  // Purge the previous ukms so that we have a clean record.
  ukm_recorder.Purge();

  // Navigate to a non-SRP, but pause it just after we reach the desired
  // milestone.
  content::TestNavigationManager navigation(web_contents(), url_non_srp_2());

  web_contents()->GetController().LoadURL(url_non_srp_2(), content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());

  EXPECT_TRUE(navigation.WaitForRequestStart());

  // Hide the content.
  web_contents()->WasHidden();

  // We expect to record the timing information on non-terminal abandonment.
  EXPECT_EQ(
      ukm_recorder
          .GetEntriesByName("Navigation.FromGoogleSearch.TimingInformation")
          .size(),
      1u);

  // Wait until the navigation finishes.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_TRUE(navigation.was_committed());

  // Delay checking the Timing Information metrics until post commit since the
  // TestUkmRecorder does not tie the source id to a url until we have
  // committed and finalized the url.
  CheckTimingInformationMetrics(
      ukm_recorder, NavigationMilestone::kNavigationStart, url_non_srp_2());

  EXPECT_TRUE(content::NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), url_non_srp()));

  // We expect to record the timing information on terminal abandonment as well.
  EXPECT_EQ(
      ukm_recorder
          .GetEntriesByName("Navigation.FromGoogleSearch.TimingInformation")
          .size(),
      2u);

  // Check if the second timing information entry has all the loading
  // milestones.
  CheckTimingInformationMetrics(
      ukm_recorder, NavigationMilestone::kLastEssentialLoadingEvent,
      url_non_srp_2(), std::nullopt, std::nullopt, /* entry_index = */ 1);
}

// Test that if the `OnComplete` will record the TimingInformation
// metrics, even if it is on Tab close.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       TabCloseAfterFinish) {
  // Make sure the WebContents is currently shown, before hiding it later.
  web_contents()->WasShown();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Navigate to a non-SRP page, to ensure we have a previous page. This is
  // important for testing hiding the WebContents or crashing the process.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // Navigate to SRP so that we kick off the `FromGws` PLMOs.
  EXPECT_TRUE(content::NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), url_srp()));

  // Purge the previous ukms so that we have a clean record.
  ukm_recorder.Purge();

  // Navigate to a non-SRP, and wait until the navigation is finished.
  content::TestNavigationManager navigation(web_contents(), url_non_srp_2());

  web_contents()->GetController().LoadURL(url_non_srp_2(), content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());

  // Wait until the navigation finishes.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_TRUE(navigation.was_committed());

  // We do not expect to see any timing information here, since there are no
  // abandonment, and we have not navigated away yet.
  EXPECT_EQ(
      ukm_recorder
          .GetEntriesByName("Navigation.FromGoogleSearch.TimingInformation")
          .size(),
      0u);

  // Close the tab here so that the timing information would be recorded.
  EXPECT_TRUE(ExecJs(web_contents(), "window.close();"));

  // Navigate to a non-SRP page, to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // We expect to record the timing information on `OnComplete`.
  EXPECT_GE(
      ukm_recorder
          .GetEntriesByName("Navigation.FromGoogleSearch.TimingInformation")
          .size(),
      1u);

  // Check if the timing information entry has all the loading milestones.
  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                url_non_srp_2());
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
            base::BindLambdaForTesting([&](content::NavigationThrottleRegistry&
                                               registry) -> void {
              auto& handle = registry.GetNavigationHandle();
              if (handle.GetURL() != url_non_srp_2() &&
                  handle.GetURL() != url_non_srp_redirect()) {
                return;
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
                  std::make_unique<content::TestNavigationThrottle>(registry);
              throttle->SetResponse(method, synchrony, action);
              registry.AddThrottle(std::move(throttle));
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
        base::BindLambdaForTesting([&](content::NavigationThrottleRegistry&
                                           registry) -> void {
          auto& handle = registry.GetNavigationHandle();
          if (handle.GetURL() != url_non_srp_2() &&
              handle.GetURL() != url_non_srp_redirect()) {
            return;
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
              std::make_unique<content::TestNavigationThrottle>(registry);
          throttle->SetResponse(
              method, content::TestNavigationThrottle::SYNCHRONOUS,
              milestone ==
                      NavigationMilestone::kNonRedirectResponseLoaderCallback
                  ? content::NavigationThrottle::BLOCK_RESPONSE
                  : content::NavigationThrottle::BLOCK_REQUEST);
          registry.AddThrottle(std::move(throttle));
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

// Test navigations that are cancelled because of the server error. We should
// record the net::Error if the cancelation is from the network error.
IN_PROC_BROWSER_TEST_F(FromGwsAbandonedPageLoadMetricsObserverBrowserTest,
                       CancelledByServerError) {
  // Navigate to SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Navigate to a redirected non-SRP page.
  EXPECT_FALSE(content::NavigateToURL(web_contents(), url_non_srp_error()));

  // Navigate to a non-SRP page to flush.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  auto ukm_entries =
      ukm_recorder.GetEntriesByName("Navigation.FromGoogleSearch.Abandoned");
  const ukm::mojom::UkmEntry* ukm_entry = ukm_entries[0].get();
  ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, url_non_srp_error());

  ukm_recorder.ExpectEntryMetric(ukm_entry, "Net.ErrorCode",
                                 -net::ERR_HTTP_RESPONSE_CODE_FAILURE);

  ukm_recorder.ExpectEntryMetric(ukm_entry, "AbandonReason",
                                 static_cast<int>(AbandonReason::kErrorPage));
  ukm_recorder.ExpectEntryMetric(
      ukm_entry, "LastMilestoneBeforeAbandon",
      static_cast<int>(
          NavigationMilestone::kNonRedirectResponseLoaderCallback));

  CheckTimingInformationMetrics(
      ukm_recorder, NavigationMilestone::kNonRedirectResponseLoaderCallback,
      url_non_srp_error(), url_non_srp_error());
}

class FromGwsAbandonedPageLoadMetricsObserverWithImpressionBrowserTest
    : public FromGwsAbandonedPageLoadMetricsObserverBrowserTest {
 public:
  FromGwsAbandonedPageLoadMetricsObserverWithImpressionBrowserTest() = default;
  ~FromGwsAbandonedPageLoadMetricsObserverWithImpressionBrowserTest() override =
      default;

 protected:
  net::EmbeddedTestServer* current_test_server() override {
    return &embedded_https_test_server();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_https_test_server().SetCertHostnames(
        {"example.com", "*.example.com", "foo.com", "*.foo.com", "bar.com",
         "*.bar.com", "a.com", "*.a.com", "b.com", "*.b.com", "c.com",
         "*.c.com", "a.test", "b.test", "www.google.com"});

    embedded_https_test_server().RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "chrome/browser/page_load_metrics/integration_tests/data");
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "third_party/blink/web_tests/external/wpt");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void createAndClickAttributionSrcAnchor(GURL url,
                                          std::string_view attribution_src) {
    ASSERT_TRUE(ExecJs(web_contents(),
                       content::JsReplace(R"(
    createAndClickAttributionSrcAnchor({url: $1, attributionsrc: $2});)",
                                          url.spec(), attribution_src)));
  }
};

// Test that we record successful navigation with the impression associated.
IN_PROC_BROWSER_TEST_F(
    FromGwsAbandonedPageLoadMetricsObserverWithImpressionBrowserTest,
    FromSearchWithImpression) {
  std::string_view kAttributionSrc = "something";

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  createAndClickAttributionSrcAnchor(url_non_srp_2(), kAttributionSrc);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);

  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                url_non_srp_2(), std::nullopt,
                                std::optional<std::string>(kAttributionSrc));
}

// Test that we record successful navigation with the impression with
// empty string associated.
IN_PROC_BROWSER_TEST_F(
    FromGwsAbandonedPageLoadMetricsObserverWithImpressionBrowserTest,
    FromSearchWithEmptyImpression) {
  std::string_view kAttributionSrc = "";

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  createAndClickAttributionSrcAnchor(url_non_srp_2(), kAttributionSrc);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);

  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                url_non_srp_2(), std::nullopt,
                                std::optional<std::string>(kAttributionSrc));
}

class FromGwsAbandonedPageLoadMetricsObserverWithCategoryBrowserTest
    : public FromGwsAbandonedPageLoadMetricsObserverBrowserTest {
 public:
  static constexpr std::string_view kCategoryPrefix = "category:";
  FromGwsAbandonedPageLoadMetricsObserverWithCategoryBrowserTest() {
    std::map<std::string, std::string> params;
    params["category_prefix"] = kCategoryPrefix;

    feature_list_.InitAndEnableFeatureWithParameters(
        page_load_metrics::features::kBeaconLeakageLogging, params);
  }
  ~FromGwsAbandonedPageLoadMetricsObserverWithCategoryBrowserTest() override =
      default;

  GURL url_non_srp_with_category(const std::string& category) {
    GURL target = url_non_srp_2();
    auto new_path = base::StrCat({target.GetPath(), "?category=", category});

    GURL url(current_test_server()->GetURL(target.GetHost(), new_path));
    EXPECT_FALSE(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

 protected:
  void SetUpOnMainThread() override {
    FromGwsAbandonedPageLoadMetricsObserverBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that a successful navigation from SRP will log all the navigation
// milestones metrics and none of the abandonment metrics, with a valid
// category.
IN_PROC_BROWSER_TEST_F(
    FromGwsAbandonedPageLoadMetricsObserverWithCategoryBrowserTest,
    FromSearch) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto target = url_non_srp_with_category(base::StrCat({kCategoryPrefix, "1"}));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), target));

  // Navigate to a new page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);

  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                target, target, std::nullopt, /*entry_index*/ 0,
                                /*category_id=*/1);
}

// Test that a successful navigation from SRP will log all the navigation
// milestones metrics and none of the abandonment metrics with an invalid
// category id.
IN_PROC_BROWSER_TEST_F(
    FromGwsAbandonedPageLoadMetricsObserverWithCategoryBrowserTest,
    FromSearchInvalidCategoryId) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto target =
      url_non_srp_with_category(base::StrCat({kCategoryPrefix, "Invalid"}));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), target));

  // Navigate to a new page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);

  // Since this is an invalid category id, we should not record the category
  // metric.
  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                target, target, std::nullopt, /*entry_index*/ 0,
                                /*category_id=*/std::nullopt);
}

// Test that a successful navigation from SRP will log all the navigation
// milestones metrics and none of the abandonment metrics, with no category
// prefix specified.
IN_PROC_BROWSER_TEST_F(
    FromGwsAbandonedPageLoadMetricsObserverWithCategoryBrowserTest,
    FromSearchInvalidCategoryPrefix) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto target = url_non_srp_with_category("Invalid");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), target));

  // Navigate to a new page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // There should be no new entry for the navigation abandonment metrics.
  ExpectEmptyAbandonedHistogramUntilCommit(ukm_recorder);

  // Since this is an invalid category prefix, we should not record the category
  // metric.
  CheckTimingInformationMetrics(ukm_recorder,
                                NavigationMilestone::kLastEssentialLoadingEvent,
                                target, target, std::nullopt, /*entry_index*/ 0,
                                /*category_id=*/std::nullopt);
}
