// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kCrossOriginHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";

const char kAdUserActivationHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "UserActivation";

const char kSqrtNumberOfPixelsHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "SqrtNumberOfPixels";

const char kSmallestDimensionHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "SmallestDimension";

const char kPeakWindowdPercentHistogramId[] =
    "PageLoad.Clients.Ads.Cpu.FullPage.PeakWindowedPercent";

const char kHeavyAdInterventionTypeHistogramId[] =
    "PageLoad.Clients.Ads.HeavyAds.InterventionType2";

const char kHttpOkResponseHeader[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";

// Use the maximum possible threshold so tests are deterministic.
const int kMaxHeavyAdNetworkSize =
    heavy_ad_thresholds::kMaxNetworkBytes +
    AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider::
        kMaxNetworkThresholdNoiseBytes;

void LoadLargeResource(net::test_server::ControllableHttpResponse* response,
                       int bytes) {
  response->WaitForRequest();
  response->Send(kHttpOkResponseHeader);
  response->Send(std::string(bytes, ' '));
  response->Done();
}

}  // namespace

class AdsPageLoadMetricsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {}
  ~AdsPageLoadMetricsObserverBrowserTest() override {}

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  void SetUp() override {
    std::vector<base::Feature> enabled = {subresource_filter::kAdTagging,
                                          features::kSitePerProcess};
    std::vector<base::Feature> disabled = {};

    if (use_process_priority_) {
      enabled.push_back(features::kUseFramePriorityInRenderProcessHost);
    } else {
      disabled.push_back(features::kUseFramePriorityInRenderProcessHost);
    }
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule(
             "expensive_animation_frame.html*")});
  }

 protected:
  bool use_process_priority_ = false;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverBrowserTest);
};

// Test that an embedded ad is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbedded) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/srcdoc_embedded_ad.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kCrossOriginHistogramId,
                                      FrameData::OriginStatus::kSame, 1);
}

// Test that an empty embedded ad isn't reported at all.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbeddedEmpty) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/srcdoc_embedded_ad_empty.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectTotalCount(kCrossOriginHistogramId, 0);
}

// Test that an ad with the same origin as the main page is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricSame) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kCrossOriginHistogramId,
                                      FrameData::OriginStatus::kSame, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(FrameData::OriginStatus::kSame));
}

// Test that an ad with a different origin as the main page is cross origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricCross) {
  // Note: Cannot navigate cross-origin without dynamically generating the URL.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  // Note that the initial iframe is not an ad, so the metric doesn't observe
  // it initially as same origin.  However, on re-navigating to a cross
  // origin site that has an ad at its origin, the ad on that page is cross
  // origin from the original page.
  NavigateIframeToURL(web_contents(), "test",
                      embedded_test_server()->GetURL(
                          "a.com", "/ads_observer/same_origin_ad.html"));

  // Wait until all resource data updates are sent.
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kCrossOriginHistogramId,
                                      FrameData::OriginStatus::kCross, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(FrameData::OriginStatus::kCross));
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       UserActivationSetOnFrame) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a second frame that will not receive activation.
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents, "createAdFrame('/ad_tagging/frame_factory.html', '');"));
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents, "createAdFrame('/ad_tagging/frame_factory.html', '');"));

  // Wait for the frames resources to be loaded as we only log histograms for
  // frames that have non-zero bytes. Four resources per frame and one favicon.
  waiter->AddMinimumCompleteResourcesExpectation(13);
  waiter->Wait();

  // Activate one frame by executing a dummy script.
  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecuteScript(ad_frame, no_op_script));

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectBucketCount(
      kAdUserActivationHistogramId,
      FrameData::UserActivationStatus::kReceivedActivation, 1);
  histogram_tester.ExpectBucketCount(
      kAdUserActivationHistogramId,
      FrameData::UserActivationStatus::kNoActivation, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());

  // Verify that one ad was reported to be activated and the other was not.
  EXPECT_TRUE(*ukm_recorder.GetEntryMetric(
                  entries.front(),
                  ukm::builders::AdFrameLoad::kStatus_UserActivationName) !=
              *ukm_recorder.GetEntryMetric(
                  entries.back(),
                  ukm::builders::AdFrameLoad::kStatus_UserActivationName));
}

// Test that a subframe that aborts (due to doc.write) doesn't cause a crash
// if it continues to load resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocOverwritesNavigation) {
  content::DOMMessageQueue msg_queue;

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/docwrite_provisional_frame.html"));
  std::string status;
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_EQ("\"loaded\"", status);

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // TODO(johnidel): Check that the subresources of the new frame are reported
  // correctly. Resources from a failed provisional load are not reported to
  // resource data updates, causing this adframe to not be recorded. This is an
  // uncommon case but should be reported. See crbug.com/914893.
}

// Test that a blank ad subframe that is docwritten correctly reports metrics.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocWriteAboutBlankAdframe) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "/ads_observer/docwrite_blank_frame.html"));
  waiter->AddMinimumCompleteResourcesExpectation(5);
  waiter->Wait();
  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2", 0 /* < 1 KB */,
      1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(),
                ukm::builders::AdFrameLoad::kLoading_NetworkBytesName),
            0);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       SubresourceFilter) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetToDisallowURLsWithPathSuffix("b()");
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b,c,d)"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(), main_url);

  // One favicon resource and 2 resources for each frame.
  waiter->AddMinimumCompleteResourcesExpectation(11);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 2, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest, FrameDepth) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetToDisallowURLsWithPathSuffix("b()))");
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b(b())))"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(), main_url);

  // One favicon resource and 2 resources for each frame.
  waiter->AddMinimumCompleteResourcesExpectation(9);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kFrameDepthName, 2);
}

// Test that a frame without display:none is reported as visible.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       VisibleAdframeRecorded) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "/ads_observer/display_block_adframe.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Visible.Bytes.AdFrames.PerFrame.Total2", 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Total2", 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.NonVisible.Bytes.AdFrames.PerFrame.Total2", 0);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kVisibility_HiddenName,
      false);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DisplayNoneAdframeRecorded) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "/ads_observer/display_none_adframe.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.NonVisible.Bytes.AdFrames.PerFrame.Total2", 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Total2", 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Visible.Bytes.AdFrames.PerFrame.Total2", 0);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kVisibility_HiddenName,
      true);
}

// TODO(https://crbug.com/929136): Investigate why setting display: none on the
// frame will cause size updates to not be received. Verify that we record the
// correct sizes for display: none iframes.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest, FramePixelSize) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/blank_with_adiframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::map<std::pair<int, int>, int> expected_dimension_counts;
  std::map<std::pair<int, int>, int> expected_bucketed_dimension_counts;
  expected_dimension_counts[std::make_pair(100, 100)] = 1;
  expected_dimension_counts[std::make_pair(0, 0)] = 1;
  expected_dimension_counts[std::make_pair(10, 1000)] = 1;

  for (auto const& dimension_and_count : expected_dimension_counts) {
    // Convert the expected dimensions into exponential buckets.
    expected_bucketed_dimension_counts[std::make_pair(
        ukm::GetExponentialBucketMinForCounts1000(
            dimension_and_count.first.first),
        ukm::GetExponentialBucketMinForCounts1000(
            dimension_and_count.first.second))] = 1;
    // Create an iframe with the given dimensions.
    ASSERT_TRUE(
        ExecJs(web_contents,
               "let frame = createAdIframe(); frame.width=" +
                   base::NumberToString(dimension_and_count.first.first) +
                   "; frame.height = " +
                   base::NumberToString(dimension_and_count.first.second) +
                   "; "
                   "frame.src = '/ads_observer/pixel.png';"));
  }

  // Wait for each frames resource to load so that they will have non-zero
  // bytes.
  waiter->AddMinimumCompleteResourcesExpectation(6);
  waiter->AddFrameSizeExpectation(gfx::Size(0, 0));
  waiter->AddFrameSizeExpectation(gfx::Size(10, 1000));
  waiter->AddFrameSizeExpectation(gfx::Size(100, 100));
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectBucketCount(kSqrtNumberOfPixelsHistogramId, 100, 2);
  histogram_tester.ExpectBucketCount(kSqrtNumberOfPixelsHistogramId, 0, 1);
  histogram_tester.ExpectBucketCount(kSmallestDimensionHistogramId, 0, 1);
  histogram_tester.ExpectBucketCount(kSmallestDimensionHistogramId, 10, 1);
  histogram_tester.ExpectBucketCount(kSmallestDimensionHistogramId, 100, 1);

  // Verify each UKM entry has a corresponding, unique size.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(3u, entries.size());
  for (auto* const entry : entries) {
    auto dimension = std::make_pair(
        *ukm_recorder.GetEntryMetric(
            entry, ukm::builders::AdFrameLoad::kVisibility_FrameWidthName),
        *ukm_recorder.GetEntryMetric(
            entry, ukm::builders::AdFrameLoad::kVisibility_FrameHeightName));
    EXPECT_EQ(1u, expected_bucketed_dimension_counts.erase(dimension));
  }
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       FrameWithSmallAreaNotConsideredVisible) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/blank_with_adiframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a 4x4 iframe. The threshold for visibility is an area of 25 pixels
  // or more.
  ASSERT_TRUE(
      ExecJs(web_contents,
             "let frame = createAdIframe(); frame.width=4; frame.height = 4; "
             "frame.src = '/ads_observer/pixel.png';"));

  // Wait for each frames resource to load so that they will have non-zero
  // bytes.
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->AddFrameSizeExpectation(gfx::Size(4, 4));
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Visible.FrameCounts.AdFrames.PerFrame."
      "SmallestDimension",
      0);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.NonVisible.FrameCounts.AdFrames.PerFrame."
      "SmallestDimension",
      4, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       AdFrameSameOriginByteMetrics) {
  base::HistogramTester histogram_tester;

  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetWithRules({subresource_filter::testing::CreateSuffixRule("b()))"),
                       subresource_filter::testing::CreateSuffixRule("e())")});
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(),d(b())),e(e,e()))"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(), main_url);

  // One favicon resource and 2 resources for each frame.
  waiter->AddMinimumCompleteResourcesExpectation(17);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Verify that iframe e is only same origin.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.PercentSameOrigin2", 100,
      1);

  // Verify that iframe b counts subframes as cross origin and a nested same
  // origin subframe as same origin.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.PercentSameOrigin2", 50, 1);

  // Verify that all iframe are treated as cross-origin to the page. Only 1/8 of
  // resources are on origin a.com.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.PercentSameOrigin2", 12.5, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       AdFrameRecordMediaStatusNotPlayed) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html"));

  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_MediaName,
      static_cast<int>(FrameData::MediaStatus::kNotPlayed));
}

// Flaky on all platforms, http://crbug.com/972822.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_AdFrameRecordMediaStatusPlayed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ad_tagging/frame_factory.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a second frame that will not receive activation.
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents,
      "createAdFrame('/ad_tagging/multiple_mimes.html', 'test');"));

  waiter->AddMinimumCompleteResourcesExpectation(8);
  waiter->Wait();

  // Wait for the video to autoplay in the frame.
  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  const std::string play_script =
      "var video = document.getElementsByTagName('video')[0];"
      "video.onplaying = () => { "
      "window.domAutomationController.send('true'); };"
      "video.play();";
  EXPECT_EQ("true", content::EvalJsWithManualReply(ad_frame, play_script));

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_MediaName,
      static_cast<int>(FrameData::MediaStatus::kPlayed));
}

class AdsPageLoadMetricsTestWaiter
    : public page_load_metrics::PageLoadMetricsTestWaiter {
 public:
  explicit AdsPageLoadMetricsTestWaiter(content::WebContents* web_contents)
      : page_load_metrics::PageLoadMetricsTestWaiter(web_contents) {}
  void AddMinimumAdResourceExpectation(int num_ad_resources) {
    expected_minimum_ad_resources_ = num_ad_resources;
  }

 protected:
  bool ExpectationsSatisfied() const override {
    return complete_ad_resources_ >= expected_minimum_ad_resources_ &&
           PageLoadMetricsTestWaiter::ExpectationsSatisfied();
  }

  void HandleResourceUpdate(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource)
      override {
    if (resource->reported_as_ad_resource && resource->is_complete)
      complete_ad_resources_++;
  }

 private:
  int complete_ad_resources_ = 0;
  int expected_minimum_ad_resources_ = 0;
};

// This test harness does not start the test server and allows
// ControllableHttpResponses to be declared.
class AdsPageLoadMetricsObserverResourceBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kAdTagging, {}},
         {features::kHeavyAdIntervention, {}},
         {features::kHeavyAdPrivacyMitigations, {{"host-threshold", "1"}}}},
        {});
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTest() override {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js"),
         subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

 protected:
  std::unique_ptr<AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<AdsPageLoadMetricsTestWaiter>(web_contents);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResources) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));
  // Two subresources should have been reported as ads.
  waiter->AddMinimumAdResourceExpectation(2);
  waiter->Wait();
}

// Main resources for adframes are counted as ad resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedMainResourceAds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('frame_factory.html', '');"),
      base::NullCallback());
  // Two pages subresources should have been reported as ad. The iframe resource
  // and its three subresources should also be reported as ads.
  waiter->AddMinimumAdResourceExpectation(6);
  waiter->Wait();
}

// Subframe navigations report ad resources correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedSubframeNavigationAds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('frame_factory.html', 'test');"),
      base::NullCallback());
  waiter->AddMinimumAdResourceExpectation(6);
  waiter->Wait();
  NavigateIframeToURL(web_contents(), "test",
                      embedded_test_server()->GetURL(
                          "foo.com", "/ad_tagging/frame_factory.html"));
  // The new subframe and its three subresources should be reported
  // as ads.
  waiter->AddMinimumAdResourceExpectation(10);
  waiter->Wait();
}

// Verify that per-resource metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResourceMetrics) {
  base::HistogramTester histogram_tester;

  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          true /*relative_url_is_prefix*/);
  auto ad_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ad_script.js",
          true /*relative_url_is_prefix*/);
  auto iframe_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/iframe.html",
          true /*relative_url_is_prefix*/);
  auto vanilla_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/vanilla_script.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/mock_page.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  main_html_response->WaitForRequest();
  main_html_response->Send(kHttpOkResponseHeader);
  main_html_response->Send(
      "<html><body></body><script src=\"ad_script.js\"></script></html>");
  main_html_response->Send(std::string(1024, ' '));
  main_html_response->Done();

  ad_script_response->WaitForRequest();
  ad_script_response->Send(kHttpOkResponseHeader);
  ad_script_response->Send(
      "var iframe = document.createElement(\"iframe\");"
      "iframe.src =\"iframe.html\";"
      "document.body.appendChild(iframe);");
  ad_script_response->Send(std::string(1000, ' '));
  ad_script_response->Done();

  iframe_response->WaitForRequest();
  iframe_response->Send(kHttpOkResponseHeader);
  iframe_response->Send("<html><script src=\"vanilla_script.js\"></script>");
  iframe_response->Send(std::string(2000, ' '));
  iframe_response->Send("</html>");
  iframe_response->Done();

  vanilla_script_response->WaitForRequest();
  vanilla_script_response->Send(kHttpOkResponseHeader);
  vanilla_script_response->Send(std::string(1024, ' '));
  waiter->AddMinimumNetworkBytesExpectation(5000);
  waiter->Wait();

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", 5, 1);
  // We have received 4 KB of ads and 1 KB of mainframe ads.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.Ads2", 4, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.MainFrame.Ads.Total2", 1, 1);

  // The main frame should have 2 KB of resources, 1KB from the main resource
  // and one from the ad script in the main frame.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.MainFrame.Total2", 2, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       IncompleteResourcesRecordedToFrameMetrics) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL(
          "/ads_observer/ad_with_incomplete_resource.html"),
      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));

  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();
  int64_t initial_page_bytes = waiter->current_network_bytes();

  // Ad resource will not finish loading but should be reported to metrics.
  incomplete_resource_response->WaitForRequest();
  incomplete_resource_response->Send(kHttpOkResponseHeader);
  incomplete_resource_response->Send(std::string(2048, ' '));

  // Wait for the resource update to be received for the incomplete response.
  waiter->AddMinimumNetworkBytesExpectation(2048);
  waiter->Wait();

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  int expected_page_kilobytes = (initial_page_bytes + 2048) / 1024;

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", expected_page_kilobytes,
      1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Network", 2, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2", 2, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Network", 2, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Total2", 2, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NetworkBytesName,
      ukm::GetExponentialBucketMinForBytes(2048));
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_CacheBytes2Name, 0);
}

// Verifies that the ad unloaded by the heavy ad intervention receives an
// intervention report prior to being unloaded.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionFired_ReportSent) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  content::DOMMessageQueue message_queue(ad_frame);

  const std::string report_script = R"(
      function process(report) {
        if (report.body.id === 'HeavyAdIntervention')
          window.domAutomationController.send('REPORT');
      }

      let observer = new ReportingObserver((reports, observer) => {
        reports.forEach(process);
      });
      observer.observe();

      window.addEventListener('unload', function(event) {
        observer.takeRecords().forEach(process);
        window.domAutomationController.send('END');
      });
  )";
  EXPECT_TRUE(content::ExecJs(ad_frame, report_script,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(incomplete_resource_response.get(), kMaxHeavyAdNetworkSize);

  std::string message;
  bool got_report = false;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"REPORT\"") {
      got_report = true;
      break;
    }
    if (message == "\"END\"")
      break;
  }
  EXPECT_TRUE(got_report);
}

// Verifies that reports are sent to all children.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionFired_ReportsToAllChildren) {
  base::HistogramTester histogram_tester;
  auto large_resource =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          false /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver child_observer(web_contents, 2);
  content::TestNavigationObserver error_observer(web_contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  auto console_delegate = std::make_unique<content::ConsoleObserverDelegate>(
      web_contents, "Ad was removed*");
  web_contents->SetDelegate(console_delegate.get());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));

  EXPECT_TRUE(ExecJs(web_contents,
                     "createAdFrame('/ad_tagging/frame_factory.html', '');"));

  child_observer.Wait();

  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  auto cross_origin_ad_url = embedded_test_server()->GetURL(
      "xyz.com", "/ad_tagging/frame_factory.html");

  EXPECT_TRUE(ExecJs(
      ad_frame,
      "createAdFrame('/ads_observer/ad_with_incomplete_resource.html', '');",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(ExecJs(ad_frame,
                     "createAdFrame('" + cross_origin_ad_url.spec() + "', '');",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(large_resource.get(), kMaxHeavyAdNetworkSize);

  error_observer.WaitForNavigationFinished();

  // Every frame should get a report (ad_with_incomplete_resource.html loads two
  // frames).
  EXPECT_EQ(4u, console_delegate->messages().size());
}

// Verifies that the frame is navigated to the intervention page when a
// heavy ad intervention triggers.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionEnabled_ErrorPageLoaded) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver error_observer(web_contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(incomplete_resource_response.get(), kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  histogram_tester.ExpectUniqueSample(kHeavyAdInterventionTypeHistogramId,
                                      FrameData::HeavyAdStatus::kNetwork, 1);

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());

  histogram_tester.ExpectUniqueSample(kHeavyAdInterventionTypeHistogramId,
                                      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHeavyAdIntervention, 1);
}

class AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention
    : public AdsPageLoadMetricsObserverResourceBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention() {
    feature_list_.InitAndDisableFeature(features::kHeavyAdIntervention);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that when the heavy ad feature is disabled we don't navigate
// the frame.
IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention,
    ErrorPageNotLoaded) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(incomplete_resource_response.get(), kMaxHeavyAdNetworkSize);

  // Wait for the resource update to be received for the large resource.
  waiter->AddMinimumNetworkBytesExpectation(kMaxHeavyAdNetworkSize);
  waiter->Wait();

  // We can't check whether the navigation didn't occur because the error page
  // load is not synchronous. Instead check that we didn't log intervention UMA
  // that is always recorded when the intervention occurs.
  histogram_tester.ExpectTotalCount(kHeavyAdInterventionTypeHistogramId, 0);

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHeavyAdIntervention, 0);
}

// Check that we don't activate a HeavyAdIntervention field trial if we don't
// have a heavy ad.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionNoHeavyAd_FieldTrialNotActive) {
  base::HistogramTester histogram_tester;

  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Load a resource not large enough to trigger the intervention.
  LoadLargeResource(incomplete_resource_response.get(),
                    kMaxHeavyAdNetworkSize / 2);

  // Wait for the resource update to be received for the large resource.
  waiter->AddMinimumNetworkBytesExpectation(kMaxHeavyAdNetworkSize / 2);
  waiter->Wait();

  histogram_tester.ExpectTotalCount(kHeavyAdInterventionTypeHistogramId, 0);

  // Verify that the trial is not activated if no heavy ads are seen.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kHeavyAdIntervention)
          ->trial_name()));
}

// Verifies that when the blocklist is at threshold, the heavy ad intervention
// does not trigger.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionBlocklistFull_InterventionBlocked) {
  base::HistogramTester histogram_tester;
  auto large_resource_1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          false /*relative_url_is_prefix*/);
  auto large_resource_2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          false /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver error_observer(web_contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));

  EXPECT_TRUE(ExecJs(
      web_contents,
      "createAdFrame('/ads_observer/ad_with_incomplete_resource.html', '');"));

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(large_resource_1.get(), kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  histogram_tester.ExpectUniqueSample(kHeavyAdInterventionTypeHistogramId,
                                      FrameData::HeavyAdStatus::kNetwork, 1);

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());

  histogram_tester.ExpectUniqueSample(kHeavyAdInterventionTypeHistogramId,
                                      FrameData::HeavyAdStatus::kNetwork, 1);

  EXPECT_TRUE(ExecJs(
      web_contents,
      "createAdFrame('/ads_observer/ad_with_incomplete_resource.html', '');"));

  // Use the current network bytes because the ad could have been unloaded
  // before loading the entire large resource.
  int64_t current_network_bytes = waiter->current_network_bytes();

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(large_resource_2.get(), kMaxHeavyAdNetworkSize);
  waiter->AddMinimumNetworkBytesExpectation(current_network_bytes +
                                            kMaxHeavyAdNetworkSize);
  waiter->Wait();

  // Check that the intervention did not trigger on this frame.
  histogram_tester.ExpectUniqueSample(kHeavyAdInterventionTypeHistogramId,
                                      FrameData::HeavyAdStatus::kNetwork, 1);
}

// Verifies that the blocklist is setup correctly and the intervention triggers
// in incognito mode.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionIncognitoMode_InterventionFired) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  Browser* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::TestNavigationObserver error_observer(web_contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);

  // Create a waiter for the incognito contents.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents);
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  ui_test_utils::NavigateToURL(incognito_browser, url);

  // Load a resource large enough to trigger the intervention.
  LoadLargeResource(incomplete_resource_response.get(), kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
}

// Verify that UKM metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       RecordedUKMMetrics) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("foo.com",
                                            "/ad_tagging/frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), url);
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('multiple_mimes.html', 'test');"),
      base::NullCallback());
  waiter->AddMinimumAdResourceExpectation(8);
  waiter->Wait();

  // Close all tabs to report metrics.
  browser()->tab_strip_model()->CloseAllTabs();

  // Verify UKM Metrics recorded.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.front(), url);
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdBytesName),
            0);
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kMainframeAdBytesName),
      0);
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdBytesPerSecondName),
      0);

  // TTI is not reached by this page and thus should not have this recorded.
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entries.front(),
      ukm::builders::AdPageLoad::kAdBytesPerSecondAfterInteractiveName));
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdJavascriptBytesName),
      0);
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdVideoBytesName),
            0);
}

void WaitForRAF(content::DOMMessageQueue* message_queue) {
  std::string message;
  while (message_queue->WaitForMessage(&message)) {
    if (message == "\"RAF DONE\"")
      break;
  }
  EXPECT_EQ("\"RAF DONE\"", message);
}

// Test that rAF events are measured as part of the cpu metrics.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       FrameRAFTriggersCpuUpdate) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the page and set up the waiter.
  base::TimeTicks start_time = base::TimeTicks::Now();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  waiter->AddMinimumAggregateCpuTimeExpectation(
      base::TimeDelta::FromMilliseconds(300));

  // Navigate the iframe to a page with a delayed rAF, waiting for it to
  // complete. Long enough to guarantee the frame client sees a cpu time
  // update. (See: LocalFrame::AddTaskTime kTaskDurationSendThreshold).
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=300"));

  // Wait until we've received the cpu update and navigate away.
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // The elapsed_time is an upper bound on the overall page time, as it runs
  // from just before to just after activation.  The task itself is guaranteed
  // to have run at least 300ms, so we can derive a minimum percent of cpu time
  // that the task should have taken.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  EXPECT_GE(elapsed_time.InMilliseconds(), 300);

  // Ensure that there is a single entry that is at least the percent specified.
  int min_percent = 100 * 300 / FrameData::kCpuWindowSize.InMilliseconds();
  auto samples = histogram_tester.GetAllSamples(kPeakWindowdPercentHistogramId);
  EXPECT_EQ(1u, samples.size());
  EXPECT_EQ(1, samples.front().count);
  EXPECT_LE(min_percent, samples.front().min);
}

// Test that rAF events are measured as part of the cpu metrics.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       TwoRAFFramesTriggerCpuUpdates) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the page and set up the waiter.
  content::DOMMessageQueue message_queue(web_contents());
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Each rAF frame in two_raf_frames delays for 200ms.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/two_raf_frames.html"));

  // Wait for both RAF calls to finish
  WaitForRAF(&message_queue);
  WaitForRAF(&message_queue);
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // The elapsed_time is an upper bound on the overall page time, as it runs
  // from just before to just after activation.  The task itself is guaranteed
  // to have run at least 200ms, so we can derive a minimum percent of cpu time
  // that the task should have taken.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  EXPECT_GE(elapsed_time.InMilliseconds(), 200);

  // Ensure that there is a single entry that is at least the peak windowed
  // percent of 400ms.
  int min_percent = 100 * 400 / FrameData::kCpuWindowSize.InMilliseconds();
  auto samples = histogram_tester.GetAllSamples(kPeakWindowdPercentHistogramId);
  EXPECT_EQ(1u, samples.size());
  EXPECT_EQ(1, samples.front().count);
  EXPECT_LE(min_percent, samples.front().min);
}

// Test that cpu time aggregation across a subframe navigation is cumulative.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       AggregateCpuTriggersCpuUpdateOverSubframeNavigate) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the page and set up the waiter.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  waiter->AddMinimumAggregateCpuTimeExpectation(
      base::TimeDelta::FromMilliseconds(100));

  // Navigate twice to a page delaying 50ms.  The first and second navigations
  // by themselves aren't enough to trigger a cpu update, but when combined an
  // update fires. (See: LocalFrame::AddTaskTime kTaskDurationSendThreshold).
  // Navigate the first time, waiting for it to complete so that the work is
  // observed, then renavigate.
  content::DOMMessageQueue message_queue(web_contents());
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=50"));
  WaitForRAF(&message_queue);
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=50"));

  // Wait until we've received the cpu update and navigate away. If CPU is
  // not cumulative, this hangs waiting for a CPU update indefinitely.
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
}

// Test that cpu metrics are cumulative across subframe navigations.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       SubframeNavigate_CpuTimesCumulative) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the page and set up the waiter.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));

  waiter->AddMinimumAggregateCpuTimeExpectation(
      base::TimeDelta::FromMilliseconds(300));

  // Navigate twice to a page with enough cumulative time to measure
  // at least 1% peak windowed percent (300ms), either individually leads to 0
  // % peak windowed percent.
  base::TimeTicks start_time = base::TimeTicks::Now();
  content::DOMMessageQueue message_queue(web_contents());
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=200"));
  WaitForRAF(&message_queue);
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=100"));

  // Wait until we've received the cpu update and navigate away.
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // The elapsed_time is an upper bound on the overall page time, as it runs
  // from just before to just after activation.  The tasks in aggregate are
  // guaranteed to have run for at least 300ms, so we can derive a minimum
  // percent of cpu time that the tasks should have taken.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  EXPECT_GE(elapsed_time.InMilliseconds(), 300);

  // Ensure that there is a single entry that is at least the peak windowed
  // percent of 400ms.
  int min_percent = 100 * 300 / FrameData::kCpuWindowSize.InMilliseconds();
  auto samples = histogram_tester.GetAllSamples(kPeakWindowdPercentHistogramId);
  EXPECT_EQ(1u, samples.size());
  EXPECT_EQ(1, samples.front().count);
  EXPECT_LE(min_percent, samples.front().min);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DisallowedAdFrames_NotMeasured) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ALL_SITES));

  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetToDisallowURLsWithPathSuffix("b()");
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b,c,d)"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(), main_url);

  // One favicon resource and 2 resources for frames a,c,d
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Check that adframes are not included in UKM's or UMA metrics.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(0u, entries.size());
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 0);
}

IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverBrowserTest,
    RenderProcessHostNotBackgroundedWhenFramePriorityDisabled) {
  // Used for assignment during testing.
  auto frame1_pred = base::BindRepeating(&content::FrameMatchesName, "iframe1");
  auto frame2_pred = base::BindRepeating(&content::FrameMatchesName, "iframe2");

  // Navigate to a page with an iframe.  Make sure the two frames share a
  // process and that process is not low priority.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/two_iframes_blank.html"));
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  content::RenderFrameHost* frame1 =
      content::FrameMatchingPredicate(web_contents(), frame1_pred);
  content::RenderFrameHost* frame2 =
      content::FrameMatchingPredicate(web_contents(), frame2_pred);
  EXPECT_EQ(main_frame->GetProcess(), frame1->GetProcess());
  EXPECT_EQ(main_frame->GetProcess(), frame2->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());

  // Navigate iframe1 to a cross-origin non-ad frame.  It should be on a
  // different process, but still not be low priority.
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL("a.com", "/iframe_blank.html"));
  frame1 = content::FrameMatchingPredicate(web_contents(), frame1_pred);
  EXPECT_NE(main_frame->GetProcess(), frame1->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_FALSE(frame1->GetProcess()->IsProcessBackgrounded());

  // Navigate iframe1 to an ad on its current domain.  It should have the same
  // process host but because the feature is turned off, not be low priority.
  content::DOMMessageQueue message_queue1(web_contents());
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=0"));
  WaitForRAF(&message_queue1);
  EXPECT_EQ(frame1->GetProcess(),
            content::FrameMatchingPredicate(web_contents(), frame1_pred)
                ->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_FALSE(frame1->GetProcess()->IsProcessBackgrounded());
}

class AdsPageLoadMetricsObserverWithBackgroundingBrowserTest
    : public AdsPageLoadMetricsObserverBrowserTest {
 public:
  AdsPageLoadMetricsObserverWithBackgroundingBrowserTest()
      : AdsPageLoadMetricsObserverBrowserTest() {
    use_process_priority_ = true;
  }
  ~AdsPageLoadMetricsObserverWithBackgroundingBrowserTest() override {}
};

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverWithBackgroundingBrowserTest,
                       RenderProcessHostBackgroundedForAd) {
  // Used for assignment during testing.
  auto frame1_pred = base::BindRepeating(&content::FrameMatchesName, "iframe1");
  auto frame2_pred = base::BindRepeating(&content::FrameMatchesName, "iframe2");

  // Navigate to a page with an iframe.  Make sure the two frames share a
  // process and that process is not low priority.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/two_iframes_blank.html"));
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  content::RenderFrameHost* frame1 =
      content::FrameMatchingPredicate(web_contents(), frame1_pred);
  content::RenderFrameHost* frame2 =
      content::FrameMatchingPredicate(web_contents(), frame2_pred);
  EXPECT_EQ(main_frame->GetProcess(), frame1->GetProcess());
  EXPECT_EQ(main_frame->GetProcess(), frame2->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());

  // Navigate iframe1 to a cross-origin non-ad frame.  It should be on a
  // different process, but still not be low priority.
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL("a.com", "/iframe_blank.html"));
  frame1 = content::FrameMatchingPredicate(web_contents(), frame1_pred);
  EXPECT_NE(main_frame->GetProcess(), frame1->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_FALSE(frame1->GetProcess()->IsProcessBackgrounded());

  // Navigate iframe1 to an ad on its current domain.  It should have the
  // same process host but now be low priority.
  content::DOMMessageQueue message_queue1(web_contents());
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=0"));
  WaitForRAF(&message_queue1);
  EXPECT_EQ(frame1->GetProcess(),
            content::FrameMatchingPredicate(web_contents(), frame1_pred)
                ->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_TRUE(frame1->GetProcess()->IsProcessBackgrounded());

  // Navigate the iframe2 to a non-ad on the same domain as iframe1.  Make sure
  // that they get assigned the same process and that it's not low priority.
  NavigateIframeToURL(
      web_contents(), "iframe2",
      embedded_test_server()->GetURL("a.com", "/iframe_blank.html"));
  frame2 = content::FrameMatchingPredicate(web_contents(), frame2_pred);
  EXPECT_EQ(frame1->GetProcess(), frame2->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_FALSE(frame1->GetProcess()->IsProcessBackgrounded());

  // Delete iframe2, make sure that iframe1 is now low priority, as it now only
  // has ads assigned to it.
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents(),
      "var frame = document.getElementById('iframe2'); "
      "frame.parentNode.removeChild(frame);"));
  EXPECT_TRUE(frame1->GetProcess()->IsProcessBackgrounded());

  // Navigate the subframe to a non-ad on its current domain.  Even though this
  // is a non-ad, the frame is still identified as an ad because it was
  // previously identified as an ad by SubresourceFilterThrottle.
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL("a.com", "/iframe_blank.html"));
  EXPECT_EQ(frame1->GetProcess(),
            content::FrameMatchingPredicate(web_contents(), frame1_pred)
                ->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_TRUE(frame1->GetProcess()->IsProcessBackgrounded());

  // Navigate the subframe to an ad on the original domain.  It should now have
  // the same process as the main frame, but not be low priority because it
  // shares a process with a non-ad frame (the main frame).
  content::DOMMessageQueue message_queue2(web_contents());
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL(
          "/ads_observer/expensive_animation_frame.html?delay=0"));
  WaitForRAF(&message_queue2);
  frame1 = content::FrameMatchingPredicate(web_contents(), frame1_pred);
  EXPECT_EQ(main_frame->GetProcess(), frame1->GetProcess());
  EXPECT_FALSE(frame1->GetProcess()->IsProcessBackgrounded());

  // Navigate the subframe to a non-ad on a different domain.  Even though this
  // is a non-ad, the frame is still identified as an ad because it was
  // previously identified as an ad by SubresourceFilterThrottle.
  NavigateIframeToURL(
      web_contents(), "iframe1",
      embedded_test_server()->GetURL("b.com", "/iframe_blank.html"));
  frame1 = content::FrameMatchingPredicate(web_contents(), frame1_pred);
  EXPECT_NE(main_frame->GetProcess(), frame1->GetProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessBackgrounded());
  EXPECT_TRUE(frame1->GetProcess()->IsProcessBackgrounded());
}
