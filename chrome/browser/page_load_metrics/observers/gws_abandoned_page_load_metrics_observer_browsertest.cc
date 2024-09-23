// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"

#include <vector>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_abandoned_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"
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
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {

const char kSRPDomain[] = "www.google.com";
const char kSRPPath[] = "/search?q=";
const char kSRPRedirectPath[] = "/custom?redirect&q=";

std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(R"(
    <html>
      <body>
        SRP Content
        <!-- for CSI beacon tests -->
        <script>
          performance.mark('SearchHeadStart');
          performance.mark('SearchHeadEnd');
          performance.mark('SearchBodyStart');
          performance.mark('SearchBodyEnd');
        </script>
      </body>
    </html>
  )");
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

using AbandonReason = GWSAbandonedPageLoadMetricsObserver::AbandonReason;
using NavigationMilestone =
    GWSAbandonedPageLoadMetricsObserver::NavigationMilestone;
using page_load_metrics::PageLoadMetricsTestWaiter;

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
  std::vector<NavigationMilestone> all_milestones() {
    return {
        NavigationMilestone::kNavigationStart,
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
        // TODO(crbug.com/352578800): Add other loading milestones.
        NavigationMilestone::kParseStart,
        NavigationMilestone::kHeaderChunkStart,
        NavigationMilestone::kHeaderChunkEnd,
        NavigationMilestone::kBodyChunkStart,
        NavigationMilestone::kBodyChunkEnd,
    };
  }
  std::vector<NavigationMilestone> all_testable_milestones() {
    return {NavigationMilestone::kLoaderStart,
            NavigationMilestone::kFirstRedirectResponseLoaderCallback,
            NavigationMilestone::kNonRedirectResponseLoaderCallback};
  }

  std::vector<NavigationMilestone> all_throttleable_milestones() {
    return {NavigationMilestone::kNavigationStart,
            NavigationMilestone::kFirstRedirectResponseLoaderCallback,
            NavigationMilestone::kNonRedirectResponseLoaderCallback};
  }

  GURL url_srp() {
    GURL url(embedded_test_server()->GetURL(kSRPDomain, kSRPPath));
    CHECK(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }
  GURL url_srp_redirect() {
    GURL url(embedded_test_server()->GetURL(kSRPDomain, kSRPRedirectPath));
    CHECK(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }
  GURL url_non_srp() {
    GURL url(embedded_test_server()->GetURL("a.test", "/title1.html"));
    CHECK(!page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }
  GURL url_non_srp_2() {
    GURL url(embedded_test_server()->GetURL("b.test", "/title2.html"));
    CHECK(!page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

  GURL url_non_srp_redirect_to_srp() {
    GURL url(embedded_test_server()->GetURL("a.test", "/redirect-to-srp"));
    CHECK(!page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

  GURL url_srp_redirect_to_non_srp() {
    GURL url(embedded_test_server()->GetURL(kSRPDomain, "/webhp?q="));
    CHECK(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

  GURL GetTargetURLForMilestone(NavigationMilestone milestone) {
    if (milestone ==
        NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
      return url_srp_redirect();
    }
    return url_srp();
  }

  std::unique_ptr<net::test_server::HttpResponse> NonSRPToSRPRedirectHandler(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", url_srp().spec());
    return http_response;
  }

  std::unique_ptr<net::test_server::HttpResponse> SRPToNonSRPRedirectHandler(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", url_non_srp().spec());
    return http_response;
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  }

  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/custom",
                            base::BindRepeating(SRPRedirectHandler)));
    embedded_test_server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/redirect-to-srp",
        base::BindRepeating(&GWSAbandonedPageLoadMetricsObserverBrowserTest::
                                NonSRPToSRPRedirectHandler,
                            base::Unretained(this))));
    embedded_test_server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/webhp",
        base::BindRepeating(&GWSAbandonedPageLoadMetricsObserverBrowserTest::
                                SRPToNonSRPRedirectHandler,
                            base::Unretained(this))));
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    Start();
  }

  std::string GetMilestoneToAbandonHistogramName(
      NavigationMilestone milestone,
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "") {
    return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix +
           GWSAbandonedPageLoadMetricsObserver::
               GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
                   milestone, abandon_reason) +
           suffix;
  }

  std::string GetMilestoneHistogramName(NavigationMilestone milestone,
                                        std::string suffix = "") {
    return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix +
           GWSAbandonedPageLoadMetricsObserver::
               GetMilestoneHistogramNameWithoutPrefixSuffix(milestone) +
           suffix;
  }

  std::string GetAbandonReasonAtMilestoneHistogramName(
      NavigationMilestone milestone,
      std::string suffix = "") {
    return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix +
           GWSAbandonedPageLoadMetricsObserver::
               GetAbandonReasonAtMilestoneHistogramNameWithoutPrefixSuffix(
                   milestone) +
           suffix;
  }

  std::string GetLastMilestoneBeforeAbandonHistogramName(
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "") {
    return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix +
           GWSAbandonedPageLoadMetricsObserver::
               GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
                   abandon_reason) +
           suffix;
  }

  std::string GetNavigationTypeToAbandonHistogramName(
      std::string_view navigation_type,
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "") {
    return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix +
           GWSAbandonedPageLoadMetricsObserver::
               GetNavigationTypeToAbandonWithoutPrefixSuffix(navigation_type,
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

  // Creates 2 version of every histogram name in `histogram_names`: One without
  // additional suffixes, and one with a RTT suffix, since both versions will be
  // recorded for all logged histograms.
  std::vector<std::pair<std::string, int>> ExpandHistograms(
      std::vector<std::string> histogram_names,
      bool is_incognito = false) {
    std::vector<std::string> with_incognito;
    for (std::string& histogram_name : histogram_names) {
      with_incognito.push_back(histogram_name);
      if (is_incognito) {
        with_incognito.push_back(histogram_name + ".Incognito");
      }
    }
    std::vector<std::pair<std::string, int>> histogram_names_expanded;
    for (std::string& histogram_name : with_incognito) {
      histogram_names_expanded.push_back(std::pair(histogram_name, 1));
      histogram_names_expanded.push_back(std::pair(
          histogram_name +
              ChromeGWSAbandonedPageLoadMetricsObserver::GetSuffixForRTT(
                  g_browser_process->network_quality_tracker()->GetHttpRTT()),
          1));
    }
    return histogram_names_expanded;
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
      std::optional<AbandonReason> abandon_after_hiding_reason = std::nullopt,
      base::OnceCallback<void()> abandon_after_hiding_callback =
          base::OnceCallback<void()>()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing abandonment with reason " << ((int)abandon_reason)
                 << " on milestone " << ((int)abandon_milestone));
    // Use a newly created HistogramTester, to prevent getting samples that are
    // recorded for previous navigations.
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    // Navigate to a non-SRP page, to ensure we have a previous page. This is
    // important for testing hiding the WebContents or crashing the process.
    EXPECT_TRUE(content::NavigateToURL(web_contents, url_non_srp()));
    // Navigate again to another SRP page, so that tests that need to do history
    // navigation before the SRP navigation commits can do so.
    EXPECT_TRUE(content::NavigateToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), url_non_srp_2()));

    // Navigate to SRP, but pause it just after we reach the desired milestone.
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

    // Wait until the SRP navigation finishes.
    EXPECT_TRUE(navigation.WaitForNavigationFinished());
    EXPECT_EQ(expect_committed, navigation.was_committed());

    // Navigate to a non-SRP page to flush metrics. Note that `web_contents`
    // might already be closed at this point. It doesn't matter which
    // WebContents we navigate for metrics flushing purposes, so we navigate
    // the active one.
    EXPECT_TRUE(content::NavigateToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), url_non_srp()));

    bool redirected_from_non_srp =
        (target_url == url_non_srp_redirect_to_srp());
    bool has_redirect =
        (target_url == url_srp_redirect() || redirected_from_non_srp);
    // Navigations that involve redirects from non-SRP URLs will have all the
    // milestones and abandonment logged with the "WasNonSRP" suffix, indicating
    // that a non-SRP redirect was involved.
    std::string histogram_suffix =
        redirected_from_non_srp ? std::string(internal::kSuffixWasNonSRP) : "";

    // There should be new entries for the navigation milestone metrics up until
    // the abandonment, but no entries for milestones after that.
    for (auto milestone : all_milestones()) {
      if (abandon_milestone < milestone ||
          (!has_redirect &&
           (milestone >= NavigationMilestone::kFirstRedirectedRequestStart &&
            milestone <=
                NavigationMilestone::kFirstRedirectResponseLoaderCallback))) {
        histogram_tester.ExpectTotalCount(
            GetMilestoneHistogramName(milestone, histogram_suffix), 0);
      } else {
        histogram_tester.ExpectTotalCount(
            GetMilestoneHistogramName(milestone, histogram_suffix), 1);
      }
    }

    // There should be a new entry for exactly one of the abandonment
    // histograms, indicating that the SRP navigation is abandoned just after
    // `abandon_milestone` because of `abandon_reason`. An exception is when the
    // navigation is first abandoned by hiding, then abandoned again by
    // `abandon_after_hiding_reason` which is not kHidden.

    // Check that the navigation type and last milestone before abandonment due
    // to `abandon_reason` and `abandon_after_hiding_reason` (if set) is
    // correctly recorded.
    if (!abandon_after_hiding_reason.has_value() ||
        abandon_after_hiding_reason == AbandonReason::kHidden) {
      EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                      GetLastMilestoneBeforeAbandonHistogramName()),
                  testing::UnorderedElementsAreArray(ExpandHistograms(
                      {GetLastMilestoneBeforeAbandonHistogramName(
                           abandon_reason, histogram_suffix),
                       GetLastMilestoneBeforeAbandonHistogramName(
                           std::nullopt, histogram_suffix)})));
      EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                      GetNavigationTypeToAbandonHistogramName(
                          internal::kNavigationTypeBrowserNav)),
                  testing::UnorderedElementsAreArray(
                      ExpandHistograms({GetNavigationTypeToAbandonHistogramName(
                          internal::kNavigationTypeBrowserNav, abandon_reason,
                          histogram_suffix)})));
    } else {
      EXPECT_THAT(
          histogram_tester.GetTotalCountsForPrefix(
              GetLastMilestoneBeforeAbandonHistogramName()),
          testing::UnorderedElementsAreArray(ExpandHistograms(
              {GetLastMilestoneBeforeAbandonHistogramName(abandon_reason,
                                                          histogram_suffix),
               GetLastMilestoneBeforeAbandonHistogramName(
                   abandon_after_hiding_reason,
                   internal::kSuffixTabWasHiddenStaysHidden + histogram_suffix),
               GetLastMilestoneBeforeAbandonHistogramName(std::nullopt,
                                                          histogram_suffix),
               GetLastMilestoneBeforeAbandonHistogramName(
                   std::nullopt, internal::kSuffixTabWasHiddenStaysHidden +
                                     histogram_suffix)})));
      EXPECT_THAT(
          histogram_tester.GetTotalCountsForPrefix(
              GetNavigationTypeToAbandonHistogramName(
                  internal::kNavigationTypeBrowserNav)),
          testing::UnorderedElementsAreArray(ExpandHistograms({
              GetNavigationTypeToAbandonHistogramName(
                  internal::kNavigationTypeBrowserNav, abandon_reason,
                  histogram_suffix),
              GetNavigationTypeToAbandonHistogramName(
                  internal::kNavigationTypeBrowserNav,
                  abandon_after_hiding_reason,
                  internal::kSuffixTabWasHiddenStaysHidden + histogram_suffix),
          })));
    }

    for (auto milestone : all_milestones()) {
      if (abandon_milestone == milestone) {
        // Check that the milestone to abandonment time is recorded.
        EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                        GetMilestoneToAbandonHistogramName(milestone)),
                    testing::UnorderedElementsAreArray(ExpandHistograms(
                        {GetMilestoneToAbandonHistogramName(
                             milestone, abandon_reason, histogram_suffix),
                         GetMilestoneToAbandonHistogramName(
                             milestone, std::nullopt, histogram_suffix)})));
        // Check that the abandonment reason at the milestone is recorded.
        EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                        GetAbandonReasonAtMilestoneHistogramName(milestone)),
                    testing::UnorderedElementsAreArray(ExpandHistograms(
                        {GetAbandonReasonAtMilestoneHistogramName(
                            milestone, histogram_suffix)})));
        histogram_tester.ExpectUniqueSample(
            GetAbandonReasonAtMilestoneHistogramName(milestone,
                                                     histogram_suffix),
            abandon_reason, 1);

      } else if (milestone ==
                     NavigationMilestone::kNonRedirectResponseLoaderCallback &&
                 abandon_after_hiding_reason.has_value() &&
                 abandon_after_hiding_reason != AbandonReason::kHidden) {
        // If a navigation was initially abandoned by hiding, but then got
        // abandoned again, it will also log the second abandonment, but with
        // a suffix indicating that it was previously hidden.
        EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                        GetMilestoneToAbandonHistogramName(milestone)),
                    testing::UnorderedElementsAreArray(ExpandHistograms(
                        {GetMilestoneToAbandonHistogramName(
                             milestone, abandon_after_hiding_reason,
                             internal::kSuffixTabWasHiddenStaysHidden +
                                 histogram_suffix),
                         GetMilestoneToAbandonHistogramName(
                             milestone, std::nullopt,
                             internal::kSuffixTabWasHiddenStaysHidden +
                                 histogram_suffix)})));
        EXPECT_THAT(
            histogram_tester.GetTotalCountsForPrefix(
                GetAbandonReasonAtMilestoneHistogramName(milestone)),
            testing::UnorderedElementsAreArray(
                ExpandHistograms({GetAbandonReasonAtMilestoneHistogramName(
                    milestone, internal::kSuffixTabWasHiddenStaysHidden +
                                   histogram_suffix)})));

        histogram_tester.ExpectUniqueSample(
            GetAbandonReasonAtMilestoneHistogramName(
                milestone,
                internal::kSuffixTabWasHiddenStaysHidden + histogram_suffix),
            abandon_after_hiding_reason.value(), 1);
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

    // For navigations that are abandoned due to hiding, we will continue
    // recording the milestones after the hiding abandonment, but will add
    // the "WasHidden" suffix.
    if (abandon_reason == AbandonReason::kHidden) {
      std::string histogram_suffix_after_hiding =
          internal::kSuffixTabWasHiddenStaysHidden + histogram_suffix;
      // Only expect entries for milestones after the hiding takes place.
      bool already_hidden =
          (abandon_milestone == NavigationMilestone::kNavigationStart);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(NavigationMilestone::kNavigationStart,
                                    histogram_suffix_after_hiding),
          0);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(NavigationMilestone::kLoaderStart,
                                    histogram_suffix_after_hiding),
          already_hidden ? 1 : 0);

      already_hidden |=
          (abandon_milestone <
           NavigationMilestone::kFirstRedirectResponseLoaderCallback);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(
              NavigationMilestone::kFirstRedirectedRequestStart,
              histogram_suffix_after_hiding),
          (has_redirect && already_hidden) ? 1 : 0);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(
              NavigationMilestone::kFirstRedirectResponseLoaderCallback,
              histogram_suffix_after_hiding),
          (has_redirect && already_hidden) ? 1 : 0);

      already_hidden |=
          (abandon_milestone <=
           NavigationMilestone::kFirstRedirectResponseLoaderCallback);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(
              NavigationMilestone::kNonRedirectedRequestStart,
              histogram_suffix_after_hiding),
          already_hidden ? 1 : 0);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(
              NavigationMilestone::kNonRedirectResponseLoaderCallback,
              histogram_suffix_after_hiding),
          already_hidden ? 1 : 0);
      // The navigation might be abandoned for a second time after hiding. In
      // that case, the milestones after the second abandonment won't be logged,
      // except if it was another hiding.
      if (abandon_after_hiding_reason.has_value() &&
          abandon_after_hiding_reason == AbandonReason::kHidden) {
        // If the second abandonment is also hiding, that means the tab was
        // shown after the first hiding (before getting hidden again).
        histogram_suffix_after_hiding =
            internal::kSuffixTabWasHiddenLaterShown + histogram_suffix;
      }
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(NavigationMilestone::kCommitSent,
                                    histogram_suffix_after_hiding),
          (!abandon_after_hiding_reason.has_value() ||
           abandon_after_hiding_reason == AbandonReason::kHidden)
              ? 1
              : 0);
      histogram_tester.ExpectTotalCount(
          GetMilestoneHistogramName(NavigationMilestone::kDidCommit,
                                    histogram_suffix_after_hiding),
          (!abandon_after_hiding_reason.has_value() ||
           abandon_after_hiding_reason == AbandonReason::kHidden)
              ? 1
              : 0);
    }

    // There should be UKM entries corresponding to the navigation.
    auto ukm_entries = ukm_recorder.GetEntriesByName("AbandonedSRPNavigation");
    const ukm::mojom::UkmEntry* ukm_entry = ukm_entries[0].get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, url_srp());
    ukm_recorder.ExpectEntryMetric(ukm_entry, "AbandonReason",
                                   (int)abandon_reason);
    ukm_recorder.ExpectEntryMetric(ukm_entry, "LastMilestoneBeforeAbandon",
                                   (int)abandon_milestone);
    if (!abandon_after_hiding_reason.has_value() ||
        abandon_after_hiding_reason == AbandonReason::kHidden) {
      EXPECT_EQ(ukm_entries.size(), 1ul);
    } else {
      EXPECT_EQ(ukm_entries.size(), 2ul);
      const ukm::mojom::UkmEntry* ukm_entry2 = ukm_entries[1].get();
      ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, url_srp());
      ukm_recorder.ExpectEntryMetric(ukm_entry2, "AbandonReason",
                                     (int)abandon_after_hiding_reason.value());
      ukm_recorder.ExpectEntryMetric(
          ukm_entry2, "LastMilestoneBeforeAbandon",
          (int)NavigationMilestone::kNonRedirectResponseLoaderCallback);
    }
    for (auto milestone : all_milestones()) {
      if (abandon_milestone < milestone ||
          (!has_redirect &&
           milestone >= NavigationMilestone::kFirstRedirectedRequestStart &&
           milestone <=
               NavigationMilestone::kFirstRedirectResponseLoaderCallback)) {
        EXPECT_FALSE(ukm_recorder.EntryHasMetric(
            ukm_entry,
            AbandonedPageLoadMetricsObserver::NavigationMilestoneToString(
                milestone) +
                "Time"));
      } else if (milestone ==
                     NavigationMilestone::kFirstRedirectResponseStart ||
                 milestone == NavigationMilestone::
                                  kFirstRedirectResponseLoaderCallback) {
        EXPECT_TRUE(ukm_recorder.EntryHasMetric(
            ukm_entry, "FirstRedirectResponseReceived"));
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

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  void LogAFTBeacons() {
    EXPECT_EQ(true, content::EvalJs(
                        web_contents()->GetPrimaryMainFrame(),
                        content::JsReplace(R"(
      performance.mark($1, {startTime: 100});
      new Promise(resolve => {
        setTimeout(() => {
          performance.mark($2, {startTime: 200});
          resolve(true);
        }, 100);
      });
    )",
                                           internal::kGwsAFTStartMarkName,
                                           internal::kGwsAFTEndMarkName)));
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

IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       NonSRPRedirectToSRP) {
  // Navigate to a non-SRP page that redirects to SRP.
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     url_non_srp_redirect_to_srp(), url_srp()));

  // Navigate to a non-SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/true, /*count=*/1,
      std::string(internal::kSuffixWasNonSRP));

  // There should be no entry for the navigation abandonment metrics.
  ExpectEmptyNavigationAbandonment();
}

IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SRPRedirectToNonSRP) {
  // Navigate to a non-SRP page that redirects to SRP.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), url_srp_redirect_to_non_srp(), url_non_srp()));

  // Navigate to a non-SRP page.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/true, 0);
  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/true, /*count=*/0,
      std::string(internal::kSuffixWasNonSRP));

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
  // Since we made a backward navigation, we will have metrics with
  // `ResponseFromCache`.
  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/false, 1,
      std::string(internal::kSuffixResponseFromCache));
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
  ExpectTotalCountForAllNavigationMilestones(
      /*include_redirect=*/false,
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 1 : 2,
      std::string(internal::kSuffixResponseFromCache));

  ExpectEmptyNavigationAbandonment();
}

// Test that no metric will be recorded for prerender navigation and activation
// to SRP.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       PrerenderToSrp) {
  GURL url_srp_prerender(embedded_test_server()->GetURL(
      kSRPDomain, std::string(kSRPPath) + "prerender"));

  // Navigate to an initial SRP page and wait until load event.]
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_srp()));
  waiter->Wait();

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
// points during the navigation.  Note we are only testing with throttleable
// milestones for this test since the new navigation might take a while to
// arrive on the browser side, and the oldnavigation might have advanced if
// it's not actually paused.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByNewNavigation) {
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
              }));
    }
  }
}

// Test SRP navigations that are cancelled by `content::WebContents::Stop()`
// (which can be triggered by e.g. the stop button), at various points during
// the navigation.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchCancelledByWebContentsStop) {
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
        AbandonReason::kHidden, milestone, GetTargetURLForMilestone(milestone),
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

// Similar to SearchTabHidden, but the navigation starts out with a non-SRP
// URL, that later redirects to SRP.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       NonSRPRedirectToSRP_Hidden) {
  for (NavigationMilestone milestone : all_throttleable_milestones()) {
    // Make sure the WebContents is currently shown, before hiding it later.
    web_contents()->WasShown();

    TestNavigationAbandonment(
        AbandonReason::kHidden, milestone, url_non_srp_redirect_to_srp(),
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

// Similar to SearchTabHidden, but after the navigation is abandoned for the
// first time due to hiding, it gets abandoned again for the second time by
// explicit cancellation.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchTabHiddenThenStopped) {
  // Make sure the WebContents is currently shown, before hiding it later.
  web_contents()->WasShown();

  TestNavigationAbandonment(
      AbandonReason::kHidden,
      // Test hiding at kNavigationStart, then stop the navigation on
      // kNonRedirectResponseLoaderCallback.
      NavigationMilestone::kNavigationStart, url_srp(),
      /*expect_milestone_successful=*/true,
      /*expect_committed=*/false, web_contents(),
      base::BindOnce(
          [](content::WebContents* web_contents,
             content::NavigationHandle* navigation_handle) {
            // Hide the tab during navigation to SRP.
            web_contents->WasHidden();
          },
          web_contents()),
      AbandonReason::kExplicitCancellation,
      base::BindOnce(
          [](content::WebContents* web_contents) {
            // Stop the navigation to SRP.
            web_contents->Stop();
          },
          web_contents()));
}

// Similar to above, but the navigation starts out with a non-SRP URL, that
// later redirects to SRP.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchTabHiddenThenStopped_NonSRPRedirectToSRP) {
  for (NavigationMilestone milestone :
       {NavigationMilestone::kNavigationStart,
        NavigationMilestone::kFirstRedirectResponseLoaderCallback}) {
    // Make sure the WebContents is currently shown, before hiding it later.
    web_contents()->WasShown();

    TestNavigationAbandonment(
        // Test hiding at `milestone`, then stop the navigation on
        // NON_REDIRECT_RESPONSE.
        AbandonReason::kHidden, milestone, url_non_srp_redirect_to_srp(),
        /*expect_milestone_successful=*/true,
        /*expect_committed=*/false, web_contents(),
        base::BindOnce(
            [](content::WebContents* web_contents,
               content::NavigationHandle* navigation_handle) {
              // Hide the tab during navigation to SRP.
              web_contents->WasHidden();
            },
            web_contents()),
        AbandonReason::kExplicitCancellation,
        base::BindOnce(
            [](content::WebContents* web_contents) {
              // Stop the navigation to SRP.
              web_contents->Stop();
            },
            web_contents()));
  }
}

// Test that if a navigation was abandoned by hiding multiple times, only the
// first hiding will be logged.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchTabHiddenMultipleTimes) {
  // Make sure the WebContents is currently shown, before hiding it later.
  web_contents()->WasShown();

  TestNavigationAbandonment(
      AbandonReason::kHidden,
      // Test hiding at kNavigationStart, then stop the navigation on
      // kNonRedirectResponseLoaderCallback.
      NavigationMilestone::kNavigationStart, url_srp(),
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
                  if (milestone == NavigationMilestone::
                                       kFirstRedirectResponseLoaderCallback) {
                    method =
                        content::TestNavigationThrottle::WILL_REDIRECT_REQUEST;
                  } else if (milestone ==
                             NavigationMilestone::
                                 kNonRedirectResponseLoaderCallback) {
                    method =
                        content::TestNavigationThrottle::WILL_PROCESS_RESPONSE;
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
              if (milestone ==
                  NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
                method = content::TestNavigationThrottle::WILL_REDIRECT_REQUEST;
              } else if (milestone == NavigationMilestone::
                                          kNonRedirectResponseLoaderCallback) {
                method = content::TestNavigationThrottle::WILL_PROCESS_RESPONSE;
              }
              auto throttle =
                  std::make_unique<content::TestNavigationThrottle>(handle);
              throttle->SetResponse(
                  method, content::TestNavigationThrottle::SYNCHRONOUS,
                  milestone == NavigationMilestone::
                                   kNonRedirectResponseLoaderCallback
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
      AbandonReason::kRenderProcessGone,
      NavigationMilestone::kNonRedirectResponseLoaderCallback, url_srp(),

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

// TODO(crbug.com/352578800): Flaky.
IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       DISABLED_TabHidden) {
  base::HistogramTester histogram_tester;

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_srp()));
  waiter->Wait();

  // Log AFT beacons after receiving other loading milestones.
  LogAFTBeacons();

  // Hide the tab during page load.
  web_contents()->WasHidden();

  auto milesone_name =
      GetMilestoneToAbandonHistogramName(NavigationMilestone::kAFTEnd);
  auto abandoned_milesone_name = GetMilestoneToAbandonHistogramName(
      NavigationMilestone::kAFTEnd, AbandonReason::kHidden);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(milesone_name),
      testing::ElementsAre(
          testing::Pair(abandoned_milesone_name, 1),
          testing::Pair(
              abandoned_milesone_name +
                  ChromeGWSAbandonedPageLoadMetricsObserver::GetSuffixForRTT(
                      g_browser_process->network_quality_tracker()
                          ->GetHttpRTT()),
              1)));

  // There should be a new entry for all the navigation and loading milestones
  // metrics achieved before abandonment.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1);
}

IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       DuplicateNavigation_BrowserInitiated) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // 1. Start browser-initiated navigation to `url_srp()`
  content::TestNavigationManager nav_manager(web_contents(), url_srp());
  web_contents()->GetController().LoadURL(
      url_srp(), content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  // Pause the navigation at request start.
  EXPECT_TRUE(nav_manager.WaitForRequestStart());

  // 2. Navigate again, also to `url_srp()`.
  web_contents()->GetController().LoadURL(
      url_srp(), content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  // Wait for the first navigation to finish.
  EXPECT_TRUE(nav_manager.WaitForNavigationFinished());
  // Ensure that the first_navigation didn't commit.
  EXPECT_FALSE(nav_manager.was_committed());

  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_EQ(url_srp(),
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Expect that the first navigation didn't get to LoaderStart.
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kNavigationStart), 2);
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kLoaderStart), 1);

  // Check that the abandonment reason is set correctly.
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix(
                  GetAbandonReasonAtMilestoneHistogramName(
                      NavigationMilestone::kNavigationStart)),
              testing::UnorderedElementsAreArray(
                  ExpandHistograms({GetAbandonReasonAtMilestoneHistogramName(
                      NavigationMilestone::kNavigationStart)})));
  histogram_tester().ExpectUniqueSample(
      GetAbandonReasonAtMilestoneHistogramName(
          NavigationMilestone::kNavigationStart),
      AbandonReason::kNewDuplicateNavigation, 1);
}

IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       DuplicateNavigation_RendererInitiated) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // 1. Start renderer-initiated navigation to `url_srp()`
  content::TestNavigationManager nav_manager(web_contents(), url_srp());
  EXPECT_TRUE(ExecJs(web_contents(),
                     content::JsReplace("location.href = $1;", url_srp())));
  // Pause the navigation at request start.
  EXPECT_TRUE(nav_manager.WaitForRequestStart());

  // 2. Navigate again, also to `url_srp()`.
  EXPECT_TRUE(ExecJs(web_contents(),
                     content::JsReplace("location.href = $1;", url_srp())));
  // Wait for the first navigation to finish.
  EXPECT_TRUE(nav_manager.WaitForNavigationFinished());
  // Ensure that the first_navigation didn't commit.
  EXPECT_FALSE(nav_manager.was_committed());

  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_EQ(url_srp(),
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Expect that the first navigation didn't get to LoaderStart.
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kNavigationStart), 2);
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kLoaderStart), 1);

  // Check that the abandonment reason is set correctly.
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix(
                  GetAbandonReasonAtMilestoneHistogramName(
                      NavigationMilestone::kNavigationStart)),
              testing::UnorderedElementsAreArray(
                  ExpandHistograms({GetAbandonReasonAtMilestoneHistogramName(
                      NavigationMilestone::kNavigationStart)})));
  histogram_tester().ExpectUniqueSample(
      GetAbandonReasonAtMilestoneHistogramName(
          NavigationMilestone::kNavigationStart),
      AbandonReason::kNewDuplicateNavigation, 1);
}

IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       MultipleDifferentRendererInitiatedNavigations) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_non_srp()));

  // 1. Start renderer-initiated navigation to `url_srp_redirect()`
  content::TestNavigationManager nav_manager(web_contents(),
                                             url_srp_redirect());
  EXPECT_TRUE(ExecJs(web_contents(), content::JsReplace("location.href = $1;",
                                                        url_srp_redirect())));
  // Pause the navigation at request start.
  EXPECT_TRUE(nav_manager.WaitForRequestStart());

  // 2. Navigate again but to `url_srp()`.
  EXPECT_TRUE(ExecJs(web_contents(),
                     content::JsReplace("location.href = $1;", url_srp())));
  // Run script to ensure that the second link click is already processed.
  EXPECT_TRUE(ExecJs(web_contents(), "console.log('Success');"));

  // Wait for the first navigation to finish.
  EXPECT_TRUE(nav_manager.WaitForNavigationFinished());
  // Ensure that the first_navigationdidn't commit.
  EXPECT_FALSE(nav_manager.was_committed());

  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_EQ(url_srp(),
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Expect that the first navigation didn't get to LoaderStart.
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kNavigationStart), 2);
  histogram_tester().ExpectTotalCount(
      GetMilestoneHistogramName(NavigationMilestone::kLoaderStart), 1);

  // Check that the abandonment reason is setcorrectly.
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix(
                  GetAbandonReasonAtMilestoneHistogramName(
                      NavigationMilestone::kNavigationStart)),
              testing::UnorderedElementsAreArray(
                  ExpandHistograms({GetAbandonReasonAtMilestoneHistogramName(
                      NavigationMilestone::kNavigationStart)})));
  histogram_tester().ExpectUniqueSample(
      GetAbandonReasonAtMilestoneHistogramName(
          NavigationMilestone::kNavigationStart),
      AbandonReason::kNewOtherNavigationRendererInitiated, 1);
}

IN_PROC_BROWSER_TEST_F(GWSAbandonedPageLoadMetricsObserverBrowserTest,
                       SearchIncognitoMode) {
  // Explicitly allow http access for the incognito mode. Otherwise the
  // incognito mode cannot reach to the SRP domain.
  ScopedAllowHttpForHostnamesForTesting allow_http(
      {kSRPDomain}, browser()->profile()->GetPrefs());

  // Navigate to SRP with incognito mode.
  Browser* incognito = CreateIncognitoBrowser();
  content::WebContents* web_contents =
      incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url_srp()));

  // Navigate to a non-SRP page to flush the metrics.
  EXPECT_TRUE(content::NavigateToURL(web_contents, url_non_srp()));

  // There should be a new entry for all the navigation milestones metrics.
  ExpectTotalCountForAllNavigationMilestones(/*include_redirect=*/false, 1,
                                             ".Incognito");
}

// TODO(https://crbug.com/347706997): Test backgrounded case.
