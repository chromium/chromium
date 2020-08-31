// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_keyed_service.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/lite_video/lite_video_observer.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/lite_video_metadata.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/mojom/network_change_manager.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run, there might be two
// profiles created, and this will return the total sample count across
// profiles.
int GetTotalHistogramSamples(const base::HistogramTester& histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester& histogram_tester,
    const std::string& histogram_name,
    int count) {
  int total = 0;
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

class LiteVideoKeyedServiceDisabledBrowserTest : public InProcessBrowserTest {
 public:
  LiteVideoKeyedServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature({::features::kLiteVideo});
  }
  ~LiteVideoKeyedServiceDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LiteVideoKeyedServiceDisabledBrowserTest,
                       KeyedServiceEnabledButLiteVideoDisabled) {
  EXPECT_EQ(nullptr,
            LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));
}

class LiteVideoDataSaverDisabledBrowserTest : public InProcessBrowserTest {
 public:
  LiteVideoDataSaverDisabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kLiteVideo);
  }
  ~LiteVideoDataSaverDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LiteVideoDataSaverDisabledBrowserTest,
                       LiteVideoEnabled_DataSaverOff) {
  EXPECT_EQ(nullptr,
            LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));
}

class LiteVideoKeyedServiceBrowserTest
    : public LiteVideoKeyedServiceDisabledBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  LiteVideoKeyedServiceBrowserTest() : use_opt_guide_(GetParam()) {}
  ~LiteVideoKeyedServiceBrowserTest() override = default;

  void SetUp() override {
    if (use_opt_guide_) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{::features::kLiteVideo,
            {{"use_optimization_guide", "true"},
             {"permanent_host_blocklist", "[\"blockedhost.com\"]"},
             {"user_blocklist_opt_out_history_threshold", "1"}}},
           {optimization_guide::features::kOptimizationHints, {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          {::features::kLiteVideo},
          {{"lite_video_origin_hints", "{\"litevideo.com\": 123}"},
           {"permanent_host_blocklist", "[\"blockedhost.com\"]"},
           {"user_blocklist_opt_out_history_threshold", "1"}});
    }
    SetUpHTTPSServer();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_4G);
    SetEffectiveConnectionType(
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
    if (use_opt_guide_)
      SeedOptGuideLiteVideoHints(GURL("https://litevideo.com"));
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitch(lite_video::switches::kLiteVideoIgnoreNetworkConditions);
  }

  void SetUpHTTPSServer() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/iframe_blank.html");
    ASSERT_TRUE(https_url().SchemeIs(url::kHttpsScheme));
  }

  // Sets up public image URL hint data.
  void SeedOptGuideLiteVideoHints(const GURL& url) {
    ASSERT_TRUE(use_opt_guide_);
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide::OptimizationMetadata optimization_metadata;
    optimization_guide::proto::LiteVideoMetadata metadata;
    optimization_metadata.SetAnyMetadataForTesting(metadata);
    optimization_guide_decider->AddHintForTesting(
        url, optimization_guide::proto::LITE_VIDEO, optimization_metadata);
  }

  // Sets the effective connection type that the Network Quality Tracker will
  // report.
  void SetEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(effective_connection_type);
  }

  lite_video::LiteVideoDecider* lite_video_decider() {
    return LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile())
        ->lite_video_decider();
  }

  void WaitForBlocklistToBeLoaded() {
    EXPECT_GT(
        RetryForHistogramUntilCountReached(
            histogram_tester_, "LiteVideo.UserBlocklist.BlocklistLoaded", 1),
        0);
  }

  const base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  GURL https_url() { return https_url_; }

  bool IsUsingOptGuide() { return use_opt_guide_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL https_url_;
  base::HistogramTester histogram_tester_;
  const bool use_opt_guide_ = false;
};

INSTANTIATE_TEST_SUITE_P(UsingOptGuide,
                         LiteVideoKeyedServiceBrowserTest,
                         ::testing::Bool(),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoEnabledWithKeyedService) {
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_UnsupportedScheme) {
  WaitForBlocklistToBeLoaded();
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://testserver.com"));

  // Close the tab to flush any UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  histogram_tester()->ExpectTotalCount("LiteVideo.Navigation.HasHint", 0);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Fails occasionally on ChromeOS. http://crbug.com/1102563
#if defined(OS_CHROMEOS)
#define MAYBE_LiteVideoCanApplyLiteVideo_NoHintForHost \
  DISABLED_LiteVideoCanApplyLiteVideo_NoHintForHost
#else
#define MAYBE_LiteVideoCanApplyLiteVideo_NoHintForHost \
  LiteVideoCanApplyLiteVideo_NoHintForHost
#endif
IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       MAYBE_LiteVideoCanApplyLiteVideo_NoHintForHost) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));
  GURL navigation_url("https://testserver.com");
  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, navigation_url);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kNotAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(lite_video::LiteVideoBlocklistReason::kAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_HasHint) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);

  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  GURL navigation_url("https://litevideo.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.HintAgent.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", true,
                                         1);
  histogram_tester()->ExpectUniqueSample("LiteVideo.HintAgent.HasHint", true,
                                         1);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, navigation_url);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(lite_video::LiteVideoBlocklistReason::kAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_Reload) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  GURL url("https://testserver.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_RELOAD);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationReload, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  // Navigate to confirm that the host is blocklisted due to a reload. This
  // happens after one such navigation due to overriding the blocklist
  // parameters for testing.
  NavigateParams params_blocklisted(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params_blocklisted);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 2),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         2);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  ASSERT_EQ(2u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kNotAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(
          lite_video::LiteVideoBlocklistReason::kNavigationReload));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));

  entry = entries[1];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kNotAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(
          lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_ForwardBack) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  GURL url("https://testserver.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_FORWARD_BACK);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationForwardBack, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  // Navigate to confirm that the host is blocklisted due to the Forward-Back
  // navigation. This happens after one such navigation due to overriding the
  // blocklist parameters for testing.
  NavigateParams params_blocklisted(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params_blocklisted);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 2),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         2);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  ASSERT_EQ(2u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kNotAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(
          lite_video::LiteVideoBlocklistReason::kNavigationForwardBack));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));

  entry = entries[1];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kNotAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(
          lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       MultipleNavigationsNotBlocklisted) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  GURL url("https://litevideo.com");

  // Navigate metrics get recorded.
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.HintAgent.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", true,
                                         1);
  histogram_tester()->ExpectUniqueSample("LiteVideo.HintAgent.HasHint", true,
                                         1);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  // Navigate  again to ensure that it was not blocklisted.
  ui_test_utils::NavigateToURL(&params);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.HintAgent.HasHint", 2),
            0);
  histogram_tester()->ExpectBucketCount("LiteVideo.Navigation.HasHint", true,
                                        2);
  histogram_tester()->ExpectBucketCount("LiteVideo.HintAgent.HasHint", true, 2);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 2);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  ASSERT_EQ(2u, entries.size());
  for (auto* entry : entries) {
    ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
        static_cast<int>(lite_video::LiteVideoDecision::kAllowed));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::LiteVideo::kBlocklistReasonName,
        static_cast<int>(lite_video::LiteVideoBlocklistReason::kAllowed));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::LiteVideo::kThrottlingResultName,
        static_cast<int>(
            lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));
  }
}

// This test fails on Windows because of the backing store for the blocklist.
// LiteVideos is an Android only feature so disabling the test permananently
// for Windows.
#if defined(OS_WIN)
#define DISABLE_ON_WIN(x) DISABLED_##x
#else
#define DISABLE_ON_WIN(x) x
#endif

IN_PROC_BROWSER_TEST_P(
    LiteVideoKeyedServiceBrowserTest,
    DISABLE_ON_WIN(UserBlocklistClearedOnBrowserHistoryClear)) {
  WaitForBlocklistToBeLoaded();
  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  GURL url("https://litevideo.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_FORWARD_BACK);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationForwardBack, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  // Navigate to confirm that the host is blocklisted.
  NavigateParams params_blocklisted(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params_blocklisted);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 2),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         2);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);

  // Wipe the browser history, clearing the user blocklist.
  // This should allow LiteVideos on the next navigation.
  browser()->profile()->Wipe();
  if (IsUsingOptGuide()) {
    // Browser clear wipes the Optimization Guide hint store so
    // we need to re-seed the hints.
    SeedOptGuideLiteVideoHints(url);
  }

  EXPECT_GT(
      RetryForHistogramUntilCountReached(
          *histogram_tester(), "LiteVideo.UserBlocklist.ClearBlocklist", 1),
      0);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.UserBlocklist.ClearBlocklist", true, 1);

  ui_test_utils::NavigateToURL(&params_blocklisted);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 3),
            0);
  histogram_tester()->ExpectBucketCount("LiteVideo.Navigation.HasHint", true,
                                        1);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}

class LiteVideoNetworkConnectionBrowserTest
    : public LiteVideoKeyedServiceBrowserTest {
 public:
  LiteVideoNetworkConnectionBrowserTest() : use_opt_guide_(GetParam()) {}
  ~LiteVideoNetworkConnectionBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // This removes the network override switch.
    cmd->AppendSwitch("enable-spdy-proxy-auth");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool use_opt_guide_ = false;
};

INSTANTIATE_TEST_SUITE_P(UsingOptGuide,
                         LiteVideoNetworkConnectionBrowserTest,
                         ::testing::Bool(),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(LiteVideoNetworkConnectionBrowserTest,
                       LiteVideoCanApplyLiteVideo_NetworkNotCellular) {
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  GURL navigation_url("https://litevideo.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);
  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);

  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}

IN_PROC_BROWSER_TEST_P(
    LiteVideoNetworkConnectionBrowserTest,
    LiteVideoCanApplyLiteVideo_NetworkConnectionBelowMinECT) {
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G);

  GURL navigation_url("https://litevideo.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       LiteVideoCanApplyLiteVideo_NavigationWithSubframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), https_url());

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 2),
            2);
  histogram_tester()->ExpectBucketCount("LiteVideo.Navigation.HasHint", false,
                                        2);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester()->ExpectBucketCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntrySourceHasUrl(entry, https_url());
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kNotAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kBlocklistReasonName,
      static_cast<int>(lite_video::LiteVideoBlocklistReason::kAllowed));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingResultName,
      static_cast<int>(
          lite_video::LiteVideoThrottleResult::kThrottledWithoutStop));
}

class LiteVideoKeyedServiceCoinflipBrowserTest
    : public LiteVideoKeyedServiceBrowserTest {
 public:
  LiteVideoKeyedServiceCoinflipBrowserTest() : use_opt_guide_(GetParam()) {}
  ~LiteVideoKeyedServiceCoinflipBrowserTest() override = default;

  void SetUp() override {
    if (use_opt_guide_) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{::features::kLiteVideo,
            {{"use_optimization_guide", "true"}, {"is_coinflip_exp", "true"}}},
           {optimization_guide::features::kOptimizationHints, {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          {::features::kLiteVideo}, {{"is_coinflip_exp", "true"}});
    }
    SetUpHTTPSServer();
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool use_opt_guide_ = false;
};

INSTANTIATE_TEST_SUITE_P(UsingOptGuide,
                         LiteVideoKeyedServiceCoinflipBrowserTest,
                         ::testing::Bool(),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceCoinflipBrowserTest,
                       LiteVideoCanApplyLiteVideo_CoinflipHoldback) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      lite_video::switches::kLiteVideoForceOverrideDecision);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      lite_video::switches::kLiteVideoForceCoinflipHoldback);
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), https_url());

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 2),
            2);
  histogram_tester()->ExpectBucketCount("LiteVideo.Navigation.HasHint", true,
                                        2);

  // Close the tab to flush the UKM metrics.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::LiteVideo::kEntryName);
  // Only recording the mainframe event.
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries[0];
  // Both entries should be tied to the mainframe url.
  ukm_recorder.ExpectEntrySourceHasUrl(entry, https_url());
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::LiteVideo::kThrottlingStartDecisionName,
      static_cast<int>(lite_video::LiteVideoDecision::kHoldback));
}

IN_PROC_BROWSER_TEST_P(LiteVideoKeyedServiceBrowserTest,
                       HostPermanentlyBlocklisted) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetEffectiveConnectionType(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);

  WaitForBlocklistToBeLoaded();
  EXPECT_TRUE(
      LiteVideoKeyedServiceFactory::GetForProfile(browser()->profile()));

  GURL navigation_url("https://blockedhost.com");

  // Navigate metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), navigation_url);

  EXPECT_GT(RetryForHistogramUntilCountReached(
                *histogram_tester(), "LiteVideo.Navigation.HasHint", 1),
            0);
  histogram_tester()->ExpectUniqueSample("LiteVideo.Navigation.HasHint", false,
                                         1);
  histogram_tester()->ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester()->ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kHostPermanentlyBlocklisted, 1);
  histogram_tester()->ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
}
