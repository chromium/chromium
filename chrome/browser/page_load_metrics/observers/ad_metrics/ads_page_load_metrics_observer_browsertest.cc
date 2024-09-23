// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/page_load_metrics/browser/ads_page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
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
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

using OriginStatus = page_load_metrics::OriginStatus;
using OriginStatusWithThrottling =
    page_load_metrics::OriginStatusWithThrottling;

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

const char kCrossOriginHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";

const char kCreativeOriginHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "CreativeOriginStatus";

const char kCreativeOriginWithThrottlingHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "CreativeOriginStatusWithThrottling";

const char kAdUserActivationHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "UserActivation";

const char kPeakWindowdPercentHistogramId[] =
    "PageLoad.Clients.Ads.Cpu.FullPage.PeakWindowedPercent2";

const char kHeavyAdInterventionTypeHistogramId[] =
    "PageLoad.Clients.Ads.HeavyAds.InterventionType2";

const char kMemoryMainFrameMaxHistogramId[] =
    "PageLoad.Clients.Ads.Memory.MainFrame.Max";

const char kMemoryUpdateCountHistogramId[] =
    "PageLoad.Clients.Ads.Memory.UpdateCount";

}  // namespace

class AdsPageLoadMetricsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {}

  AdsPageLoadMetricsObserverBrowserTest(
      const AdsPageLoadMetricsObserverBrowserTest&) = delete;
  AdsPageLoadMetricsObserverBrowserTest& operator=(
      const AdsPageLoadMetricsObserverBrowserTest&) = delete;

  ~AdsPageLoadMetricsObserverBrowserTest() override {}

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging, features::kV8PerFrameMemoryMonitoring};
    std::vector<base::test::FeatureRef> disabled = {};

    scoped_feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule(
             "expensive_animation_frame.html*"),
         subresource_filter::testing::CreateSuffixRule("ad.html")});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that an embedded ad is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbedded) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/srcdoc_embedded_ad.html")));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId, page_load_metrics::OriginStatus::kSame, 1);
}

// Test that an empty embedded ad isn't reported at all.
// TODO(crbug.com/40188872): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_OriginStatusMetricEmbeddedEmpty) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/srcdoc_embedded_ad_empty.html")));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectTotalCount(kCrossOriginHistogramId, 0);
}

// Test that an ad with the same origin as the main page is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricSame) {
  // Set the frame's resource as a rule.
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html")));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId, page_load_metrics::OriginStatus::kSame, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(page_load_metrics::OriginStatus::kSame));
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricCross) {
  // Note: Cannot navigate cross-origin without dynamically generating the URL.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));
  // Note that the initial iframe is not an ad, so the metric doesn't observe
  // it initially as same origin.  However, on re-navigating to a cross
  // origin site that has an ad at its origin, the ad on that page is cross
  // origin from the original page.
  NavigateIframeToURL(web_contents(), "test",
                      embedded_test_server()->GetURL(
                          "a.com", "/ads_observer/same_origin_ad.html"));

  // Wait until all resource data updates are sent. Note that there is one more
  // than in the tests above due to the navigation to same_origin_ad.html being
  // itself made in an iframe.
  waiter->AddMinimumCompleteResourcesExpectation(5);
  waiter->Wait();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId, page_load_metrics::OriginStatus::kCross, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(page_load_metrics::OriginStatus::kCross));
}

// TODO(crbug.com/40840626): Re-enable this test
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_AverageViewportAdDensity) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int scrollbar_width =
      EvalJs(web_contents,
             "window.innerWidth - document.documentElement.clientWidth")
          .ExtractInt();

  content::RenderWidgetHostView* guest_host_view =
      web_contents->GetRenderWidgetHostView();
  gfx::Size viewport_size = guest_host_view->GetVisibleViewportSize();
  viewport_size -= gfx::Size(scrollbar_width, 0);

  // Configure the waiter to wait for the viewport rect after scrolling, and for
  // the subframe rect.
  waiter->AddMainFrameViewportRectExpectation(
      gfx::Rect(0, 5000, viewport_size.width(), viewport_size.height()));
  gfx::Rect expected_rect =
      gfx::Rect(/*x=*/0, /*y=*/4950, /*width=*/500, /*height=*/500);
  waiter->AddMainFrameIntersectionExpectation(expected_rect);

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace("let frame = createAdIframeAtRect(0, 4950, 500, 500); "
                         "frame.style.position = 'absolute'; frame.src = $1;",
                         subframe_url.spec())));

  waiter->Wait();

  gfx::Rect viewport_rect =
      gfx::Rect(0, 0, viewport_size.width(), viewport_size.height());
  gfx::Rect intersect_rect = gfx::Rect(0, 0, 500, 450);
  intersect_rect.Intersect(viewport_rect);

  int expected_final_viewport_density =
      intersect_rect.size().GetArea() * 100 / viewport_size.GetArea();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling3::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling3::
                                      kAverageViewportAdDensityName);

  EXPECT_TRUE(reported_average_viewport_density);

  // `reported_average_viewport_density` is a time averaged value and it can
  // theoretically be any value within [0, `expected_final_viewport_density`].
  EXPECT_GE(*reported_average_viewport_density, 0);
  EXPECT_LE(*reported_average_viewport_density,
            expected_final_viewport_density);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       AverageViewportAdDensity_ImageAd) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->SetMainFrameImageAdRectsExpectation();

  GURL image_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  std::string create_image_script = content::JsReplace(R"(
          const img = document.createElement('img');
          img.style.position = 'fixed';
          img.style.left = 0;
          img.style.top = 0;
          img.width = 5;
          img.height = 5;
          img.src = $1;
          document.body.appendChild(img);)",
                                                       image_url.spec());

  EXPECT_TRUE(ExecJs(web_contents, create_image_script));

  waiter->Wait();

  EXPECT_TRUE(waiter->DidObserveMainFrameImageAdRect(gfx::Rect(0, 0, 5, 5)));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling3::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling3::
                                      kAverageViewportAdDensityName);

  EXPECT_TRUE(reported_average_viewport_density);
}

// Verifies that the page ad density records the maximum value during
// a page's lifecycling by creating a large ad frame, destroying it, and
// creating a smaller iframe. The ad density recorded is the density with
// the first larger frame.
// Flaky on Lacros bots. crbug.com/1338035
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PageAdDensityRecordsPageMax DISABLED_PageAdDensityRecordsPageMax
#else
#define MAYBE_PageAdDensityRecordsPageMax PageAdDensityRecordsPageMax
#endif
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       MAYBE_PageAdDensityRecordsPageMax) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Evaluate the height and width of the page as the browser_test can
  // vary the dimensions.
  int document_height =
      EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
  int document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  // Expectation is before NavigateToUrl for this test as the expectation can be
  // met after NavigateToUrl and before the Wait.
  waiter->AddMainFrameIntersectionExpectation(
      gfx::Rect(0, 0, document_width,
                document_height));  // Initial main frame rect.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));
  waiter->Wait();
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Create a frame at 100,100 of size 200,200.
  gfx::Rect large_rect = gfx::Rect(100, 100, 200, 200);
  waiter->AddMainFrameIntersectionExpectation(large_rect);

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace(
          "let frame = createAdIframeAtRect(100, 100, 200, 200); "
          "frame.src = $1; ",
          embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"))));
  waiter->Wait();

  // Load should stop before we remove the frame.
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_TRUE(ExecJs(web_contents,
                     "let frames = document.getElementsByTagName('iframe'); "
                     "frames[0].remove(); "));
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(400, 400, 10, 10));

  // Delete the frame and create a new frame at 400,400 of size 10x10. The
  // ad density resulting from this frame is lower than the 200x200.
  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace("let frame = createAdIframeAtRect(400, 400, 10, 10); "
                         "frame.src = $1; ",
                         embedded_test_server()
                             ->GetURL("b.com", "/ads_observer/pixel.png")
                             .spec())));
  waiter->Wait();

  // Evaluate the height and width of the page as the browser_test can
  // vary the dimensions.
  document_height =
      EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
  document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  gfx::Rect document_rect = gfx::Rect(0, 0, document_width, document_height);
  large_rect.Intersect(document_rect);
  int ad_area_within_page =
      large_rect.size().GetArea();  // The area of the first larger ad iframe.
  int ad_height_within_page = large_rect.size().height();

  int page_area = document_rect.size().GetArea();
  int expected_page_density_area = ad_area_within_page * 100 / page_area;
  int expected_page_density_height =
      ad_height_within_page * 100 / document_height;

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdPageLoad::kMaxAdDensityByAreaName,
      expected_page_density_area);
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdPageLoad::kMaxAdDensityByHeightName,
      expected_page_density_height);
}

// TODO(crbug.com/40857704): Flaky on Lacros bots.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PageAdDensityMultipleFrames DISABLED_PageAdDensityMultipleFrames
#else
#define MAYBE_PageAdDensityMultipleFrames PageAdDensityMultipleFrames
#endif
// Creates multiple overlapping frames and verifies the page ad density.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       MAYBE_PageAdDensityMultipleFrames) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int document_height =
      EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
  int document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  // Expectation is before NavigateToUrl for this test as the expectation can be
  // met after NavigateToUrl and before the Wait.
  waiter->AddMainFrameIntersectionExpectation(
      gfx::Rect(0, 0, document_width,
                document_height));  // Initial main frame rect.

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));
  waiter->Wait();
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Create a frame of size 100,100 at 400,400.
  gfx::Rect rect1 = gfx::Rect(400, 400, 100, 100);
  waiter->AddMainFrameIntersectionExpectation(rect1);

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  EXPECT_TRUE(ExecJs(
      web_contents, content::JsReplace(
                        "let frame = createAdIframeAtRect(400, 400, 100, 100); "
                        "frame.src = $1",
                        embedded_test_server()
                            ->GetURL("b.com", "/ads_observer/pixel.png")
                            .spec())));

  waiter->Wait();

  // Create a frame at of size 200,200 at 450,450.
  gfx::Rect rect2 = gfx::Rect(450, 450, 200, 200);
  waiter->AddMainFrameIntersectionExpectation(rect2);
  EXPECT_TRUE(ExecJs(
      web_contents, content::JsReplace(
                        "let frame = createAdIframeAtRect(450, 450, 200, 200); "
                        "frame.src = $1",
                        embedded_test_server()
                            ->GetURL("b.com", "/ads_observer/pixel.png")
                            .spec())));
  waiter->Wait();

  // Evaluate the height and width of the page as the browser_test can
  // vary the dimensions.
  document_height =
      EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
  document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  // Calculate the ad area and height within the page.
  gfx::Rect document_rect = gfx::Rect(0, 0, document_width, document_height);
  rect1.Intersect(document_rect);
  rect2.Intersect(document_rect);
  gfx::Rect intersect12 = rect1;
  intersect12.Intersect(rect2);

  gfx::Rect union12 = rect1;
  union12.Union(rect2);

  int ad_area_within_page = rect1.size().GetArea() + rect2.size().GetArea() -
                            intersect12.size().GetArea();
  int ad_height_within_page = union12.size().height();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  int page_area = document_rect.size().GetArea();
  int expected_page_density_area = ad_area_within_page * 100 / page_area;
  int expected_page_density_height =
      ad_height_within_page * 100 / document_height;

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdPageLoad::kMaxAdDensityByAreaName,
      expected_page_density_area);
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdPageLoad::kMaxAdDensityByHeightName,
      expected_page_density_height);
}

// Creates a frame with display:none styling and verifies that it has an
// empty intersection with the main frame.
//
// TODO(crbug.com/40715497): This test is disabled due to flaky failures on
// multiple platforms.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_PageAdDensityIgnoreDisplayNoneFrame) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Evaluate the height and width of the page as the browser_test can
  // vary the dimensions.
  int document_height =
      EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
  int document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  // Expectation is before NavigateToUrl for this test as the expectation can be
  // met after NavigateToUrl and before the Wait.
  waiter->AddMainFrameIntersectionExpectation(
      gfx::Rect(0, 0, document_width,
                document_height));  // Initial main frame rect.

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));
  waiter->Wait();
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Create a frame at 100,100 of size 200,200. The expectation is an empty rect
  // as the frame is display:none and as a result has no main frame
  // intersection.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(0, 0, 0, 0));

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  EXPECT_TRUE(ExecJs(
      web_contents, content::JsReplace(
                        "let frame = createAdIframeAtRect(100, 100, 200, 200); "
                        "frame.src = $1; "
                        "frame.style.display = \"none\";",
                        embedded_test_server()
                            ->GetURL("b.com", "/ads_observer/pixel.png")
                            .spec())));

  waiter->Wait();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdPageLoad::kMaxAdDensityByAreaName, 0);
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdPageLoad::kMaxAdDensityByHeightName, 0);
}

// Each CreativeOriginStatus* browser test inputs a pointer to a frame object
// representing the frame tree path of a website with with a (possibly null)
// ad subframe, which itself may have linearly nested subframes.
// Each test then queries cross_site_iframe_factory.html with the query string
// corresponding to the frame tree path, computes the actual creative origin
// status and compares it to the expected creative origin status.
class CreativeOriginAdsPageLoadMetricsObserverBrowserTest
    : public AdsPageLoadMetricsObserverBrowserTest {
 public:
  CreativeOriginAdsPageLoadMetricsObserverBrowserTest() = default;
  ~CreativeOriginAdsPageLoadMetricsObserverBrowserTest() override = default;

  // Data structure to store the frame tree path for the root/main frame and
  // its single ad subframe (if such a child exists), as well as to keep track
  // of whether or not to render text in the frame. A child frame may have at
  // most one child frame of its own, and so forth.
  class Frame {
   public:
    Frame(std::string origin,
          std::unique_ptr<Frame> child,
          bool has_text = false,
          bool is_outside_view = false)
        : origin_(origin),
          child_(std::move(child)),
          has_text_(has_text),
          is_outside_view_(is_outside_view) {}

    ~Frame() = default;

    bool HasChild() const { return child_ != nullptr; }

    bool HasDescendantRenderingText(bool is_top_frame = true) const {
      if (!is_top_frame && has_text_ && !is_outside_view_)
        return true;

      if (!is_top_frame && is_outside_view_)
        return false;

      if (!child_)
        return false;

      return child_->HasDescendantRenderingText(false);
    }

    std::string Hostname() const { return origin_ + ".com"; }

    std::string Print(bool should_escape = false) const {
      std::vector<std::string> query_pieces = {origin_};
      if (!has_text_ && is_outside_view_)
        query_pieces.push_back("{no-text-render,out-of-view}");
      else if (!has_text_)
        query_pieces.push_back("{no-text-render}");
      else if (is_outside_view_)
        query_pieces.push_back("{out-of-view}");
      query_pieces.push_back("(");
      if (child_)
        query_pieces.push_back(child_->Print());
      query_pieces.push_back(")");
      std::string out = base::StrCat(query_pieces);
      if (should_escape)
        out = base::EscapeQueryParamValue(out, false /* use_plus */);
      return out;
    }

    std::string PrintChild(bool should_escape = false) const {
      return HasChild() ? child_->Print(should_escape) : "";
    }

   private:
    std::string origin_;
    std::unique_ptr<Frame> child_;
    bool has_text_;
    bool is_outside_view_;
  };

  // A convenience function to make frame creation less verbose.
  std::unique_ptr<Frame> MakeFrame(std::string origin,
                                   std::unique_ptr<Frame> child,
                                   bool has_text = false,
                                   bool is_outside_view = false) {
    return std::make_unique<Frame>(origin, std::move(child), has_text,
                                   is_outside_view);
  }

  void RecordCreativeOriginStatusHistograms(std::unique_ptr<Frame> frame) {
    // The file cross_site_iframe_factory.html loads URLs like:
    // http://a.com:40919/
    //   cross_site_iframe_factory.html?a{no-text-render}(b(c{no-text-render}))
    // The frame thus intended as the creative will be the only one in which
    // text renders.
    std::string ad_suffix = frame->PrintChild(true /* should_escape */);
    if (!ad_suffix.empty())
      SetRulesetToDisallowURLsWithPathSuffix(ad_suffix);
    std::string query = frame->Print();
    std::string relative_url = "/cross_site_iframe_factory.html?" + query;
    const GURL main_url(
        embedded_test_server()->GetURL(frame->Hostname(), relative_url));

    // If there is text to render in any subframe, wait until there is a first
    // contentful paint. Load some bytes in any case.
    auto waiter = CreatePageLoadMetricsTestWaiter();
    if (frame->HasDescendantRenderingText()) {
      waiter->AddSubFrameExpectation(
          page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
              kFirstContentfulPaint);
    } else if (frame->HasChild()) {
      waiter->AddSubframeDataExpectation();
    }
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
    waiter->Wait();

    // Navigate away to force the histogram recording.
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void TestCreativeOriginStatus(
      std::unique_ptr<Frame> main_frame,
      page_load_metrics::OriginStatus expected_status,
      std::optional<page_load_metrics::OriginStatusWithThrottling>
          expected_status_with_throttling) {
    base::HistogramTester histogram_tester;
    bool subframe_exists = main_frame->HasChild();

    RecordCreativeOriginStatusHistograms(std::move(main_frame));

    // Test histograms.
    if (subframe_exists) {
      histogram_tester.ExpectUniqueSample(kCreativeOriginHistogramId,
                                          expected_status, 1);
      if (expected_status_with_throttling.has_value()) {
        histogram_tester.ExpectUniqueSample(
            kCreativeOriginWithThrottlingHistogramId,
            expected_status_with_throttling.value(), 1);
      } else {
        // the CreativeOriginStatusWithThrottling histogram is best-effort,
        // and in the case where there is no content, multiple possible
        // states are valid.
        histogram_tester.ExpectTotalCount(
            kCreativeOriginWithThrottlingHistogramId, 1);
      }

    } else {
      // If no subframe exists, verify that each histogram is not set.
      histogram_tester.ExpectTotalCount(kCreativeOriginHistogramId, 0);
      histogram_tester.ExpectTotalCount(
          kCreativeOriginWithThrottlingHistogramId, 0);
    }
  }
};

// Test that an ad with same origin as the main page is same-origin.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusSame) {
  TestCreativeOriginStatus(
      MakeFrame("a", MakeFrame("a", MakeFrame("b", MakeFrame("c", nullptr)),
                               true /* has_text */)),
      OriginStatus::kSame, OriginStatusWithThrottling::kSameAndUnthrottled);
}

// Test that an ad with a different origin as the main page is cross-origin.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusCross) {
  TestCreativeOriginStatus(
      MakeFrame("a", MakeFrame("b", MakeFrame("c", nullptr), true)),
      OriginStatus::kCross, OriginStatusWithThrottling::kCrossAndUnthrottled);
}

// Test that an ad creative with the same origin as the main page,
// but nested in a cross-origin root ad frame, is same-origin.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusSameNested) {
  TestCreativeOriginStatus(
      MakeFrame("a",
                MakeFrame("b", MakeFrame("a", MakeFrame("c", nullptr), true))),
      OriginStatus::kSame, OriginStatusWithThrottling::kSameAndUnthrottled);
}

// Test that an ad creative with a different origin as the main page,
// but nested in a same-origin root ad frame, is cross-origin.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusCrossNested) {
  TestCreativeOriginStatus(
      MakeFrame("a",
                MakeFrame("a", MakeFrame("b", MakeFrame("c", nullptr), true))),
      page_load_metrics::OriginStatus::kCross,
      page_load_metrics::OriginStatusWithThrottling::kCrossAndUnthrottled);
}

// Test that an ad creative with a different origin as the main page,
// but nested two deep in a same-origin root ad frame, is cross-origin.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusCrossDoubleNested) {
  TestCreativeOriginStatus(
      MakeFrame("a",
                MakeFrame("a", MakeFrame("a", MakeFrame("b", nullptr,
                                                        true /* has_text */)))),
      OriginStatus::kCross, OriginStatusWithThrottling::kCrossAndUnthrottled);
}

// Test that if no iframe renders text, the creative origin status is
// indeterminate. The creative origin status with throttling can be
// either kUnknownAndThrottled or kUnknownAndUnthrottled in this case
// due to race conditions, as nothing is painted.
// TODO(cammie): Find a better workaround for testing COSwT in this
// edge case.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusNoCreativeDesignated) {
  TestCreativeOriginStatus(
      MakeFrame("a", MakeFrame("b", MakeFrame("c", nullptr))),
      OriginStatus::kUnknown, std::nullopt);
}

// Test that if no iframe is created, there is no histogram set.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       CreativeOriginStatusNoSubframes) {
  TestCreativeOriginStatus(MakeFrame("a", nullptr), OriginStatus::kUnknown,
                           OriginStatusWithThrottling::kUnknownAndUnthrottled);
}

// Flakily fails (crbug.com/1099758)
// Test that a throttled ad with a different origin as the main page is
// marked as throttled, with indeterminate creative origin status.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_CreativeOriginStatusWithThrottlingUnknown) {
  TestCreativeOriginStatus(
      MakeFrame("a",
                MakeFrame("b", MakeFrame("c", nullptr), true /* has_text */,
                          true /* is_outside_view */)),
      OriginStatus::kUnknown, OriginStatusWithThrottling::kUnknownAndThrottled);
}

// Disabled due to flakiness https://crbug.com/1229601
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_CreativeOriginStatusWithThrottlingNestedThrottled \
  DISABLED_CreativeOriginStatusWithThrottlingNestedThrottled
#else
#define MAYBE_CreativeOriginStatusWithThrottlingNestedThrottled \
  CreativeOriginStatusWithThrottlingNestedThrottled
#endif

// Test that an ad creative with the same origin as the main page,
// but nested in a throttled cross-origin root ad frame, is marked as
// throttled, with indeterminate creative origin status.
IN_PROC_BROWSER_TEST_F(
    CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
    MAYBE_CreativeOriginStatusWithThrottlingNestedThrottled) {
  TestCreativeOriginStatus(
      MakeFrame(
          "a",
          MakeFrame(
              "b", MakeFrame("a", MakeFrame("c", nullptr), true /* has_text */),
              false /* has_text */, true /* is_outside_view */)),
      OriginStatus::kUnknown, OriginStatusWithThrottling::kUnknownAndThrottled);
}

// Flakily fails. https://crbug.com/1099545
// Test that an ad creative with a different origin as the main page,
// but nested in a same-origin root ad frame, such that its root ad frame
// is outside the main frame but not throttled (because the root is
// same-origin), will be marked as having unknown creative origin status
// (since there will be no FCP) and being unthrottled.
IN_PROC_BROWSER_TEST_F(
    CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
    DISABLED_CreativeOriginStatusWithThrottlingNestedUnthrottled) {
  TestCreativeOriginStatus(
      MakeFrame(
          "a",
          MakeFrame(
              "a", MakeFrame("b", MakeFrame("c", nullptr), true /* has_text */),
              false /* has_text */, true /* is_outside_view */)),
      OriginStatus::kUnknown,
      OriginStatusWithThrottling::kUnknownAndUnthrottled);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       UserActivationSetOnFrame) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a second frame that will not receive activation.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "createAdFrame('/ad_tagging/ad.html', '');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "createAdFrame('/ad_tagging/ad.html', '');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for the frames resources to be loaded as we only log histograms for
  // frames that have non-zero bytes. Four resources in the main frame and one
  // favicon.
  waiter->AddMinimumCompleteResourcesExpectation(7);
  waiter->Wait();

  // Activate one frame by executing a dummy script.
  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecJs(ad_frame, no_op_script));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectBucketCount(
      kAdUserActivationHistogramId,
      page_load_metrics::UserActivationStatus::kReceivedActivation, 1);
  histogram_tester.ExpectBucketCount(
      kAdUserActivationHistogramId,
      page_load_metrics::UserActivationStatus::kNoActivation, 1);
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

// See https://crbug.com/1193885.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       UserActivationSetOnFrameAfterSameOriginActivation) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create two same-origin ad frames.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "createAdFrame('/ad_tagging/ad.html', '');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "createAdFrame('/ad_tagging/ad.html', '');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for the frames resources to be loaded as we only log histograms for
  // frames that have non-zero bytes. Four resources in the main frame and one
  // favicon.
  waiter->AddMinimumCompleteResourcesExpectation(7);
  waiter->Wait();

  // Activate one frame by executing a dummy script. This will inherently
  // activate the second frame due to same-origin visibility user activation.
  // The activation of the second frame by this heuristic should be ignored.
  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecJs(ad_frame, no_op_script));

  // Activate the other frame directly by executing a dummy script.
  content::RenderFrameHost* ad_frame_2 =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 1);
  EXPECT_TRUE(ExecJs(ad_frame_2, no_op_script));

  // Ensure both frames are marked active.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(
      kAdUserActivationHistogramId,
      page_load_metrics::UserActivationStatus::kReceivedActivation, 2);
}

// TODO(https://crbug.com/40286659): Fix this test.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_DocOverwritesNavigation DISABLED_DocOverwritesNavigation
#else
#define MAYBE_DocOverwritesNavigation DocOverwritesNavigation
#endif
// Test that a subframe that aborts (due to doc.write) doesn't cause a crash
// if it continues to load resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       MAYBE_DocOverwritesNavigation) {
  // Ensure that the previous page won't be stored in the back/forward cache, so
  // that the histogram will be recorded when the previous page is unloaded.
  // TODO(https://crbug.com/40189815): Investigate if this needs further fix.
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .GetBackForwardCache()
      .DisableForTesting(content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  content::DOMMessageQueue msg_queue(
      browser()->tab_strip_model()->GetActiveWebContents());

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/docwrite_provisional_frame.html")));
  std::string status;
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_EQ("\"loaded\"", status);

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NumResourcesName,
      3);

  // TODO(crbug.com/): We should verify that we also receive FCP for
  // frames that are loaded in this manner. Currently timing updates are not
  // sent for aborted navigations due to doc.write.
}

// Test that a blank ad subframe that is docwritten correctly reports metrics.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocWriteAboutBlankAdframe) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/docwrite_blank_frame.html")));
  waiter->AddMinimumCompleteResourcesExpectation(5);
  waiter->Wait();
  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // One favicon resource and 2 resources for each frame.
  waiter->AddMinimumCompleteResourcesExpectation(11);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // One favicon resource and 2 resources for each frame.
  waiter->AddMinimumCompleteResourcesExpectation(9);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kFrameDepthName, 2);
}

// Test that an ad frame with visible resource gets a FCP.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       FirstContentfulPaintRecorded) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddSubFrameExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                     TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/display_block_adframe.html")));

  // Wait for FirstContentfulPaint in a subframe.
  waiter->Wait();

  // Navigate away so that it records the metric.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.AdPaintTiming.NavigationToFirstContentfulPaint3",
      1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Visible.AdPaintTiming."
      "NavigationToFirstContentfulPaint3",
      1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.NonVisible.AdPaintTiming."
      "NavigationToFirstContentfulPaint3",
      0);
}

// Test that a frame without display:none is reported as visible.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       VisibleAdframeRecorded) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/display_block_adframe.html")));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/display_none_adframe.html")));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
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

// TODO(crbug.com/41439596): Investigate why setting display: none on the
// frame will cause size updates to not be received. Verify that we record the
// correct sizes for display: none iframes.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest, FramePixelSize) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/blank_with_adiframe_writer.html")));
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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Verify each UKM entry has a corresponding, unique size.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(3u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/blank_with_adiframe_writer.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a 4x4 iframe. The threshold for visibility is an area of 25 pixels
  // or more.
  ASSERT_TRUE(
      ExecJs(web_contents,
             "let frame = createAdIframe(); frame.width=4; frame.height = 4; "
             "frame.src = '/ads_observer/ad.html';"));

  // Wait for each frames resource to load so that they will have non-zero
  // bytes.
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->AddFrameSizeExpectation(gfx::Size(4, 4));
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Visible.FrameCounts.AdFrames.Total", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.NonVisible.FrameCounts.AdFrames.Total", 1, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       AdFrameRecordMediaStatusNotPlayed) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html")));

  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_MediaName,
      static_cast<int>(page_load_metrics::MediaStatus::kNotPlayed));
}

// Flaky on all platforms, http://crbug.com/972822.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_AdFrameRecordMediaStatusPlayed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ad_tagging/frame_factory.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a second frame that will not receive activation.
  EXPECT_TRUE(content::ExecJs(
      web_contents, "createAdFrame('/ad_tagging/multiple_mimes.html', 'test');",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  waiter->AddMinimumCompleteResourcesExpectation(8);
  waiter->Wait();

  // Wait for the video to autoplay in the frame.
  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  const std::string play_script =
      "var video = document.getElementsByTagName('video')[0];"
      "new Promise(resolve => {"
      "  video.onplaying = () => { resolve('true'); };"
      "  video.play();"
      "});";
  EXPECT_EQ("true", content::EvalJs(ad_frame, play_script));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_MediaName,
      static_cast<int>(page_load_metrics::MediaStatus::kPlayed));
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       SameDomainFrameCreatedByAdScript_NotRecorddedAsAd) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));

  waiter->AddSubframeDataExpectation();
  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace("createAdIframeWithSrc($1);",
                                embedded_test_server()
                                    ->GetURL("a.com", "/ads_observer/pixel.png")
                                    .spec())));
  waiter->Wait();

  // Re-navigate to record histograms.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // There should be no observed ads because the ad iframe was same domain.
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 0, 1);

  waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));

  // This time create a frame that is not same-domain.
  waiter->AddSubframeDataExpectation();
  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace("createAdIframeWithSrc($1);",
                                embedded_test_server()
                                    ->GetURL("b.com", "/ads_observer/pixel.png")
                                    .spec())));
  waiter->Wait();

  // The frame should be tagged an ad.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 1, 1);
}

IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverBrowserTest,
    FrameCreatedByAdScriptNavigatedToAllowListRule_NotRecorddedAsAd) {
  // Subdocument resources should always check allowlist rules, even if
  // there is not matching blocklist rule.
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js"),
       subresource_filter::testing::CreateAllowlistSuffixRule("xel.png")});
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));

  waiter->AddSubframeDataExpectation();
  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace("createAdIframeWithSrc($1);",
                                embedded_test_server()
                                    ->GetURL("b.com", "/ads_observer/pixel.png")
                                    .spec())));
  waiter->Wait();

  // Re-navigate to record histograms.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // There should be no observed ads because the ad iframe was navigated to an
  // allowlist rule.
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 0, 1);
}

enum class ReduceTransferSizeUpdatedIPCTestCase {
  kEnabled,
  kDisabled,
};

// This test harness does not start the test server and allows
// ControllableHttpResponses to be declared.
class AdsPageLoadMetricsObserverResourceBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<
          ReduceTransferSizeUpdatedIPCTestCase> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ReduceTransferSizeUpdatedIPCTestCase>&
          info) {
    switch (info.param) {
      case ReduceTransferSizeUpdatedIPCTestCase::kEnabled:
        return "ReduceTransferSizeUpdatedIPCEnabled";
      case ReduceTransferSizeUpdatedIPCTestCase::kDisabled:
        return "ReduceTransferSizeUpdatedIPCDisabled";
    }
  }

  AdsPageLoadMetricsObserverResourceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kAdTagging, {}},
         {subresource_filter::kAdsInterventionsEnforced, {}},
         {heavy_ad_intervention::features::kHeavyAdIntervention, {}},
         {heavy_ad_intervention::features::kHeavyAdPrivacyMitigations,
          {{"host-threshold", "3"}}}},
        {});
    if (IsReduceTransferSizeUpdatedIPCEnabled()) {
      reduce_ipc_feature_list_.InitAndEnableFeature(
          network::features::kReduceTransferSizeUpdatedIPC);
    } else {
      reduce_ipc_feature_list_.InitAndDisableFeature(
          network::features::kReduceTransferSizeUpdatedIPC);
    }
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTest() override {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
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

  // This function loads a |large_resource| and if |will_block| is set, then
  // checks to see the resource is blocked, otherwise, it uses the |waiter| to
  // wait until the resource is loaded.
  void LoadHeavyAdResourceAndWaitOrError(
      net::test_server::ControllableHttpResponse* large_resource,
      page_load_metrics::PageLoadMetricsTestWaiter* waiter,
      bool will_block) {
    // Create a frame for the large resource.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(ExecJs(web_contents,
                       "createAdFrame('/ads_observer/"
                       "ad_with_incomplete_resource.html', '');"));

    if (will_block) {
      // If we expect the resource to be blocked, load a resource large enough
      // to trigger the intervention and ensure that the navigation failed.
      content::TestNavigationObserver error_observer(
          web_contents, net::ERR_BLOCKED_BY_CLIENT);
      page_load_metrics::LoadLargeResource(
          large_resource, page_load_metrics::kMaxHeavyAdNetworkSize);
      error_observer.WaitForNavigationFinished();
      EXPECT_FALSE(error_observer.last_navigation_succeeded());
    } else {
      // Otherwise load the resource, ensuring enough bytes were loaded.
      int64_t current_network_bytes = waiter->current_network_bytes();
      page_load_metrics::LoadLargeResource(
          large_resource, page_load_metrics::kMaxHeavyAdNetworkSize);
      waiter->AddMinimumNetworkBytesExpectation(
          current_network_bytes + page_load_metrics::kMaxHeavyAdNetworkSize);
      waiter->Wait();
    }
  }

  std::unique_ptr<page_load_metrics::AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::AdsPageLoadMetricsTestWaiter>(
        web_contents);
  }

 private:
  bool IsReduceTransferSizeUpdatedIPCEnabled() const {
    return GetParam() == ReduceTransferSizeUpdatedIPCTestCase::kEnabled;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList reduce_ipc_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AdsPageLoadMetricsObserverResourceBrowserTest,
    testing::ValuesIn({ReduceTransferSizeUpdatedIPCTestCase::kDisabled,
                       ReduceTransferSizeUpdatedIPCTestCase::kEnabled}),
    AdsPageLoadMetricsObserverResourceBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResources) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));
  // Two subresources should have been reported as ads.
  waiter->AddMinimumAdResourceExpectation(2);
  waiter->Wait();
}

// Main resources for adframes are counted as ad resources.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedMainResourceAds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"createAdFrame('frame_factory.html', '');", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  // Two pages subresources should have been reported as ad. The iframe resource
  // and its three subresources should also be reported as ads.
  waiter->AddMinimumAdResourceExpectation(6);
  waiter->Wait();
}

// Subframe navigations report ad resources correctly.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedSubframeNavigationAds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"createAdFrame('frame_factory.html', 'test');", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
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
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResourceMetrics) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad.html"),
       subresource_filter::testing::CreateSuffixRule("ad_script.js")});
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
          embedded_test_server(), "/ad.html", true /*relative_url_is_prefix*/);
  auto vanilla_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/vanilla_script.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(
      content::OpenURLParams(embedded_test_server()->GetURL("/mock_page.html"),
                             content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  main_html_response->WaitForRequest();
  main_html_response->Send(page_load_metrics::kHttpOkResponseHeader);
  main_html_response->Send(
      "<html><body></body><script src=\"ad_script.js\"></script></html>");
  main_html_response->Send(std::string(1024, ' '));
  main_html_response->Done();

  ad_script_response->WaitForRequest();
  ad_script_response->Send(page_load_metrics::kHttpOkResponseHeader);
  ad_script_response->Send(
      "var iframe = document.createElement(\"iframe\");"
      "iframe.src =\"ad.html\";"
      "document.body.appendChild(iframe);");
  ad_script_response->Send(std::string(1000, ' '));
  ad_script_response->Done();

  iframe_response->WaitForRequest();
  iframe_response->Send(page_load_metrics::kHttpOkResponseHeader);
  iframe_response->Send("<html><script src=\"vanilla_script.js\"></script>");
  iframe_response->Send(std::string(2000, ' '));
  iframe_response->Send("</html>");
  iframe_response->Done();

  vanilla_script_response->WaitForRequest();
  vanilla_script_response->Send(page_load_metrics::kHttpOkResponseHeader);
  vanilla_script_response->Send(std::string(1024, ' '));
  waiter->AddMinimumNetworkBytesExpectation(5000);
  waiter->Wait();

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  // We have received 4 KB of ads, including 1 KB of mainframe ads, plus 1 KB of
  // mainframe content.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", 5, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.Ads2", 4, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.AllPages.NonAdNetworkBytes", 1, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.MainFrame.Ads.Total2", 1, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.MainFrame.Total2", 2, 1);
}

IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       IncompleteResourcesRecordedToFrameMetrics) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL(
              "/ads_observer/ad_with_incomplete_resource.html"),
          content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();
  int64_t initial_page_bytes = waiter->current_network_bytes();

  // Make the response large enough so that normal editing to the resource files
  // won't interfere with the test expectations.
  const int response_kilobytes = 64;
  const int response_bytes = response_kilobytes * 1024;

  // Ad resource will not finish loading but should be reported to metrics.
  incomplete_resource_response->WaitForRequest();
  incomplete_resource_response->Send(page_load_metrics::kHttpOkResponseHeader);
  incomplete_resource_response->Send(std::string(response_bytes, ' '));

  // Wait for the resource update to be received for the incomplete response.

  waiter->AddMinimumNetworkBytesExpectation(response_bytes);
  waiter->Wait();

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  int expected_page_kilobytes = (initial_page_bytes + response_bytes) / 1024;

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", expected_page_kilobytes,
      1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Network",
      response_kilobytes, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2",
      response_kilobytes, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Network",
      response_kilobytes, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Total2", response_kilobytes,
      1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NetworkBytesName,
      ukm::GetExponentialBucketMinForBytes(response_bytes));
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_CacheBytes2Name, 0);
}

// Verifies that the ad unloaded by the heavy ad intervention receives an
// intervention report prior to being unloaded.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

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

      window.addEventListener('pagehide', function(event) {
        observer.takeRecords().forEach(process);
        window.domAutomationController.send('END');
      });
  )";
  EXPECT_TRUE(content::ExecJs(ad_frame, report_script,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

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
// crbug.com/1189635: flaky on win and linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_HeavyAdInterventionFired_ReportsToAllChildren \
  DISABLED_HeavyAdInterventionFired_ReportsToAllChildren
#else
#define MAYBE_HeavyAdInterventionFired_ReportsToAllChildren \
  HeavyAdInterventionFired_ReportsToAllChildren
#endif
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       MAYBE_HeavyAdInterventionFired_ReportsToAllChildren) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("frame_factory.html")});
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
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern("Ad was removed*");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));

  EXPECT_TRUE(ExecJs(web_contents,
                     "createAdFrame('/ad_tagging/frame_factory.html', '');"));

  child_observer.Wait();

  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

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
  page_load_metrics::LoadLargeResource(
      large_resource.get(), page_load_metrics::kMaxHeavyAdNetworkSize);

  error_observer.WaitForNavigationFinished();

  // Every frame should get a report (ad_with_incomplete_resource.html loads two
  // frames).
  EXPECT_EQ(4u, console_observer.messages().size());
}

// Verifies that the frame is navigated to the intervention page when a
// heavy ad intervention triggers.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());

  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHeavyAdIntervention, 1);
}

class AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention
    : public AdsPageLoadMetricsObserverResourceBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention() {
    // The experiment is "on" if either intervention or reporting is active.
    feature_list_.InitWithFeatures(
        {}, {heavy_ad_intervention::features::kHeavyAdIntervention,
             heavy_ad_intervention::features::kHeavyAdInterventionWarning});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention,
    testing::ValuesIn({ReduceTransferSizeUpdatedIPCTestCase::kDisabled,
                       ReduceTransferSizeUpdatedIPCTestCase::kEnabled}),
    AdsPageLoadMetricsObserverResourceBrowserTest::DescribeParams);

// Check that when the heavy ad feature is disabled we don't navigate
// the frame.
IN_PROC_BROWSER_TEST_P(
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  // Wait for the resource update to be received for the large resource.
  waiter->AddMinimumNetworkBytesExpectation(
      page_load_metrics::kMaxHeavyAdNetworkSize);
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
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Load a resource not large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize / 2);

  // Wait for the resource update to be received for the large resource.
  waiter->AddMinimumNetworkBytesExpectation(
      page_load_metrics::kMaxHeavyAdNetworkSize / 2);
  waiter->Wait();

  histogram_tester.ExpectTotalCount(kHeavyAdInterventionTypeHistogramId, 0);

  // Verify that the trial is not activated if no heavy ads are seen.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          heavy_ad_intervention::features::kHeavyAdIntervention)
          ->trial_name()));
}

// Check that the Heavy Ad Intervention fires the correct number of times to
// protect privacy, and that after that limit is hit, the Ads Intervention
// Framework takes over for future navigations.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionBlocklistFull_InterventionBlocked) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      http_responses(4);
  for (auto& http_response : http_responses) {
    http_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/ads_observer/incomplete_resource.js",
            false /*relative_url_is_prefix*/);
  }
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a waiter for the navigation and navigate.
  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));

  // Load and block the resource. The ads intervention framework should not
  // be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[0].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Block a second resource on the page. The ads intervention framework should
  // not be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[1].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 2);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Create a new waiter for the next navigation and navigate.
  waiter = CreateAdsPageLoadMetricsTestWaiter();
  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "foo.com", "/ad_tagging/frame_factory.html?avoid_reload")));

  // Load and block the resource. The ads intervention framework should
  // be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[2].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 3);
  histogram_tester.ExpectUniqueSample(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kHeavyAdsInterventionAtHostLimit,
      1);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Allow a second resource on the page. The ads intervention shouldn't fire a
  // second time.
  LoadHeavyAdResourceAndWaitOrError(http_responses[3].get(), waiter.get(),
                                    /*will_block=*/false);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 3);
  histogram_tester.ExpectUniqueSample(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kHeavyAdsInterventionAtHostLimit,
      1);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Reset the waiter and navigate again. Check we show the Ads Intervention UI.
  waiter.reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html")));
  histogram_tester.ExpectUniqueSample(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kHeavyAdsInterventionAtHostLimit,
      1);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
}

// Verifies that the blocklist is setup correctly and the intervention triggers
// in incognito mode.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
}

// Verify that UKM metrics are recorded correctly.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       RecordedUKMMetrics) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("foo.com",
                                            "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"createAdFrame('multiple_mimes.html', 'test');", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));
  waiter->AddMinimumAggregateCpuTimeExpectation(base::Milliseconds(300));

  // Navigate the iframe to a page with a delayed rAF, waiting for it to
  // complete. Long enough to guarantee the frame client sees a cpu time
  // update. (See: LocalFrame::AddTaskTime kTaskDurationSendThreshold).
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/expensive_animation_frame.html?delay=300"));

  // Wait until we've received the cpu update and navigate away.
  waiter->Wait();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // The elapsed_time is an upper bound on the overall page time, as it runs
  // from just before to just after activation.  The task itself is guaranteed
  // to have run at least 300ms, so we can derive a minimum percent of cpu time
  // that the task should have taken.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  EXPECT_GE(elapsed_time.InMilliseconds(), 300);

  // Ensure that there is a single entry that is at least the percent specified.
  int min_percent =
      100 * 300 /
      page_load_metrics::PeakCpuAggregator::kWindowSize.InMilliseconds();
  auto samples = histogram_tester.GetAllSamples(kPeakWindowdPercentHistogramId);
  EXPECT_EQ(1u, samples.size());
  EXPECT_EQ(1, samples.front().count);
  EXPECT_LE(min_percent, samples.front().min);
}

// Test that rAF events are measured as part of the cpu metrics.
// TODO(crbug.com/40826975): Flaky on multiple platforms.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TwoRAFFramesTriggerCpuUpdates \
  DISABLED_TwoRAFFramesTriggerCpuUpdates
#else
#define MAYBE_TwoRAFFramesTriggerCpuUpdates TwoRAFFramesTriggerCpuUpdates
#endif
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       MAYBE_TwoRAFFramesTriggerCpuUpdates) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the page and set up the waiter.
  content::DOMMessageQueue message_queue(web_contents());
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Each rAF frame in two_raf_frames delays for 200ms.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/two_raf_frames.html")));

  // Wait for both RAF calls to finish
  WaitForRAF(&message_queue);
  WaitForRAF(&message_queue);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // The elapsed_time is an upper bound on the overall page time, as it runs
  // from just before to just after activation.  The task itself is guaranteed
  // to have run at least 200ms, so we can derive a minimum percent of cpu time
  // that the task should have taken.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  EXPECT_GE(elapsed_time.InMilliseconds(), 200);

  // Ensure that there is a single entry that is at least the peak windowed
  // percent of 400ms.
  int min_percent =
      100 * 400 /
      page_load_metrics::PeakCpuAggregator::kWindowSize.InMilliseconds();
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));
  waiter->AddMinimumAggregateCpuTimeExpectation(base::Milliseconds(100));

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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

// Test that cpu metrics are cumulative across subframe navigations.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       SubframeNavigate_CpuTimesCumulative) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the page and set up the waiter.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html")));

  waiter->AddMinimumAggregateCpuTimeExpectation(base::Milliseconds(300));

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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // The elapsed_time is an upper bound on the overall page time, as it runs
  // from just before to just after activation.  The tasks in aggregate are
  // guaranteed to have run for at least 300ms, so we can derive a minimum
  // percent of cpu time that the tasks should have taken.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  EXPECT_GE(elapsed_time.InMilliseconds(), 300);

  // Ensure that there is a single entry that is at least the peak windowed
  // percent of 400ms.
  int min_percent =
      100 * 300 /
      page_load_metrics::PeakCpuAggregator::kWindowSize.InMilliseconds();
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // One favicon resource and 2 resources for frames a,c,d
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Check that adframes are not included in UKM's or UMA metrics.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(0u, entries.size());
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 0);
}

// DummyMemoryObserver is a subclass of V8DetailedMemoryObserverAnySeq so
// that we can spin up a request in the AdsMemoryMeasurementBrowserTest with
// MeasurementMode::kEagerForTesting, which will make measurements available
// to the PageLoadMetricsMemoryTracker much more quickly than they would be
// otherwise.
class DummyMemoryObserver
    : public performance_manager::v8_memory::V8DetailedMemoryObserverAnySeq {
 public:
  DummyMemoryObserver() = default;
  ~DummyMemoryObserver() override = default;

  void OnV8MemoryMeasurementAvailable(
      performance_manager::RenderProcessHostId process_id,
      const performance_manager::v8_memory::V8DetailedMemoryProcessData&
          process_data,
      const performance_manager::v8_memory::V8DetailedMemoryObserverAnySeq::
          FrameDataMap& frame_data) override {}
};

class AdsMemoryMeasurementBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsMemoryMeasurementBrowserTest() = default;

  AdsMemoryMeasurementBrowserTest(const AdsMemoryMeasurementBrowserTest&) =
      delete;
  AdsMemoryMeasurementBrowserTest& operator=(
      const AdsMemoryMeasurementBrowserTest&) = delete;

  ~AdsMemoryMeasurementBrowserTest() override = default;

  void SetUp() override {
    performance_manager::v8_memory::internal::
        SetEagerMemoryMeasurementEnabledForTesting(true);
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging, features::kV8PerFrameMemoryMonitoring};
    std::vector<base::test::FeatureRef> disabled = {};
    scoped_feature_list_.InitWithFeatures(enabled, disabled);

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  std::unordered_set<content::GlobalRenderFrameHostId,
                     content::GlobalRenderFrameHostIdHasher>
  GetFrameRoutingIds() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unordered_set<content::GlobalRenderFrameHostId,
                       content::GlobalRenderFrameHostIdHasher>
        frame_routing_ids;

    web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&frame_routing_ids](content::RenderFrameHost* frame) {
          frame_routing_ids.insert(frame->GetGlobalId());
        });

    return frame_routing_ids;
  }

 private:
  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter> waiter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsMemoryMeasurementBrowserTest,
                       SingleAdFrame_MaxMemoryBytesRecorded) {
  base::HistogramTester histogram_tester;

  // Instantiate a memory request and observer to set memory measurement
  // polling parameters.
  std::unique_ptr<performance_manager::v8_memory::V8DetailedMemoryRequestAnySeq>
      memory_request = std::make_unique<
          performance_manager::v8_memory::V8DetailedMemoryRequestAnySeq>(
          base::Seconds(1),
          performance_manager::v8_memory::V8DetailedMemoryRequest::
              MeasurementMode::kEagerForTesting);
  auto memory_observer = std::make_unique<DummyMemoryObserver>();
  memory_request->AddObserver(memory_observer.get());

  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetWithRules({subresource_filter::testing::CreateSuffixRule("b()")});
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));

  // Create a waiter, navigate to the main URL, and prime the waiter with the
  // mainframe's routing ID.
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Navigate to the main URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  waiter->AddMemoryUpdateExpectation(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame()
                                         ->GetGlobalId());

  // Add any additional frame routing IDs and wait until we get positive
  // memory measurements for each frame.
  for (content::GlobalRenderFrameHostId id : GetFrameRoutingIds())
    waiter->AddMemoryUpdateExpectation(id);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount(kMemoryMainFrameMaxHistogramId, 1);
  EXPECT_GT(
      histogram_tester.GetAllSamples(kMemoryMainFrameMaxHistogramId)[0].min, 0);

  histogram_tester.ExpectTotalCount(kMemoryUpdateCountHistogramId, 1);
  EXPECT_GE(
      histogram_tester.GetAllSamples(kMemoryUpdateCountHistogramId)[0].min, 1);

  memory_request->RemoveObserver(memory_observer.get());
}

class AdsPageLoadMetricsObserverPrerenderingBrowserTest
    : public AdsPageLoadMetricsObserverBrowserTest {
 public:
  AdsPageLoadMetricsObserverPrerenderingBrowserTest() = default;
  ~AdsPageLoadMetricsObserverPrerenderingBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AdsPageLoadMetricsObserverBrowserTest::SetUpCommandLine(command_line);

    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because AdsPageLoadMetricsObserverBrowserTest also
    // uses a ScopedFeatureList and initialization order matters.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            &AdsPageLoadMetricsObserverPrerenderingBrowserTest::web_contents,
            base::Unretained(this)));
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// Test that prerendering doesn't have metrics by AdsPageLoadMetricsObserver.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverPrerenderingBrowserTest,
                       NoMetricsInPrerendering) {
  // Navigate to an initial page.
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  // Start a prerender.
  GURL prerender_url =
      embedded_test_server()->GetURL("/ads_observer/srcdoc_embedded_ad.html");
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          host_id);

  // `waiter` is created for prerendering, it counts except for the favicon
  // resource.
  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();
  waiter.reset();

  prerender_helper().CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();

  // Ensure that prerendering doesn't have metrics by
  // AdsPageLoadMetricsObserver.
  DCHECK_EQ(
      0u,
      histogram_tester.GetTotalCountsForPrefix("PageLoad.Clients.Ads.").size());

  waiter = CreatePageLoadMetricsTestWaiter();
  prerender_helper().NavigatePrimaryPage(prerender_url);
  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();

  // Navigation in a primary page has metrics by AdsPageLoadMetricsObserver.
  DCHECK_NE(
      0u,
      histogram_tester.GetTotalCountsForPrefix("PageLoad.Clients.Ads.").size());
}
