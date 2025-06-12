// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_BROWSERTEST_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_BROWSERTEST_H_

#include <vector>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_abandoned_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_page_load_metrics_observer.h"
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
  std::vector<NavigationMilestone> all_milestones();
  std::vector<NavigationMilestone> all_testable_milestones();
  std::vector<NavigationMilestone> all_throttleable_milestones();

  GURL url_srp();
  GURL url_srp_redirect();
  GURL url_non_srp();
  GURL url_non_srp_2();
  GURL url_non_srp_redirect_to_srp();
  GURL url_srp_redirect_to_non_srp();
  virtual GURL GetTargetURLForMilestone(NavigationMilestone milestone);

  std::unique_ptr<net::test_server::HttpResponse> NonSRPToSRPRedirectHandler(
      const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::HttpResponse> SRPToNonSRPRedirectHandler(
      const net::test_server::HttpRequest& request);
  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter();

  void SetUpOnMainThread() override;

  std::string GetMilestoneToAbandonHistogramName(
      NavigationMilestone milestone,
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "");

  std::string GetMilestoneHistogramName(NavigationMilestone milestone,
                                        std::string suffix = "");

  std::string GetAbandonReasonAtMilestoneHistogramName(
      NavigationMilestone milestone,
      std::string suffix = "");

  std::string GetLastMilestoneBeforeAbandonHistogramName(
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "");

  std::string GetNavigationTypeToAbandonHistogramName(
      std::string_view navigation_type,
      std::optional<AbandonReason> abandon_reason = std::nullopt,
      std::string suffix = "");

  void ExpectTotalCountForAllNavigationMilestones(
      bool include_redirect,
      int count,
      std::string histogram_suffix = "");

  void ExpectEmptyNavigationAbandonmentUntilCommit();

  // Creates 2 version of every histogram name in `histogram_names`: One without
  // additional suffixes, and one with a RTT suffix, since both versions will be
  // recorded for all logged histograms.
  std::vector<std::pair<std::string, int>> ExpandHistograms(
      std::vector<std::string> histogram_names,
      bool is_incognito = false);

  virtual void TestNavigationAbandonment(
      AbandonReason abandon_reason,
      NavigationMilestone abandon_milestone,
      GURL target_url,
      bool expect_milestone_successful,
      bool expect_committed,
      content::WebContents* web_contents,
      base::OnceCallback<void(content::NavigationHandle*)>
          after_nav_start_callback,
      std::optional<AbandonReason> abandon_after_hiding_reason,
      base::OnceCallback<void()> abandon_after_hiding_callback);

  void LogAFTBeacons();

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  // Gets the currently used test server. This is used to switch between the
  // http / https servers depending on the test.
  virtual net::EmbeddedTestServer* current_test_server();

 protected:
  std::vector<NavigationMilestone> all_milestones_with_performance_mark();

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_BROWSERTEST_H_
