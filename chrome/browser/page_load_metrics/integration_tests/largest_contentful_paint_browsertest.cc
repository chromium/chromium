// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif
#include "ui/events/test/event_generator.h"

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
// `gn check` doesn't recognize that this is included conditionally, with the
// same condition as the dependencies.
#include "components/paint_preview/browser/paint_preview_client.h"  // nogncheck
#endif

using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

namespace {

void ValidateTraceEventHasCorrectCandidateSize(int expected_size,
                                               const TraceEvent& event) {
  ASSERT_TRUE(event.HasDictArg("data"));
  base::Value::Dict data = event.GetKnownArgAsDict("data");

  const std::optional<int> traced_size = data.FindInt("size");
  ASSERT_TRUE(traced_size.has_value());
  EXPECT_EQ(traced_size.value(), expected_size);

  const std::optional<bool> traced_main_frame_flag =
      data.FindBool("isMainFrame");
  ASSERT_TRUE(traced_main_frame_flag.has_value());
  EXPECT_TRUE(traced_main_frame_flag.value());
}

void ValidateTraceEventBreakdownTimings(const TraceEvent& event,
                                        double lcp_time) {
  ASSERT_TRUE(event.HasDictArg("data"));
  base::Value::Dict data = event.GetKnownArgAsDict("data");

  const std::optional<double> load_start = data.FindDouble("imageLoadStart");
  ASSERT_TRUE(load_start.has_value());

  const std::optional<double> discovery_time =
      data.FindDouble("imageDiscoveryTime");
  ASSERT_TRUE(discovery_time.has_value());

  const std::optional<double> load_end = data.FindDouble("imageLoadEnd");
  ASSERT_TRUE(load_end.has_value());

  // Verify image discovery time < load start < load end < lcp time;
  EXPECT_LT(discovery_time.value(), load_start.value());

  EXPECT_LT(load_start.value(), load_end.value());

  EXPECT_LT(load_end.value(), lcp_time);
}

int GetCandidateIndex(const TraceEvent& event) {
  base::Value::Dict data = event.GetKnownArgAsDict("data");
  std::optional<int> candidate_idx = data.FindInt("candidateIndex");
  DCHECK(candidate_idx.has_value()) << "couldn't find 'candidateIndex'";

  return candidate_idx.value();
}

bool compare_candidate_index(const TraceEvent* lhs, const TraceEvent* rhs) {
  return GetCandidateIndex(*lhs) < GetCandidateIndex(*rhs);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, LargestContentfulPaint) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  Start();
  StartTracing({"loading"});
  Load("/largest_contentful_paint.html");

  // The test harness serves files from something like http://example.com:34777
  // but the port number can vary. Extract the 'window.origin' property so we
  // can compare encountered URLs to expected values.
  const std::string window_origin =
      EvalJs(web_contents(), "window.origin").ExtractString();
  const std::string image_1_url_expected =
      base::StrCat({window_origin, "/images/lcp-16x16.png"});
  const std::string image_2_url_expected =
      base::StrCat({window_origin, "/images/lcp-96x96.png"});
  const std::string image_3_url_expected =
      base::StrCat({window_origin, "/images/lcp-256x256.png"});
  const std::string expected_url[3] = {
      image_1_url_expected, image_2_url_expected, image_3_url_expected};

  // Verify that the JS API yielded three LCP reports. Note that, as we resolve
  // https://github.com/WICG/largest-contentful-paint/issues/41, this test may
  // need to be updated to reflect new semantics.
  const std::string test_name[3] = {"test_first_image()", "test_larger_image()",
                                    "test_largest_image()"};
  std::optional<double> lcp_timestamps[3];
  for (size_t i = 0; i < 3; i++) {
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);

    // The AddMinimumLargestContentfulPaintImageExpectation method adds an
    // expected number of actual LCP updates that do change the LCP candidate
    // value.
    // For example, when i is 1, which is the second test, there are 2 images
    // added. The first added image updates the LCP candidate value once. The
    // second added, because it is larger than the first added, it should also
    // update the LCP candidate value. Therefore we should see 2 LCP updates
    // before the test waiter can exit the waiting.
    waiter->AddMinimumLargestContentfulPaintImageExpectation(1);

    content::EvalJsResult result = EvalJs(web_contents(), test_name[i]);
    EXPECT_EQ("", result.error);

    const auto& list = result.value.GetList();
    EXPECT_EQ(1u, list.size());
    ASSERT_TRUE(list[0].is_dict());

    const std::string* url = list[0].GetDict().FindString("url");
    EXPECT_TRUE(url);
    EXPECT_EQ(*url, expected_url[i]);

    lcp_timestamps[i] = list[0].GetDict().FindDouble("time");
    EXPECT_TRUE(lcp_timestamps[i].has_value());

    waiter->Wait();
  }

  EXPECT_LT(lcp_timestamps[0], lcp_timestamps[1])
      << "The first LCP report should be before the second";
  EXPECT_LT(lcp_timestamps[1], lcp_timestamps[2])
      << "The second LCP report should be before the third";

  // Need to navigate away from the test html page to force metrics to get
  // flushed/synced.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check Trace Events.
  std::unique_ptr<TraceAnalyzer> trace_analyzer = StopTracingAndAnalyze();
  TraceEventVector candidate_events;
  trace_analyzer->FindEvents(
      Query::EventNameIs("largestContentfulPaint::Candidate"),
      &candidate_events);
  EXPECT_EQ(3ul, candidate_events.size());
  std::sort(candidate_events.begin(), candidate_events.end(),
            compare_candidate_index);

  // LCP_0 uses lcp-16x16.png, of size 16 x 16.
  ValidateTraceEventHasCorrectCandidateSize(16 * 16, *candidate_events[0]);
  // LCP_1 uses lcp-96x96.png, of size 96 x 96.
  ValidateTraceEventHasCorrectCandidateSize(96 * 96, *candidate_events[1]);
  // LCP_2 uses lcp-256x256.png, of size 16 x 16.
  ValidateTraceEventHasCorrectCandidateSize(256 * 256, *candidate_events[2]);

  ValidateTraceEventBreakdownTimings(*candidate_events[0],
                                     lcp_timestamps[0].value());

  ValidateTraceEventBreakdownTimings(*candidate_events[1],
                                     lcp_timestamps[1].value());

  ValidateTraceEventBreakdownTimings(*candidate_events[2],
                                     lcp_timestamps[2].value());

  ExpectMetricInLastUKMUpdateTraceEventNear(
      *trace_analyzer, "latest_largest_contentful_paint_ms",
      lcp_timestamps[2].value(), 1.2);

  // Check UKM.
  // Since UKM rounds to an integer while the JS API returns a coarsened double,
  // we'll assert that the UKM and JS values are within 1.2 of each other.
  // Comparing with strict equality could round incorrectly and introduce
  // flakiness into the test.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
      lcp_timestamps[2].value(), 1.2);
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName,
      lcp_timestamps[2].value(), 1.2);

  // Check UMA.
  // Similar to UKM, rounding could introduce flakiness, so use helper to
  // compare near.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2",
      lcp_timestamps[2].value());
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.MainFrame",
      lcp_timestamps[2].value());
}

// TODO(crbug.com/40936591): This test is flaky on ChromeOS and Linux.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_LargestContentfulPaint_SubframeInput \
  DISABLED_LargestContentfulPaint_SubframeInput
#else
#define MAYBE_LargestContentfulPaint_SubframeInput \
  LargestContentfulPaint_SubframeInput
#endif
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       MAYBE_LargestContentfulPaint_SubframeInput) {
  Start();
  Load("/lcp_subframe_input.html");
  auto* sub = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(EvalJs(sub, "test_step_1()").value.GetString(), "lcp-16x16.png");

  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));

  EXPECT_EQ(EvalJs(sub, "test_step_2()").value.GetString(), "lcp-16x16.png");
}

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LargestContentfulPaintPaintPreview) {
  Start();
  Load("/largest_contentful_paint_paint_preview.html");

  content::EvalJsResult lcp_before_paint_preview =
      EvalJs(web_contents(), "block_for_next_lcp()");
  EXPECT_EQ("", lcp_before_paint_preview.error);

  paint_preview::PaintPreviewClient::CreateForWebContents(
      web_contents());  // Is a singleton.
  auto* client =
      paint_preview::PaintPreviewClient::FromWebContents(web_contents());

  paint_preview::PaintPreviewClient::PaintPreviewParams params(
      paint_preview::RecordingPersistence::kMemoryBuffer);
  params.inner.clip_rect = gfx::Rect(0, 0, 1, 1);
  params.inner.is_main_frame = true;
  params.inner.capture_links = false;
  params.inner.max_capture_size = 50 * 1024 * 1024;
  params.inner.max_decoded_image_size_bytes = 50 * 1024 * 1024;
  params.inner.skip_accelerated_content = true;

  base::RunLoop run_loop;
  client->CapturePaintPreview(
      params, web_contents()->GetPrimaryMainFrame(),
      base::BindOnce(
          [](base::OnceClosure callback, base::UnguessableToken,
             paint_preview::mojom::PaintPreviewStatus,
             std::unique_ptr<paint_preview::CaptureResult>) {
            std::move(callback).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  content::EvalJsResult lcp_after_paint_preview =
      EvalJs(web_contents(), "trigger_repaint_and_block_for_next_lcp()");
  EXPECT_EQ("", lcp_after_paint_preview.error);

  // When PaintPreview creates new LCP candidates, we compare the short text and
  // the long text here, which will fail. But in order to consistently get the
  // new LCP candidate in that case, we always add a medium text in
  // `trigger_repaint_and_block_for_next_lcp`. So use a soft comparison here
  // that would permit the medium text, but not the long text.
  EXPECT_LT(lcp_after_paint_preview.value.GetDouble(),
            2 * lcp_before_paint_preview.value.GetDouble());
}
#endif

class LCPLazyLoadingImageTest : public MetricIntegrationTest {
 public:
  base::Value::Dict setUpTraceEvent(std::string test_url) {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());

    Start();
    StartTracing({"loading"});
    Load(test_url);

    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);
    waiter->Wait();

    std::unique_ptr<TraceAnalyzer> trace_analyzer = StopTracingAndAnalyze();
    TraceEventVector candidate_events;
    trace_analyzer->FindEvents(
        Query::EventNameIs("largestContentfulPaint::Candidate"),
        &candidate_events);

    return candidate_events[0]->GetKnownArgAsDict("data");
  }
};

IN_PROC_BROWSER_TEST_F(LCPLazyLoadingImageTest,
                       LargestContentfulPaint_EventLazyLoadingImage_Enabled) {
  std::string test_url =
      "/lcp_breakdown_timings_native_lazy_loading_images.html";

  const base::Value::Dict data = setUpTraceEvent(test_url);
  const std::string* loading_attr = data.FindString("loadingAttr");
  ASSERT_TRUE(loading_attr);
  EXPECT_EQ(*loading_attr, "lazy");
}

IN_PROC_BROWSER_TEST_F(LCPLazyLoadingImageTest,
                       LargestContentfulPaint_EventLazyLoadingImage_Unset) {
  std::string test_url = "/iframe_with_image.html";

  const base::Value::Dict data = setUpTraceEvent(test_url);
  const std::string* loading_attr = data.FindString("loadingAttr");
  ASSERT_TRUE(loading_attr);
  EXPECT_EQ(*loading_attr, "");
}

// TODO(crbug.com/333641374): Re-enable this test
IN_PROC_BROWSER_TEST_F(
    LCPLazyLoadingImageTest,
    DISABLED_LargestContentfulPaint_EventLazyLoadingImage_Video) {
  std::string test_url = "/is_video.html";

  const base::Value::Dict data = setUpTraceEvent(test_url);
  const std::string* loading_attr = data.FindString("loadingAttr");
  ASSERT_TRUE(loading_attr);
  EXPECT_EQ(*loading_attr, "");
}

class PageViewportInLCPTest : public MetricIntegrationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures(
        {blink::features::kUsePageViewportInLCP} /*enabled*/, {} /*disabled*/);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageViewportInLCPTest, FullSizeImageInIframe) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddSubFrameExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                     TimingField::kLargestContentfulPaint);
  Start();
  StartTracing({"loading"});
  Load("/full_size_image.html");
  double lcpTime = EvalJs(web_contents(), "waitForLCP()").ExtractDouble();

  waiter->Wait();
  // Navigate away to force metrics recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  std::unique_ptr<TraceAnalyzer> trace_analyzer = StopTracingAndAnalyze();

  // |lcpTime| is computed from 3 different JS timestamps, so use an epsilon of
  // 2 to account for coarsening and UKM integer rounding.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name, lcpTime,
      2.0);
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2", lcpTime);

  ExpectMetricInLastUKMUpdateTraceEventNear(
      *trace_analyzer, "latest_largest_contentful_paint_ms", lcpTime, 2.0);
}

// TODO(crbug.com/40866505): Flaky on lacros
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_IsAnimatedLCPTest DISABLED_IsAnimatedLCPTest
#else
#define MAYBE_IsAnimatedLCPTest IsAnimatedLCPTest
#endif
class MAYBE_IsAnimatedLCPTest : public MetricIntegrationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "LCPAnimatedImagesWebExposed");
  }
  void test_is_animated(const char* html_name,
                        blink::LargestContentfulPaintType flag_set,
                        bool expected,
                        unsigned entries = 1) {
    // Install a ScopedRunLoopTimeout override to distinguish the timeout from
    // IsAnimatedLCPTest vs browser_test_base.
    base::test::ScopedRunLoopTimeout run_loop_timeout(FROM_HERE, std::nullopt,
                                                      base::NullCallback());
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);
    if (entries) {
      waiter->AddMinimumCompleteResourcesExpectation(entries);
    }
    Start();
    Load(html_name);
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(), "run_test()").error,
              "");

    // Need to navigate away from the test html page to force metrics to get
    // flushed/synced.
    waiter->Wait();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    ExpectUKMPageLoadMetricFlagSet(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flag_set), expected);
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_IsAnimatedLCPTest,
                       LargestContentfulPaint_IsAnimated) {
  test_is_animated("/is_animated.html",
                   blink::LargestContentfulPaintType::kAnimatedImage,
                   /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(MAYBE_IsAnimatedLCPTest,
                       LargestContentfulPaint_IsNotAnimated) {
  test_is_animated("/non_animated.html",
                   blink::LargestContentfulPaintType::kAnimatedImage,
                   /*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_IsAnimatedLCPTest,
    LargestContentfulPaint_AnimatedImageWithLargerTextFirst) {
  test_is_animated("/animated_image_with_larger_text_first.html",
                   blink::LargestContentfulPaintType::kAnimatedImage,
                   /*expected=*/false);
}

// crbug.com/1373885: This test is unreliable on ChromeOS, Linux and Mac
IN_PROC_BROWSER_TEST_F(MAYBE_IsAnimatedLCPTest,
                       DISABLED_LargestContentfulPaint_IsVideo) {
  test_is_animated("/is_video.html", blink::LargestContentfulPaintType::kVideo,
                   /*expected=*/true, /*entries=*/0);
}

class LargestContentfulPaintTypeTest : public MetricIntegrationTest {
 public:
  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    waiter_ = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

 protected:
  enum class ElementOrder { kImageFirst, kTextFirst };

  void Wait() { waiter_->Wait(); }

  void TestImage(std::string& imgSrc,
                 blink::LargestContentfulPaintType flagSet) {
    AddMinimumLargestContentfulPaintImageExpectation(1);

    Navigate("/lcp_type.html");

    AddImage(imgSrc);

    Wait();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

    ExpectUKMPageLoadMetricFlagSetExactMatch(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flagSet));
  }

  void TestText(std::string& text, blink::LargestContentfulPaintType flagSet) {
    AddMinimumLargestContentfulPaintTextExpectation(1);

    Navigate("/lcp_type.html");

    AddText(text);

    Wait();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

    ExpectUKMPageLoadMetricFlagSetExactMatch(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flagSet));
  }

  void TestTextAndImage(ElementOrder elementOrder,
                        std::string& text,
                        std::string& imgSrc,
                        blink::LargestContentfulPaintType flagSet) {
    Navigate("/lcp_type.html");

    if (elementOrder == ElementOrder::kTextFirst) {
      AddMinimumLargestContentfulPaintTextExpectation(1);

      AddText(text);

      Wait();

      AddMinimumLargestContentfulPaintImageExpectation(1);

      AddImage(imgSrc);
    } else {
      AddMinimumLargestContentfulPaintImageExpectation(1);

      AddImage(imgSrc);

      Wait();

      AddMinimumLargestContentfulPaintTextExpectation(1);

      AddText(text);
    }

    Wait();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

    ExpectUKMPageLoadMetricFlagSetExactMatch(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flagSet));
  }

  void TestVideoDataURI(blink::LargestContentfulPaintType flagSet) {
    AddMinimumLargestContentfulPaintImageExpectation(1);
    Navigate("/lcp_type_video_data_uri.html");

    Wait();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

    ExpectUKMPageLoadMetricFlagSetExactMatch(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flagSet));
  }

 private:
  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter> waiter_;

  void AddMinimumLargestContentfulPaintTextExpectation(
      uint32_t expected_count) {
    waiter_->AddMinimumLargestContentfulPaintTextExpectation(expected_count);
  }

  void AddMinimumLargestContentfulPaintImageExpectation(
      uint32_t expected_count) {
    waiter_->AddMinimumLargestContentfulPaintImageExpectation(expected_count);
  }

  void Navigate(std::string url) {
    Start();
    Load(url);
  }

  void AddImage(const std::string& imgSrc) {
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                     content::JsReplace("add_image($1)", imgSrc))
                  .error,
              "");
  }

  void AddText(std::string_view text) {
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                     content::JsReplace("add_text($1)", text))
                  .error,
              "");
  }
};

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, ImageType_PNG) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kPNG;
  std::string imgSrc = "images/lcp-133x106.png";
  TestImage(imgSrc, flag_set);
}

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, ImageType_JPG) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kJPG;
  std::string imgSrc = "images/arrow-oriented-upright.jpg";
  TestImage(imgSrc, flag_set);
}

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, ImageType_WebP) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kAnimatedImage |
                  blink::LargestContentfulPaintType::kWebP;
  std::string imgSrc = "images/webp-animated.webp";
  TestImage(imgSrc, flag_set);
}

// TODO(crbug.com/333963663): Flaky on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ImageType_GIF DISABLED_ImageType_GIF
#else
#define MAYBE_ImageType_GIF ImageType_GIF
#endif
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, MAYBE_ImageType_GIF) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kGIF |
                  blink::LargestContentfulPaintType::kAnimatedImage;
  std::string imgSrc = "images/fail.gif";
  TestImage(imgSrc, flag_set);
}

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, ImageType_AVIF) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kAVIF;
  std::string imgSrc = "images/green.avif";
  TestImage(imgSrc, flag_set);
}

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, ImageType_SVG) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kSVG;
  std::string imgSrc = "images/colors.svg";
  TestImage(imgSrc, flag_set);
}

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, TextType) {
  auto flag_set = blink::LargestContentfulPaintType::kText;
  std::string text = "This is to test LargestContentfulPaintType::kText";
  TestText(text, flag_set);
}

// Case when text that is larger and comes before an image. The
// LargestContentfulPaintType should be those of a text element.
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest,
                       LargeTextAndImage_TextType) {
  auto flag_set = blink::LargestContentfulPaintType::kText;
  std::string text =
      "This is a text that is larger and comes before an image. The "
      "LargestContentfulPaintType should be those of a text element.";
  std::string imgSrc = "images/lcp-2x2.png";

  // The larger element comes first so 1 LCP entry is expected.
  TestTextAndImage(ElementOrder::kTextFirst, text, imgSrc, flag_set);
}

// Case when text that is larger and comes after an image. The
// LargestContentfulPaintType should be those of a text element.
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest,
                       ImageAndLargeText_TextType) {
  auto flag_set = blink::LargestContentfulPaintType::kText;
  std::string text =
      "This is a text that is larger and comes after an image. The "
      "LargestContentfulPaintType should be those of a text element.";
  std::string imgSrc = "images/lcp-2x2.png";

  // The larger element comes later so 2 LCP entries are expected.
  TestTextAndImage(ElementOrder::kImageFirst, text, imgSrc, flag_set);
}

// Case when a text that is smaller and comes before an Image. The
// LargestContentfulPaintType should be those of an image element.
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest,
                       TextAndLargeImage_ImageType) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kGIF |
                  blink::LargestContentfulPaintType::kAnimatedImage;
  ;
  std::string text =
      "This is a text that is smaller and comes before an image. The "
      "LargestContentfulPaintType should be those of an image element";
  std::string imgSrc = "images/fail.gif";
  TestTextAndImage(ElementOrder::kTextFirst, text, imgSrc, flag_set);
}

// Case when a text that is smaller and comes after an Image. The
// LargestContentfulPaintType should be those of an image element.
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest,
                       LargeImageAndText_ImageType) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kGIF |
                  blink::LargestContentfulPaintType::kAnimatedImage;
  std::string text =
      "This is a text that is smaller and comes after an Image. The "
      "LargestContentfulPaintType should be those of an image element.";
  std::string imgSrc = "images/fail.gif";
  TestTextAndImage(ElementOrder::kImageFirst, text, imgSrc, flag_set);
}

// (https://crbug.com/1385713): Flaky on mac12-arm64-rel M1 Mac CQ.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DataURIType DISABLED_DataURIType
#else
#define MAYBE_DataURIType DataURIType
#endif
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, MAYBE_DataURIType) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kGIF |
                  blink::LargestContentfulPaintType::kAnimatedImage |
                  blink::LargestContentfulPaintType::kDataURI;
  std::string imgSrc =
      "data:image/gif;base64,R0lGODdhAgADAKEDAAAA//8AAAD/AP///"
      "ywAAAAAAgADAAACBEwkAAUAOw==";
  TestImage(imgSrc, flag_set);
}

// (https://crbug.com/1385713): Flaky on mac12-arm64-rel M1 Mac CQ.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DataURIType_SVG DISABLED_DataURIType_SVG
#else
#define MAYBE_DataURIType_SVG DataURIType_SVG
#endif
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest, MAYBE_DataURIType_SVG) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kSVG |
                  blink::LargestContentfulPaintType::kDataURI;
  // percent-encoding of the svg url obtained by encodeURIComponent("<svg
  // xmlns='http://www.w3.org/2000/svg' width='16' height='16'
  // viewBox='0 0 16 16'><rect stroke-width='2' stroke='black' x='1' y='1'
  // width='14' height='14'fill='lime'/></svg>"
  std::string imgSrc =
      "data:image/"
      "svg+xml, "
      "%3Csvg%20xmlns%3D%27http%3A//www.w3.org/2000/svg%27%20width%3D%2716%27%"
      "20height%3D%2716%27%20viewBox%3D%270%200%2016%2016%27%3E%3Crect%20strok"
      "e-width%3D%272%27%20stroke%3D%27black%27%20x%3D%271%27%20y%3D%271%27%20"
      "width%3D%2714%27%20height%3D%2714%27%20fill%3D%27lime%27/%3E%3C/svg%3E";

  TestImage(imgSrc, flag_set);
}

// (https://crbug.com/1385713): Flaky on mac12-arm64-rel M1 Mac CQ.
// (https://crbug.com/1405307): Flaky on ChromeOS and Linux as well.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_DataURIType_Video DISABLED_DataURIType_Video
#else
#define MAYBE_DataURIType_Video DataURIType_Video
#endif
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTypeTest,
                       MAYBE_DataURIType_Video) {
  auto flag_set = blink::LargestContentfulPaintType::kImage |
                  blink::LargestContentfulPaintType::kVideo |
                  blink::LargestContentfulPaintType::kDataURI;

  TestVideoDataURI(flag_set);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, LCPBreakdownTimings) {
  Start();
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kLargestContentfulPaint);
  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);

  Load("/lcp_breakdown_timings.html");

  std::string url = "/images/lcp-16x16.png";
  std::string element_id = "image";
  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   content::JsReplace("addImage($1, $2)", url, element_id))
                .error,
            "");
  double web_exposed_lcp = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                  content::JsReplace("getLCP($1)", element_id))
                               .ExtractDouble();

  double request_start = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                content::JsReplace("getRequestStart($1)", url))
                             .ExtractDouble();

  double response_end = EvalJs(web_contents()->GetPrimaryMainFrame(),
                               content::JsReplace("getResponseEnd($1)", url))
                            .ExtractDouble();

  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify breakdown timings of LCP are in correct order.
  ExpectUKMPageLoadMetricsInAscendingOrder(
      PageLoad::kPaintTiming_LargestContentfulPaintImageDiscoveryTimeName,
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName);

  ExpectUKMPageLoadMetricsInAscendingOrder(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName,
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName);

  ExpectUKMPageLoadMetricsInAscendingOrder(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName,
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name);

  // Verify breakdown timings recorded to UKM are correct. There's discrepancy
  // between the web-exposed value and the UKM value. An epsilon of 2
  // milliseconds is used to account for +-2 difference as this 2 is used
  // elsewhere.
  double epsilon = 2;

  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName,
      request_start, epsilon);

  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName,
      response_end, epsilon);

  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
      web_exposed_lcp, epsilon);
}

class LcpBreakdownTimingsTest : public MetricIntegrationTest {
 protected:
  void RunTest(std::string test_url,
               std::string resource,
               std::optional<uint8_t> subframe_index,
               std::string script = "") {
    Start();
    auto waiter0 =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());

    waiter0->AddMinimumLargestContentfulPaintImageExpectation(1);

    // Navigate to the test page.
    Load(test_url);

    // Execute script if any.
    if (!script.empty()) {
      EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(), script).error,
                "");
    }

    waiter0->Wait();

    auto* rfh =
        !subframe_index.has_value()
            ? web_contents()->GetPrimaryMainFrame()
            : content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(),
                                    subframe_index.value());

    timeline_lcp_list_[0] =
        EvalJs(rfh, content::JsReplace("getLcp($1)", resource)).ExtractDouble();

    // Retrieve resource timing timings of initial load for validation.
    request_start_list_[0] =
        EvalJs(rfh, content::JsReplace("getRequestStart($1)", resource))
            .ExtractDouble();

    response_end_list_[0] =
        EvalJs(rfh, content::JsReplace("getResponseEnd($1)", resource))
            .ExtractDouble();

    start_time_list_[0] =
        EvalJs(rfh, content::JsReplace("getStartTime($1)", resource))
            .ExtractDouble();

    auto waiter1 =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());

    waiter1->AddMinimumLargestContentfulPaintImageExpectation(1);

    // Refresh
    web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);

    EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

    // Execute script if any.
    if (!script.empty()) {
      EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(), script).error,
                "");
    }

    waiter1->Wait();

    rfh = !subframe_index.has_value()
              ? web_contents()->GetPrimaryMainFrame()
              : content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(),
                                      subframe_index.value());
    timeline_lcp_list_[1] =
        EvalJs(rfh, content::JsReplace("getLcp($1)", resource)).ExtractDouble();

    // Retrieve resource timing timings after refresh for validation.
    request_start_list_[1] =
        EvalJs(rfh, content::JsReplace("getRequestStart($1)", resource))
            .ExtractDouble();

    response_end_list_[1] =
        EvalJs(rfh, content::JsReplace("getResponseEnd($1)", resource))
            .ExtractDouble();

    start_time_list_[1] =
        EvalJs(rfh, content::JsReplace("getStartTime($1)", resource))
            .ExtractDouble();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  void Validate(bool is_lcp_from_subframe = false) {
    std::vector<double> ttfb_list = GetPageLoadMetricsAsList(
        PageLoad::kMainFrameResource_NavigationStartToReceiveHeadersStartName);

    discovery_time_list_ = GetPageLoadMetricsAsList(
        PageLoad::kPaintTiming_LargestContentfulPaintImageDiscoveryTimeName);

    load_start_list_ = GetPageLoadMetricsAsList(
        PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName);

    load_end_list_ = GetPageLoadMetricsAsList(
        PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName);

    lcp_list_ = GetPageLoadMetricsAsList(
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name);

    EXPECT_EQ(discovery_time_list_.size(), 2u);

    EXPECT_EQ(load_start_list_.size(), 2u);

    EXPECT_EQ(load_end_list_.size(), 2u);

    EXPECT_EQ(lcp_list_.size(), 2u);

    //  Validate timings for initial load.
    EXPECT_LT(ttfb_list[0], discovery_time_list_[0]);

    EXPECT_LE(discovery_time_list_[0], load_start_list_[0]);

    EXPECT_LT(load_start_list_[0], load_end_list_[0]);

    EXPECT_LT(load_end_list_[0], lcp_list_[0]);

    // LCP breadown timings should be the same as resource timings. If the LCP
    // element is from a subframe, the navigation start offset should be
    // accounted for.
    auto navigation_start_offset =
        is_lcp_from_subframe ? discovery_time_list_[0] - start_time_list_[0]
                             : 0;
    EXPECT_NEAR(discovery_time_list_[0], start_time_list_[0],
                epsilon_ + navigation_start_offset);

    EXPECT_NEAR(load_start_list_[0], request_start_list_[0],
                epsilon_ + navigation_start_offset);

    EXPECT_NEAR(load_end_list_[0], response_end_list_[0],
                epsilon_ + navigation_start_offset);

    EXPECT_NEAR(lcp_list_[0], timeline_lcp_list_[0],
                epsilon_ + navigation_start_offset);

    // Validate timings after refresh. LCP breakdown timings could be equal if
    // the image is loaded from memory. Hence we use EXPECT_LE instead of
    // EXPECT_LT.
    EXPECT_LE(ttfb_list[1], discovery_time_list_[1]);

    EXPECT_LE(discovery_time_list_[1], load_start_list_[1]);

    EXPECT_LE(load_start_list_[1], load_end_list_[1]);

    EXPECT_LE(load_end_list_[1], lcp_list_[1]);

    // LCP breadown timings should be the same as resource timings.
    navigation_start_offset =
        is_lcp_from_subframe ? discovery_time_list_[1] - start_time_list_[1]
                             : 0;

    EXPECT_NEAR(discovery_time_list_[1], start_time_list_[1],
                epsilon_ + navigation_start_offset);

    EXPECT_NEAR(load_start_list_[1], request_start_list_[1],
                epsilon_ + navigation_start_offset);

    EXPECT_NEAR(load_end_list_[1], response_end_list_[1],
                epsilon_ + navigation_start_offset);

    EXPECT_NEAR(lcp_list_[1], timeline_lcp_list_[1],
                epsilon_ + navigation_start_offset);
  }

  void ValidateForMemCacheLoadedImages() {
    EXPECT_EQ(discovery_time_list_[1], load_start_list_[1]);

    EXPECT_EQ(load_start_list_[1], load_end_list_[1]);
  }

 private:
  std::array<double, 2> start_time_list_;
  std::array<double, 2> request_start_list_;
  std::array<double, 2> response_end_list_;
  std::array<double, 2> timeline_lcp_list_;
  std::vector<double> discovery_time_list_;
  std::vector<double> load_start_list_;
  std::vector<double> load_end_list_;
  std::vector<double> lcp_list_;
  double epsilon_ = 1.5;
};

IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, Subframe) {
  std::string url = "/lcp_breakdown_timings_with_subframe.html";
  auto* resource_name = "lcp-256x256.png";
  RunTest(url, resource_name, 0, "addSameSiteSubframe()");
  Validate(true);
  ValidateForMemCacheLoadedImages();
}

// TODO(323888356): There is an issue in the LCP size calculation in a cross
// site subframe. This test should be enabled after that issue is resolved.
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, DISABLED_CrossSiteSubframe) {
  std::string url = "/lcp_breakdown_timings_with_subframe.html";
  auto* resource_name = "lcp-256x256.png";
  RunTest(url, resource_name, 0, "addCrossSiteSubframe()");
  Validate(true);
  // Image that is in cross origin subframe wouldn't be loaded from cache, so
  // we don't verify that the breakdown timings are all set to discovery time
  // after refresh.
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_MemCacheServedImage DISABLED_MemCacheServedImage
#else
#define MAYBE_MemCacheServedImage MemCacheServedImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_MemCacheServedImage) {
  std::string test_url = "/lcp_breakdown_timings_memcache_served_images.html";
  std::string resource = "green.png";

  RunTest(test_url, resource, std::nullopt);
  Validate();

  // Since after refresh, the image is loaded from mem cache, the discovery_time
  // load start and load end should be the same.
  ValidateForMemCacheLoadedImages();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PreloadedImage DISABLED_PreloadedImage
#else
#define MAYBE_PreloadedImage PreloadedImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_PreloadedImage) {
  std::string test_url = "/lcp_breakdown_timings_preloaded_images.html";
  std::string resource = "/images/lcp-16x16.png";
  RunTest(test_url, resource, std::nullopt,
          content::JsReplace("addImageWithUrl($1)", resource));
  Validate();
}

// TODO(crbug.com/333963663): Flaky on multiple platforms.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_PreloadedCacheableImage DISABLED_PreloadedCacheableImage
#else
#define MAYBE_PreloadedCacheableImage PreloadedCacheableImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_PreloadedCacheableImage) {
  std::string test_url =
      "/lcp_breakdown_timings_preloaded_cacheable_images.html";
  std::string resource = "green.png";
  RunTest(test_url, resource, std::nullopt,
          content::JsReplace("addImageWithUrl($1)", resource));
  Validate();
  ValidateForMemCacheLoadedImages();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_NativeLazyLoadingImage DISABLED_NativeLazyLoadingImage
#else
#define MAYBE_NativeLazyLoadingImage NativeLazyLoadingImage
#endif
// TODO(crbug.com/335901379): Re-enable test
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_NativeLazyLoadingImage) {
  std::string test_url =
      "/lcp_breakdown_timings_native_lazy_loading_images.html";
  std::string resource = "lcp-16x16.png";
  RunTest(test_url, resource, std::nullopt);
  Validate();
}

// TODO(crbug.com/40283415): This test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest,
                       DISABLED_ManualLazyLoadingImage) {
  std::string test_url =
      "/lcp_breakdown_timings_manual_lazy_loading_images.html";
  std::string resource = "lcp-16x16.png";
  RunTest(test_url, resource, std::nullopt,
          content::JsReplace("(async ()=>{await scrollToLoadImage($1);})()",
                             resource));
  Validate();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_CssBackgroundImage DISABLED_CssBackgroundImage
#else
#define MAYBE_CssBackgroundImage CssBackgroundImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_CssBackgroundImage) {
  std::string test_url = "/lcp_breakdown_timings_css_background_images.html";
  std::string resource = "lcp-256x256.png";
  RunTest(test_url, resource, std::nullopt);
  Validate();
}

// TODO(crbug.com/41495170): Flaky test.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_WrittenAsInnerHtmlImage DISABLED_WrittenAsInnerHtmlImage
#else
#define MAYBE_WrittenAsInnerHtmlImage WrittenAsInnerHtmlImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_WrittenAsInnerHtmlImage) {
  std::string test_url = "/lcp_breakdown_timings_written_as_html_images.html";
  std::string resource = "/images/lcp-256x256.png";
  RunTest(test_url, resource, std::nullopt,
          "AddImageByScript(WriteToDomAsInnerHtml);");
  Validate();
}

// TODO(crbug.com/41494085): Flaky on Mac.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_WrittenAsOuterHtmlImage DISABLED_WrittenAsOuterHtmlImage
#else
#define MAYBE_WrittenAsOuterHtmlImage WrittenAsOuterHtmlImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_WrittenAsOuterHtmlImage) {
  std::string test_url = "/lcp_breakdown_timings_written_as_html_images.html";
  std::string resource = "/images/lcp-256x256.png";
  RunTest(test_url, resource, std::nullopt,
          "AddImageByScript(WriteToDomAsOuterHtml);");
  Validate();
}

// Flaky timeout with ASAN (crbug.com/337012486)
#if BUILDFLAG(IS_CHROMEOS_LACROS) || defined(ADDRESS_SANITIZER)
#define MAYBE_DocumentWrittenImage DISABLED_DocumentWrittenImage
#else
#define MAYBE_DocumentWrittenImage DocumentWrittenImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_DocumentWrittenImage) {
  std::string test_url = "/lcp_breakdown_timings_document_written_images.html";
  std::string resource = "/images/lcp-256x256.png";
  RunTest(test_url, resource, std::nullopt);
  Validate();
}

// Flaky timeout with ASAN (crbug.com/337012486)
#if BUILDFLAG(IS_CHROMEOS_LACROS) || defined(ADDRESS_SANITIZER)
#define MAYBE_SrcSetImage DISABLED_SrcSetImage
#else
#define MAYBE_SrcSetImage SrcSetImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_SrcSetImage) {
  std::string test_url = "/lcp_breakdown_timings_srcset_images.html";
  std::string resource = "lcp-256x256.png";

  RunTest(test_url, resource, std::nullopt);
  Validate();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_DomMethodAddedImage DISABLED_DomMethodAddedImage
#else
#define MAYBE_DomMethodAddedImage DomMethodAddedImage
#endif
IN_PROC_BROWSER_TEST_F(LcpBreakdownTimingsTest, MAYBE_DomMethodAddedImage) {
  std::string test_url = "/lcp_breakdown_timings_empty.html";
  std::string resource = "/images/lcp-256x256.png";

  RunTest(test_url, resource, std::nullopt,
          content::JsReplace("addImageWithUrl($1)", resource));
  Validate();
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LCPBreakdownTimings_ImageAndLargerText) {
  Start();
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kLargestContentfulPaint);
  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);

  Load("/lcp_breakdown_timings.html");

  const std::string url1 = "/images/lcp-16x16.png";
  const std::string element_id1 = "image";

  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   content::JsReplace("addImage($1, $2)", url1, element_id1))
                .error,
            "");

  waiter->Wait();

  waiter->AddMinimumLargestContentfulPaintTextExpectation(1);

  const std::string element_id2 = "text";

  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   content::JsReplace("addText($1, $2)", element_id2))
                .error,
            "");

  waiter->Wait();

  double text_element_lcp =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace("getLCP($1)", element_id2))
          .ExtractDouble();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify the LCP recorded in the UKM is the one of the text element.
  double epsilon = 2;
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
      text_element_lcp, epsilon);

  // Verify breakdown timings of LCP are not set for text elements.
  ExpectUKMPageLoadMetricNonExistence(
      PageLoad::kPaintTiming_LargestContentfulPaintImageDiscoveryTimeName);

  ExpectUKMPageLoadMetricNonExistence(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName);

  ExpectUKMPageLoadMetricNonExistence(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LCPBreakdownTimings_ImageAndLargerImage) {
  Start();
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());

  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);

  Load("/lcp_breakdown_timings.html");

  // Load an image.
  const std::string url1 = "/images/lcp-16x16.png";
  const std::string element_id1 = "image";
  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   content::JsReplace("addImage($1, $2)", url1, element_id1))
                .error,
            "");

  waiter->Wait();

  // Load Larger image which becomes the LCP element.
  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);

  // The UKM recorded LCP should be that of the second image, which should be
  // larger than the LCP of first image.

  const std::string url2 = "/images/lcp-256x256.png";
  const std::string element_id2 = "larger_image";

  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                   content::JsReplace("addImage($1, $2)", url2, element_id2))
                .error,
            "");

  double web_exposed_lcp2 =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace("getLCP($1)", element_id2))
          .ExtractDouble();
  double epsilon = 2;

  // This is to reduce flakiness by waiting for an LCP larger than the value
  // passed in so that by the time the test waiter exits from waiting the LCP of
  // 2nd image has been updated. The value web_exposed_lcp2 - epsilon is roughly
  // the LCP of 2nd image which is considerably larger than the LCP of 1st
  // image. Having a negative epsilon is to count for rounding/conversion
  // differences.
  waiter->AddLargestContentfulPaintGreaterThanExpectation(web_exposed_lcp2 -
                                                          epsilon);

  double request_start = EvalJs(web_contents()->GetPrimaryMainFrame(),
                                content::JsReplace("getRequestStart($1)", url2))
                             .ExtractDouble();

  double response_end = EvalJs(web_contents()->GetPrimaryMainFrame(),
                               content::JsReplace("getResponseEnd($1)", url2))
                            .ExtractDouble();

  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify breakdown timings of LCP are in correct order.
  ExpectUKMPageLoadMetricsInAscendingOrder(
      PageLoad::kPaintTiming_LargestContentfulPaintImageDiscoveryTimeName,
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName);

  ExpectUKMPageLoadMetricsInAscendingOrder(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName,
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName);

  ExpectUKMPageLoadMetricsInAscendingOrder(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName,
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name);

  // Verify breakdown timings recorded to UKM are the correct ones.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName,
      request_start, epsilon);

  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName,
      response_end, epsilon);

  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
      web_exposed_lcp2, epsilon);
}

// TODO(1495363, 1495698): Test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       DISABLED_LCPBreakdownTimings_DetachedWindow) {
  Start();

  Load("/lcp_detached_window.html");

  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(), "runTest()").error,
            "");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // There are 2 PageLoadMetrics instances, one for main window, one for the
  // opened window.
  ExpectUKMPageLoadMetricNonExistenceWithExpectedPageLoadMetricsNum(
      2ul, PageLoad::kPaintTiming_LargestContentfulPaintImageLoadStartName);

  ExpectUKMPageLoadMetricNonExistenceWithExpectedPageLoadMetricsNum(
      2ul, PageLoad::kPaintTiming_LargestContentfulPaintImageLoadEndName);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LCPBreakdownTimings_NoLcpForBrokenImage) {
  Start();

  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  Load("/lcp_broken_image_icon.html");

  waiter->Wait();

  // Expect no LCP entry is emitted to performance timeline.
  EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(), "GetLCP();"), 0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // No LCP is recorded.
  ExpectUKMPageLoadMetricNonExistence(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LcpSameOriginImage_CrossOriginTypeNotSet) {
  Start();
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);
  auto image_url =
      embedded_test_server()->GetURL("example.com", "/lcp-256x256.png");

  Load("/lcp_image_varyorigin.html");

  auto image_url_set = EvalJs(
      web_contents(),
      base::StringPrintf("lcp_image.src='%s'", image_url.spec().c_str()));

  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  int64_t expected_lcp_type_flags =
      static_cast<uint64_t>(blink::LargestContentfulPaintType::kImage |
                            blink::LargestContentfulPaintType::kPNG);

  ASSERT_EQ(GetUKMPageLoadMetricFlagSet(
                PageLoad::kPaintTiming_LargestContentfulPaintTypeName),
            expected_lcp_type_flags);

  ExpectUKMPageLoadMetric(
      PageLoad::kPaintTiming_LargestContentfulPaintImageIsCrossOriginName, 0);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LcpCrossOriginImage_CrossOriginTypeIsSet) {
  Start();
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddMinimumLargestContentfulPaintImageExpectation(1);
  auto image_url =
      embedded_test_server()->GetURL("crossorigin.com", "/lcp-256x256.png");

  Load("/lcp_image_varyorigin.html");

  auto image_url_set = EvalJs(
      web_contents(),
      base::StringPrintf("lcp_image.src='%s'", image_url.spec().c_str()));

  waiter->Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  int64_t expected_lcp_type_flags =
      static_cast<uint64_t>(blink::LargestContentfulPaintType::kImage |
                            blink::LargestContentfulPaintType::kPNG |
                            blink::LargestContentfulPaintType::kCrossOrigin);

  ASSERT_EQ(GetUKMPageLoadMetricFlagSet(
                PageLoad::kPaintTiming_LargestContentfulPaintTypeName),
            expected_lcp_type_flags);

  ExpectUKMPageLoadMetric(
      PageLoad::kPaintTiming_LargestContentfulPaintImageIsCrossOriginName, 1);
}
