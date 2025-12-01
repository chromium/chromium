// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/byte_count.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/page_load_metrics/browser/ads_page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
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
#include "content/public/common/content_switches.h"
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
#include "net/test/embedded_test_server/request_handler_util.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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

}  // namespace

class AdsPageLoadMetricsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest() = default;

  AdsPageLoadMetricsObserverBrowserTest(
      const AdsPageLoadMetricsObserverBrowserTest&) = delete;
  AdsPageLoadMetricsObserverBrowserTest& operator=(
      const AdsPageLoadMetricsObserverBrowserTest&) = delete;

  ~AdsPageLoadMetricsObserverBrowserTest() override = default;

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging};
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
    // Ensure browser is active so that the expected dimensions are correct.
    ui_test_utils::BrowserActivationWaiter(browser()).WaitForActivation();
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
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();

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
  waiter->SetMainFrameAdRectsExpectation();

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace("let frame = createAdIframeAtRect(0, 4950, 500, 500); "
                         "frame.style.position = 'absolute'; frame.src = $1;",
                         subframe_url.spec())));

  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(
      gfx::Rect(/*x=*/0, /*y=*/4950, /*width=*/500, /*height=*/500)));

  gfx::Rect viewport_rect =
      gfx::Rect(0, 0, viewport_size.width(), viewport_size.height());
  gfx::Rect intersect_rect = gfx::Rect(0, 0, 500, 450);
  intersect_rect.Intersect(viewport_rect);

  int expected_final_viewport_density =
      intersect_rect.size().GetArea() * 100 / viewport_size.GetArea();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling4::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling4::
                                      kAverageViewportAdDensityName);

  EXPECT_TRUE(reported_average_viewport_density);

  // `reported_average_viewport_density` is a time averaged value and it can
  // theoretically be any value within [0, `expected_final_viewport_density`].
  EXPECT_GE(*reported_average_viewport_density, 0);
  EXPECT_LE(*reported_average_viewport_density,
            expected_final_viewport_density);
}

// TODO(crbug.com/431787502): Re-enable this test.
// The test seems to be flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_AverageViewportAdDensity_ImageAd) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  waiter->SetMainFrameAdRectsExpectation();

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

  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(gfx::Rect(0, 0, 5, 5)));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling4::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling4::
                                      kAverageViewportAdDensityName);

  EXPECT_TRUE(reported_average_viewport_density);
}

// TODO(crbug.com/431787502): Re-enable this test.
// The test seems to be flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverBrowserTest,
    DISABLED_AverageViewportAdDensity_SpanBackgroundImageAd) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  waiter->SetMainFrameAdRectsExpectation();

  GURL image_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  std::string create_span_script = content::JsReplace(R"(
        const span = document.createElement('span');
        span.style.position = 'fixed';
        span.style.left = 0;
        span.style.top = 0;
        span.style.width = '5px';
        span.style.height = '5px';
        span.style.display = 'block';
        span.style.backgroundImage = 'url(' + $1 + ')';
        document.body.appendChild(span);)",
                                                      image_url.spec());

  EXPECT_TRUE(ExecJs(web_contents, create_span_script));

  waiter->Wait();

  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(gfx::Rect(0, 0, 5, 5)));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling4::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling4::
                                      kAverageViewportAdDensityName);

  EXPECT_TRUE(reported_average_viewport_density);
}

// Test that viewport ad density does not accumulate for ads that are injected
// while the tab is in the background.
// TODO(crbug.com/448982399): Re-enable this test
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
#define MAYBE_AdDensity_AdCreatedInBackgroundNotAccountedWhileInBackground \
  DISABLED_AdDensity_AdCreatedInBackgroundNotAccountedWhileInBackground
#else
#define MAYBE_AdDensity_AdCreatedInBackgroundNotAccountedWhileInBackground \
  AdDensity_AdCreatedInBackgroundNotAccountedWhileInBackground
#endif
IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverBrowserTest,
    MAYBE_AdDensity_AdCreatedInBackgroundNotAccountedWhileInBackground) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  // Open a new tab, which backgrounds the original web_contents.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verify the original tab is hidden.
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents->GetVisibility());

  // In the backgrounded tab, add a full-viewport image ad.
  GURL image_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  std::string create_image_script = content::JsReplace(R"(
          const img = document.createElement('img');
          img.style.position = 'fixed';
          img.style.left = '0';
          img.style.top = '0';
          img.style.width = '100vw';
          img.style.height = '100vh';
          img.src = $1;
          document.body.appendChild(img);
      )",
                                                       image_url.spec());

  EXPECT_TRUE(ExecJs(web_contents, create_image_script));

  // Wait for 0.5 seconds. If ad density tracking were buggy and still running
  // while hidden, this delay would give it time to (incorrectly) accumulate a
  // non-zero density value.
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(500));
  loop.Run();

  // Navigate the backgrounded tab away to trigger metric recording.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));

  // Check the recorded metrics.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling4::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling4::
                                      kAverageViewportAdDensityName);

  // The density should be 0. When a tab is hidden, ad density tracking is
  // paused in the browser. Furthermore, the renderer's lifecycle update also
  // stops, so the browser wouldn't be notified of the new ad's geometry anyway.
  EXPECT_TRUE(reported_average_viewport_density);
  EXPECT_EQ(*reported_average_viewport_density, 0);
}

// Tests that viewport ad density starts to accumulate for an ad injected in a
// backgrounded tab, once that tab is shown again.
// TODO(https://crbug.com/448524935): Flaky on mac x64.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
#define MAYBE_AdDensity_AdCreatedInBackgroundAccountedWhenTabRefocused \
  DISABLED_AdDensity_AdCreatedInBackgroundAccountedWhenTabRefocused
#else
#define MAYBE_AdDensity_AdCreatedInBackgroundAccountedWhenTabRefocused \
  AdDensity_AdCreatedInBackgroundAccountedWhenTabRefocused
#endif
IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverBrowserTest,
    MAYBE_AdDensity_AdCreatedInBackgroundAccountedWhenTabRefocused) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  int original_tab_index = browser()->tab_strip_model()->active_index();

  // Open a new tab, which backgrounds the original web_contents.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verify the original tab is hidden.
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents->GetVisibility());

  // In the backgrounded tab, add a full-viewport image ad.
  GURL image_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  std::string create_image_script = content::JsReplace(R"(
          const img = document.createElement('img');
          img.style.position = 'fixed';
          img.style.left = '0';
          img.style.top = '0';
          img.style.width = '100vw';
          img.style.height = '100vh';
          img.src = $1;
          document.body.appendChild(img);
      )",
                                                       image_url.spec());

  EXPECT_TRUE(ExecJs(web_contents, create_image_script));

  // Switch back to the original tab. This should trigger the renderer to detect
  // the ad and report its geometry.
  waiter->SetMainFrameAdRectsExpectation();
  browser()->tab_strip_model()->ActivateTabAt(original_tab_index);
  waiter->Wait();

  // Wait for 0.5 seconds to allow time for ad density to accumulate now that
  // the tab is visible.
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(500));
  loop.Run();

  // Navigate the tab away to trigger metric recording.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));

  // Check the recorded metrics.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling4::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling4::
                                      kAverageViewportAdDensityName);

  // The density should be greater than 0. When the tab becomes visible again,
  // the ad's geometry is reported and density tracking resumes.
  EXPECT_TRUE(reported_average_viewport_density);
  EXPECT_GT(*reported_average_viewport_density, 0);
}

// Tests that viewport ad density does not accumulate time when the tab is
// backgrounded.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       AdDensity_BackgroundedTimeNotAccounted) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto waiter = CreatePageLoadMetricsTestWaiter();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  waiter->SetMainFrameAdRectsExpectation();

  base::ElapsedTimer timer1;

  // This variable will track the total duration the ad was present *and* the
  // tab was visible. Due to the asynchronous nature of the test, this measures
  // the *maximum potential* time, capturing the brief windows between ad
  // insertion/removal and tab visibility changes.
  base::TimeDelta max_potential_ad_visible_time;

  // In the backgrounded tab, add a full-viewport image ad.
  GURL image_url =
      embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png");

  std::string create_image_script = content::JsReplace(R"(
          const img = document.createElement('img');
          img.style.position = 'fixed';
          img.style.left = '0';
          img.style.top = '0';
          img.style.width = '100vw';
          img.style.height = '100vh';
          img.src = $1;
          document.body.appendChild(img);
      )",
                                                       image_url.spec());

  EXPECT_TRUE(ExecJs(web_contents, create_image_script));
  waiter->Wait();

  int original_tab_index = browser()->tab_strip_model()->active_index();

  // Open a new tab, which backgrounds the original web_contents.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verify the original tab is hidden.
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents->GetVisibility());

  // `max_potential_ad_visible_time` now holds the duration from ad insertion
  // until the tab was hidden.
  max_potential_ad_visible_time += timer1.Elapsed();

  // Stay backgrouneded for 1 second. The test will assert that this 1-second
  // duration, where the ad is present but backgrounded, is *not* counted
  // towards the ad density accumulation.
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), base::Seconds(1));
    loop.Run();
  }

  std::string remove_image_script =
      R"(
          const images = document.getElementsByTagName('img');
          images[0].remove();
      )";
  EXPECT_TRUE(ExecJs(web_contents, remove_image_script));

  base::ElapsedTimer timer2;

  // Switch back to the original tab, and wait for the removal event (a new,
  // empty rectangle).
  waiter->SetMainFrameAdRectsExpectation();
  browser()->tab_strip_model()->ActivateTabAt(original_tab_index);
  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(gfx::Rect(0, 0, 0, 0)));

  // There can be a small window between the tab becoming visible and the
  // browser processing the ad removal. Ad density could accumulate during
  // this brief period, so we add it to `max_potential_ad_visible_time`.
  max_potential_ad_visible_time += timer2.Elapsed();

  // Wait for 0.5 seconds while the tab is visible and the ad is gone. This adds
  // to the total visible time, which is the denominator in the average density
  // calculation.
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), base::Milliseconds(500));
    loop.Run();
  }

  // Navigate the tab away to trigger metric recording.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));

  // Check the recorded metrics.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling4::kEntryName);
  EXPECT_EQ(1u, entries.size());

  const int64_t* reported_average_viewport_density =
      ukm_recorder.GetEntryMetric(entries.front(),
                                  ukm::builders::AdPageLoadCustomSampling4::
                                      kAverageViewportAdDensityName);

  // `max_potential_ad_visible_time` should be less than 0.5 seconds in a
  // reasonably fast test environment. We only run the assertion if this
  // condition holds, to avoid flakiness on very slow bots.
  if (max_potential_ad_visible_time < base::Milliseconds(500)) {
    // The density should be less than 50%. This implies the backgrouned time
    // was not accounted in the density accumulation.
    EXPECT_TRUE(reported_average_viewport_density);
    EXPECT_LE(*reported_average_viewport_density, 50);
  }
}

// Verifies that the page ad density records the maximum value during
// a page's lifecycling by creating a large ad frame, destroying it, and
// creating a smaller iframe. The ad density recorded is the density with
// the first larger frame.
// TODO(crbug.com/443615131, crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_PageAdDensityRecordsPageMax) {
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

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  // Create a frame at 100,100 of size 200,200, with b.com as origin to not get
  // caught by restricted ad tagging.
  gfx::Rect large_rect = gfx::Rect(100, 100, 200, 200);
  waiter->SetMainFrameAdRectsExpectation();
  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace(
          R"(
          let frame = createAdIframeAtRect(100, 100, 200, 200);
          frame.src = $1;
        )",
          embedded_test_server()->GetURL("b.com", "/ads_observer/pixel.png"))));
  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(large_rect));

  // Load should stop before we remove the frame.
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Remove the frame.
  waiter->SetMainFrameAdRectsExpectation();
  EXPECT_TRUE(ExecJs(web_contents,
                     "let frames = document.getElementsByTagName('iframe'); "
                     "frames[0].remove(); "));
  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(gfx::Rect()));

  // Create a new frame at 400,400 of size 10x10. The ad density resulting from
  // this frame is lower than the 200x200.
  waiter->SetMainFrameAdRectsExpectation();
  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace("let frame = createAdIframeAtRect(400, 400, 10, 10); "
                         "frame.src = $1; ",
                         embedded_test_server()
                             ->GetURL("b.com", "/ads_observer/pixel.png")
                             .spec())));
  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(gfx::Rect(400, 400, 10, 10)));

  // Evaluate the height and width of the page as the browser_test can vary the
  // dimensions.
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

// Creates multiple overlapping frames and verifies the page ad density.
// TODO(crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_PageAdDensityMultipleFrames) {
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

  page_load_metrics::AddTextAndWaitForFirstContentfulPaint(web_contents,
                                                           waiter.get());

  // Create a frame of size 100,100 at 400,400, with b.com as origin to not get
  // caught by restricted ad tagging.
  gfx::Rect rect1 = gfx::Rect(400, 400, 100, 100);
  waiter->SetMainFrameAdRectsExpectation();
  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace(R"(
          let frame = createAdIframeAtRect(400, 400, 100, 100);
          frame.src = $1;
        )",
                                embedded_test_server()
                                    ->GetURL("b.com", "/ads_observer/pixel.png")
                                    .spec())));
  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(rect1));

  // Create a frame at of size 200,200 at 450,450.
  gfx::Rect rect2 = gfx::Rect(450, 450, 200, 200);
  waiter->SetMainFrameAdRectsExpectation();
  EXPECT_TRUE(ExecJs(
      web_contents, content::JsReplace(
                        "let frame = createAdIframeAtRect(450, 450, 200, 200); "
                        "frame.src = $1",
                        embedded_test_server()
                            ->GetURL("b.com", "/ads_observer/pixel.png")
                            .spec())));
  waiter->Wait();
  EXPECT_TRUE(waiter->DidObserveMainFrameAdRect(rect2));

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
      if (!is_top_frame && has_text_ && !is_outside_view_) {
        return true;
      }

      if (!is_top_frame && is_outside_view_) {
        return false;
      }

      if (!child_) {
        return false;
      }

      return child_->HasDescendantRenderingText(false);
    }

    std::string Hostname() const { return origin_ + ".com"; }

    std::string Print(bool should_escape = false) const {
      std::vector<std::string> query_pieces = {origin_};
      if (!has_text_ && is_outside_view_) {
        query_pieces.push_back("{no-text-render,out-of-view}");
      } else if (!has_text_) {
        query_pieces.push_back("{no-text-render}");
      } else if (is_outside_view_) {
        query_pieces.push_back("{out-of-view}");
      }
      query_pieces.push_back("(");
      if (child_) {
        query_pieces.push_back(child_->Print());
      }
      query_pieces.push_back(")");
      std::string out = base::StrCat(query_pieces);
      if (should_escape) {
        out = base::EscapeQueryParamValue(out, false /* use_plus */);
      }
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
    if (!ad_suffix.empty()) {
      SetRulesetToDisallowURLsWithPathSuffix(ad_suffix);
    }
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
// TODO(crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_CreativeOriginStatusSameNested) {
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
// TODO(crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(CreativeOriginAdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_CreativeOriginStatusNoSubframes) {
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

// TODO(crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_UserActivationSetOnFrame) {
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
// TODO(crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverBrowserTest,
    DISABLED_UserActivationSetOnFrameAfterSameOriginActivation) {
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

// This test harness does not start the test server and allows
// ControllableHttpResponses to be declared.
class AdsPageLoadMetricsObserverResourceBrowserTestBase
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  static std::string DescribeParams(const testing::TestParamInfo<bool>& info) {
    return info.param ? "ReduceTransferSizeUpdatedIPCEnabled"
                      : "ReduceTransferSizeUpdatedIPCDisabled";
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTestBase() override = default;

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
    scoped_feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                       GetDisabledFeatures());

    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    // Required for web bluetooth.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  virtual bool IsReduceTransferSizeUpdatedIPCEnabled() const { return false; }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const {
    std::vector<base::test::FeatureRefAndParams> enabled{
        {subresource_filter::kAdTagging, {}},
        {subresource_filter::kAdsInterventionsEnforced, {}},
        {heavy_ad_intervention::features::kHeavyAdIntervention, {}},
        {heavy_ad_intervention::features::
             kHeavyAdInterventionSendReportToEmbedder,
         {}},
        {heavy_ad_intervention::features::kHeavyAdPrivacyMitigations,
         {{"host-threshold", "3"}}}};
    if (IsReduceTransferSizeUpdatedIPCEnabled()) {
      enabled.push_back({network::features::kReduceTransferSizeUpdatedIPC, {}});
    }
    return enabled;
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() const {
    std::vector<base::test::FeatureRef> disabled;
    if (!IsReduceTransferSizeUpdatedIPCEnabled()) {
      disabled.push_back(network::features::kReduceTransferSizeUpdatedIPC);
    }
    return disabled;
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
      base::ByteCount current_network_bytes = waiter->current_network_bytes();
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AdsPageLoadMetricsObserverResourceBrowserTest
    : public AdsPageLoadMetricsObserverResourceBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  bool IsReduceTransferSizeUpdatedIPCEnabled() const override {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AdsPageLoadMetricsObserverResourceBrowserTest,
    testing::Bool(),
    &AdsPageLoadMetricsObserverResourceBrowserTest::DescribeParams);

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

// Verify that privacy sensitive permissions policy use counters are recorded
// correctly when ad script is in the stack.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedPrivacyPermissionsUseCounters) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_script.js")});
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          true /*relative_url_is_prefix*/);
  auto ad_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ad_script.js",
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
  // Get ad script to use a bunch of privacy sensitive features.
  ad_script_response->Send(R"(
        navigator.bluetooth.requestDevice().catch(e => {});
        navigator.geolocation.getCurrentPosition(() => {});
        navigator.mediaDevices.getUserMedia({video: true});
        navigator.mediaDevices.getDisplayMedia().catch(() => {});
        navigator.mediaDevices.getUserMedia({audio: true});
        navigator.serial.requestPort().catch(() => {});
        navigator.usb.requestDevice({ filters: [] }).catch(() => {});
  )");
  ad_script_response->Send(std::string(5000, ' '));
  ad_script_response->Done();

  waiter->AddMinimumNetworkBytesExpectation(base::ByteCount(5000));

  std::vector<network::mojom::PermissionsPolicyFeature> features = {
      network::mojom::PermissionsPolicyFeature::kBluetooth,
      network::mojom::PermissionsPolicyFeature::kCamera,
      network::mojom::PermissionsPolicyFeature::kDisplayCapture,
      network::mojom::PermissionsPolicyFeature::kGeolocation,
      network::mojom::PermissionsPolicyFeature::kMicrophone,
      network::mojom::PermissionsPolicyFeature::kSerial,
      network::mojom::PermissionsPolicyFeature::kUsb};

  for (auto feature : features) {
    waiter->AddUseCounterFeatureExpectation(
        {blink::mojom::UseCounterFeatureType::
             kPermissionsPolicyEnabledPrivacySensitive,
         static_cast<blink::UseCounterFeature::EnumValue>(feature)});
  }

  waiter->Wait();
  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.PermissionsPolicy.PrivacySensitive.Enabled", features.size());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Permissions_PrivacySensitive_UseCounter::kEntryName);
  EXPECT_EQ(features.size(), entries.size());
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
  ad_script_response->Send(R"(
      var iframe = document.createElement("iframe");
      iframe.src ="ad.html";
      document.body.appendChild(iframe);
  )");
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
  waiter->AddMinimumNetworkBytesExpectation(base::ByteCount(5000));
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
  base::ByteCount initial_page_bytes = waiter->current_network_bytes();

  // Make the response large enough so that normal editing to the resource files
  // won't interfere with the test expectations.
  const base::ByteCount response_size = base::KiB(64);

  // Ad resource will not finish loading but should be reported to metrics.
  incomplete_resource_response->WaitForRequest();
  incomplete_resource_response->Send(page_load_metrics::kHttpOkResponseHeader);
  incomplete_resource_response->Send(std::string(response_size.InBytes(), ' '));

  // Wait for the resource update to be received for the incomplete response.

  waiter->AddMinimumNetworkBytesExpectation(response_size);
  waiter->Wait();

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  base::ByteCount expected_page_size = initial_page_bytes + response_size;

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", expected_page_size.InKiB(),
      1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Network",
      response_size.InKiB(), 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2",
      response_size.InKiB(), 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Network",
      response_size.InKiB(), 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Total2",
      response_size.InKiB(), 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NetworkBytesName,
      ukm::GetExponentialBucketMinForBytes(response_size.InBytes()));
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
    if (message == "\"END\"") {
      break;
    }
  }
  EXPECT_TRUE(got_report);
}

IN_PROC_BROWSER_TEST_P(
    AdsPageLoadMetricsObserverResourceBrowserTest,
    HeavyAdInterventionFired_ReportSentToEmbedderOfSameOriginIframe) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/ad_with_incomplete_resource.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::DOMMessageQueue message_queue(web_contents);

  const std::string report_script = R"(
      function process(report) {
        if (report.body.id === 'HeavyAdIntervention') {
          window.domAutomationController.send(report.body.message);
        }
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
  EXPECT_TRUE(content::ExecJs(web_contents, report_script));

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  std::string report_message;
  EXPECT_TRUE(message_queue.WaitForMessage(&report_message));

  EXPECT_THAT(report_message, testing::HasSubstr("Ad was removed"));

  // The intervention report should identify the subframe using an empty
  // identifier, as the iframe lacks 'id' and 'src' attributes (e.g., created
  // via 'srcdoc').
  EXPECT_THAT(report_message, testing::HasSubstr("(id=;url=)"));
}

IN_PROC_BROWSER_TEST_P(
    AdsPageLoadMetricsObserverResourceBrowserTest,
    HeavyAdInterventionFired_ReportSentToEmbedderOfCrossOriginIframe) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/ads_observer/blank_with_adiframe_writer.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL redirect_to_url = embedded_test_server()->GetURL(
      "c.com", "/ads_observer/doc_with_incomplete_resource.html");

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair(
      "{{REDIRECT_HEADER}}", "Location: " + redirect_to_url.spec()));

  GURL redirect_from_url_without_fragment = embedded_test_server()->GetURL(
      "b.com", net::test_server::GetFilePathWithReplacements(
                   "/ads_observer/redirect_from.html", replacement));

  GURL redirect_from_url =
      GURL(redirect_from_url_without_fragment.spec() + "#abc");

  content::TestNavigationObserver child_observer(web_contents);
  EXPECT_TRUE(ExecJs(web_contents, content::JsReplace(R"(
          createAdIframeWithSrc($1, /*id=*/123);
      )",
                                                      redirect_from_url)));
  child_observer.WaitForNavigationFinished();
  EXPECT_TRUE(child_observer.last_navigation_succeeded());

  content::DOMMessageQueue message_queue(web_contents);

  const std::string report_script = R"(
      function process(report) {
        if (report.body.id === 'HeavyAdIntervention') {
          window.domAutomationController.send(report.body.message);
        }
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
  EXPECT_TRUE(content::ExecJs(web_contents, report_script));

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  std::string report_message;
  EXPECT_TRUE(message_queue.WaitForMessage(&report_message));

  EXPECT_THAT(report_message, testing::HasSubstr("Ad was removed"));

  // The intervention report should identify the subframe using the frame
  // element's `id` and its initial URL (pre-redirect). The URL is sanitized by
  // removing the fragment.
  EXPECT_THAT(report_message, testing::HasSubstr("id=123"));
  EXPECT_THAT(
      report_message,
      testing::HasSubstr("url=" + redirect_from_url_without_fragment.spec()));
  EXPECT_THAT(report_message,
              testing::Not(testing::HasSubstr(redirect_from_url.spec())));
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

  // The embedder frame and every child frame should get a report
  // (ad_with_incomplete_resource.html loads two frames).
  EXPECT_EQ(5u, console_observer.messages().size());
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
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    // Override and do not enable any features.
    return {};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    // Override and only disable heavy ad intervention features.
    return {heavy_ad_intervention::features::kHeavyAdIntervention,
            heavy_ad_intervention::features::kHeavyAdInterventionWarning};
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AdsPageLoadMetricsObserverResourceBrowserTestWithoutHeavyAdIntervention,
    testing::Bool(),
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

// TODO(crbug.com/361671258): Flaky on multiple platforms
IN_PROC_BROWSER_TEST_P(
    AdsPageLoadMetricsObserverResourceBrowserTest,
    DISABLED_HeavyAdInterventionBlocklistFull_InterventionBlocked) {
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

#if BUILDFLAG(ENABLE_PDF)

class AdsPageLoadMetricsObserverRecordedUKMMetricsTest
    : public AdsPageLoadMetricsObserverResourceBrowserTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    std::string reduce_ipc_description =
        std::get<0>(info.param) ? "ReduceTransferSizeUpdatedIPCEnabled"
                                : "ReduceTransferSizeUpdatedIPCDisabled";
    std::string oopif_pdf_description =
        std::get<1>(info.param) ? "_OopifPdf" : "_GuestViewPdf";
    return base::StrCat({reduce_ipc_description, "_", oopif_pdf_description});
  }

  bool IsReduceTransferSizeUpdatedIPCEnabled() const override {
    return std::get<0>(GetParam());
  }

  bool UseOopif() const { return std::get<1>(GetParam()); }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled =
        AdsPageLoadMetricsObserverResourceBrowserTestBase::GetEnabledFeatures();
    if (UseOopif()) {
      enabled.push_back({chrome_pdf::features::kPdfOopif, {}});
    }
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    auto disabled = AdsPageLoadMetricsObserverResourceBrowserTestBase::
        GetDisabledFeatures();
    if (!UseOopif()) {
      disabled.push_back(chrome_pdf::features::kPdfOopif);
    }
    return disabled;
  }
};

// Verify that UKM metrics are recorded correctly.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverRecordedUKMMetricsTest,
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
  const int kExpectedNumAdResources = UseOopif() ? 9 : 8;
  waiter->AddMinimumAdResourceExpectation(kExpectedNumAdResources);
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

// TODO(crbug.com/324635792): Stop testing GuestView PDF viewer after OOPIF PDF
// viewer launches.
INSTANTIATE_TEST_SUITE_P(
    All,
    AdsPageLoadMetricsObserverRecordedUKMMetricsTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    &AdsPageLoadMetricsObserverRecordedUKMMetricsTest::DescribeParams);

#endif  // BUILDFLAG(ENABLE_PDF)

void WaitForRAF(content::DOMMessageQueue* message_queue) {
  std::string message;
  while (message_queue->WaitForMessage(&message)) {
    if (message == "\"RAF DONE\"") {
      break;
    }
  }
  EXPECT_EQ("\"RAF DONE\"", message);
}

// Test that rAF events are measured as part of the cpu metrics.
// TODO(crbug.com/402536429): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_FrameRAFTriggersCpuUpdate) {
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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
// TODO(https://crbug.com/448524935): Flaky on mac x64.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
#define MAYBE_AggregateCpuTriggersCpuUpdateOverSubframeNavigate \
  DISABLED_AggregateCpuTriggersCpuUpdateOverSubframeNavigate
#else
#define MAYBE_AggregateCpuTriggersCpuUpdateOverSubframeNavigate \
  AggregateCpuTriggersCpuUpdateOverSubframeNavigate
#endif
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       MAYBE_AggregateCpuTriggersCpuUpdateOverSubframeNavigate) {
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

struct SurfaceTestCase {
  const char* name;
  const char* script;
};

class AdsPageLoadMetricsObserverSurfaceBrowserTest
    : public AdsPageLoadMetricsObserverResourceBrowserTestBase,
      public ::testing::WithParamInterface<std::tuple<SurfaceTestCase, bool>> {
 public:
  bool IsReduceTransferSizeUpdatedIPCEnabled() const override { return false; }
};

// The ad script requests an image via various methods. We
// check to make sure that regardless of method, the image is still tagged as an
// ad. This is basically ensuring that the top-of-stack frame check works in
// each scenario.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverSurfaceBrowserTest,
                       DetectedAsAd) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_script.js")});

  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          /*relative_url_is_prefix=*/true);

  auto ad_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ad_script.js",
          /*relative_url_is_prefix=*/true);

  auto image_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/image.gif",
          /*relative_url_is_prefix=*/true);

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
  main_html_response->Done();

  const char* script_format = std::get<0>(GetParam()).script;
  const bool lambda = std::get<1>(GetParam());

  std::string script;

  // Either load the image from an ad-script-defined function
  // or from an ad-script-created lambda.
  if (lambda) {
    script = base::ReplaceStringPlaceholders(
        script_format,
        {R"SCRIPT((() => document.createElement("img").src = "image.gif"))SCRIPT"},
        /*offsets=*/nullptr);
  } else {
    script =
        R"SCRIPT(function loadImg() { document.createElement("img").src = "image.gif"; })SCRIPT";

    script += base::ReplaceStringPlaceholders(script_format, {"top.loadImg"},
                                              /*offsets=*/nullptr);
  }

  ad_script_response->WaitForRequest();
  ad_script_response->Send(page_load_metrics::kHttpOkResponseHeader);
  ad_script_response->Send(script);
  ad_script_response->Done();

  image_response->WaitForRequest();
  image_response->Send(page_load_metrics::kHttpOkResponseHeader);
  image_response->Done();

  // Two subresources should have been reported as ads.
  waiter->AddMinimumAdResourceExpectation(2);
  waiter->Wait();
}

constexpr SurfaceTestCase kSurfaceTestCases[] = {
    {"direct_call", "$1();"},
    {"setTimeout", "setTimeout($1, 1);"},
    {"event_handler", R"SCRIPT(
      const i = document.createElement("img");
      i.onerror = $1;
      i.src = "http://";
    )SCRIPT"},
    {"postMessage", R"SCRIPT(
      window.onmessage = $1;
      window.postMessage("", "*");
    )SCRIPT"},
    {"eval", "eval('$1();');"},
    {"promise_constructor", "new Promise($1);"},
    {"promise_resolve", "Promise.resolve().then($1);"},
    {"promise_reject", "Promise.reject().catch($1);"},
    {"queueMicrotask", "queueMicrotask($1);"},
    {"postTask", "scheduler.postTask($1);"},
    {"dynamic_script_tag", R"SCRIPT(
      const s = document.createElement("script");
      s.innerText = '$1();';
      document.body.appendChild(s);
    )SCRIPT"},
    {"dynamic_script_tag_data", R"SCRIPT(
      const s = document.createElement("script");
      s.src = 'data:text/javascript,$1();';
      document.body.appendChild(s);
    )SCRIPT"},
    {"dynamic_frame", R"SCRIPT(
      const i = document.createElement("iframe");
      i.srcdoc = '<script>$1();</script>';
      document.body.appendChild(i);
    )SCRIPT"},
    {"requestIdleCallback", "requestIdleCallback($1);"},
    {"requestAnimationFrame", "requestAnimationFrame($1);"},
    {"dynamic_script_tag_blob", R"SCRIPT(
      const b = new Blob(['$1();'], {
        type: "text/javascript",
      });
      const s = document.createElement("script");
      s.src = URL.createObjectURL(b);
      document.body.appendChild(s);
    )SCRIPT"},
    {"dynamic_script_tag_module_data", R"SCRIPT(
      const s = document.createElement("script");
      s.type = "module";
      s.src = 'data:text/javascript,$1();';
      document.body.appendChild(s);
    )SCRIPT"},
    {"dynamic_script_tag_module", R"SCRIPT(
      const s = document.createElement("script");
      s.type = "module";
      s.innerText = '$1();';
      document.body.appendChild(s);
    )SCRIPT"},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AdsPageLoadMetricsObserverSurfaceBrowserTest,
    ::testing::Combine(::testing::ValuesIn(kSurfaceTestCases),
                       ::testing::Bool()),
    [](const ::testing::TestParamInfo<std::tuple<SurfaceTestCase, bool>>&
           info) {
      return base::StrCat({std::get<0>(info.param).name,
                           std::get<1>(info.param) ? "_lambda" : "_func"});
    });
