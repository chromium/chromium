// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tpcd_pref_names.h"
#include "components/privacy_sandbox/tpcd_utils.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"

namespace {

typedef InProcessBrowserTest NavigationMetricsRecorderBrowserTest;

// A site engagement score that falls into the range for HIGH engagement level.
const int kHighEngagementScore = 50;

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest, TestMetrics) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html, <html></html>")));
  histograms.ExpectTotalCount(navigation_metrics::kMainFrameScheme, 1);
  histograms.ExpectBucketCount(navigation_metrics::kMainFrameScheme,
                               5 /* data: */, 1);
  histograms.ExpectTotalCount(navigation_metrics::kMainFrameSchemeDifferentPage,
                              1);
  histograms.ExpectBucketCount(
      navigation_metrics::kMainFrameSchemeDifferentPage, 5 /* data: */, 1);
}

// crbug.com/1292471: the test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Navigation_EngagementLevel DISABLED_Navigation_EngagementLevel
#else
#define MAYBE_Navigation_EngagementLevel Navigation_EngagementLevel
#endif
IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest,
                       MAYBE_Navigation_EngagementLevel) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  const GURL url("https://google.com");
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::NONE, 1);

  site_engagement::SiteEngagementService::Get(browser()->profile())
      ->ResetBaseScoreForURL(url, kHighEngagementScore);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 2);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::NONE, 1);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::HIGH, 1);
}

class NavigationMetricsRecorderPrerenderBrowserTest
    : public NavigationMetricsRecorderBrowserTest {
 public:
  NavigationMetricsRecorderPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &NavigationMetricsRecorderPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~NavigationMetricsRecorderPrerenderBrowserTest() override = default;
  NavigationMetricsRecorderPrerenderBrowserTest(
      const NavigationMetricsRecorderPrerenderBrowserTest&) = delete;

  NavigationMetricsRecorderPrerenderBrowserTest& operator=(
      const NavigationMetricsRecorderPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    NavigationMetricsRecorderBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    NavigationMetricsRecorderBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderPrerenderBrowserTest,
                       PrerenderingShouldNotRecordSiteEngagementLevelMetric) {
  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          GetWebContents());
  ASSERT_TRUE(recorder);

  base::HistogramTester histograms;

  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);

  // Load a prerender page and prerendering should not increase the total count.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 2);
}

struct ExperimentVersusActualCookieStatusHistogramBrowserTestCase {
  bool is_experiment_cookies_disabled = true;
  bool is_third_party_cookies_allowed = true;
  tpcd::experiment::utils::ExperimentState is_client_eligible =
      tpcd::experiment::utils::ExperimentState::kEligible;
  tpcd::experiment::utils::Experiment3PCBlockStatus expected_bucket;
};

const ExperimentVersusActualCookieStatusHistogramBrowserTestCase kTestCases[] =
    {{
         .expected_bucket = tpcd::experiment::utils::Experiment3PCBlockStatus::
             kAllowedAndExperimentBlocked,
     },
     {
         .is_experiment_cookies_disabled = false,
         .is_client_eligible =
             tpcd::experiment::utils::ExperimentState::kIneligible,
         .expected_bucket = tpcd::experiment::utils::Experiment3PCBlockStatus::
             kAllowedAndExperimentAllowed,
     },
     {
         .is_third_party_cookies_allowed = false,
         .expected_bucket = tpcd::experiment::utils::Experiment3PCBlockStatus::
             kBlockedAndExperimentBlocked,
     },
     {
         .is_experiment_cookies_disabled = false,
         .is_third_party_cookies_allowed = false,
         .is_client_eligible =
             tpcd::experiment::utils::ExperimentState::kIneligible,
         .expected_bucket = tpcd::experiment::utils::Experiment3PCBlockStatus::
             kBlockedAndExperimentAllowed,
     }};

class
    NavigationMetricsRecorder3pcdExperimentVersusActualCookieStatusHistogramBrowserTest
    : public NavigationMetricsRecorderBrowserTest,
      public testing::WithParamInterface<
          ExperimentVersusActualCookieStatusHistogramBrowserTestCase> {
 public:
  NavigationMetricsRecorder3pcdExperimentVersusActualCookieStatusHistogramBrowserTest() {
    // Experiment feature param requests 3PCs blocked.
    tpcd_experiment_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{tpcd::experiment::kDisable3PCookiesName,
          test_case_.is_experiment_cookies_disabled ? "true" : "false"}});

    // When features are disabled, IsForceThirdPartyCookieBlockingEnabled will
    // return false, cookies are allowed.
    if (test_case_.is_third_party_cookies_allowed) {
      cookies_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              content_settings::features::kTrackingProtection3pcd,
              net::features::kForceThirdPartyCookieBlocking,
              net::features::kThirdPartyStoragePartitioning});
    } else {
      cookies_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {net::features::kForceThirdPartyCookieBlocking,
           net::features::kThirdPartyStoragePartitioning},
          /*disabled_features=*/{});
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    NavigationMetricsRecorderBrowserTest::SetUpOnMainThread();
  }

  void Wait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        tpcd::experiment::kDecisionDelayTime.Get());
    run_loop.Run();
  }

 protected:
  const ExperimentVersusActualCookieStatusHistogramBrowserTestCase test_case_ =
      GetParam();
  base::test::ScopedFeatureList tpcd_experiment_feature_list_;
  base::test::ScopedFeatureList cookies_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    NavigationMetricsRecorder3pcdExperimentVersusActualCookieStatusHistogramBrowserTest,
    ExperimentBlockingStatusCookieStatusComparisons) {
  Wait();
  // When TPCD Experiment Client State is eligible, experiment should be
  // active on this client.
  g_browser_process->local_state()->SetInteger(
      tpcd::experiment::prefs::kTPCDExperimentClientState,
      static_cast<int>(test_case_.is_client_eligible));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  const GURL url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  base::HistogramTester histograms;
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));

  if (test_case_.is_third_party_cookies_allowed) {
    histograms.ExpectBucketCount(
        "Navigation.MainFrame.ThirdPartyCookieBlockingEnabled",
        ThirdPartyCookieBlockState::kCookiesAllowed, 1);
  }
  histograms.ExpectBucketCount(
      tpcd::experiment::utils::Experiment3pcBlockStatusHistogramName,
      test_case_.expected_bucket, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ExperimentVersusActualCookiesAllowedHistogramBrowserTests,
    NavigationMetricsRecorder3pcdExperimentVersusActualCookieStatusHistogramBrowserTest,
    testing::ValuesIn(kTestCases));

}  // namespace
