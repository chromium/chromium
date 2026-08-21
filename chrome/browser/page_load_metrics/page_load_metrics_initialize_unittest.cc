// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class PageLoadMetricsInitializeTest : public BrowserWithTestWindowTest {
 public:
  PageLoadMetricsInitializeTest() = default;
  ~PageLoadMetricsInitializeTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Default to startup complete so we can customize it in specific tests.
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
    page_load_metrics::LargestContentfulPaintHandler::SetTestMode(true);
  }

  void TearDown() override {
    // Clean up startup complete flag so we don't pollute other tests.
    AfterStartupTaskUtils::UnsafeResetForTesting();
    mock_tabs_.clear();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::vector<std::unique_ptr<tabs::MockTabInterface>> mock_tabs_;
  ui::UnownedUserDataHost unowned_user_data_host_;

  void SendTimingUpdate(content::WebContents* web_contents,
                        base::TimeDelta fcp,
                        base::TimeDelta lcp,
                        base::Time navigation_start_time) {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = navigation_start_time;
    timing.response_start = base::Milliseconds(10);
    timing.parse_timing->parse_start = base::Milliseconds(50);
    timing.paint_timing->first_contentful_paint = fcp;
    timing.paint_timing->largest_contentful_paint->largest_text_paint = lcp;
    timing.paint_timing->largest_contentful_paint->largest_text_paint_size =
        10u;
    PopulateRequiredTimingFields(&timing);

    page_load_metrics::mojom::PageLoadTimingPtr timing_ptr = timing.Clone();
    page_load_metrics::mojom::FrameMetadataPtr metadata_ptr =
        page_load_metrics::mojom::FrameMetadata::New();
    page_load_metrics::mojom::FrameRenderDataUpdatePtr render_data_ptr =
        page_load_metrics::mojom::FrameRenderDataUpdate::New();
    page_load_metrics::mojom::CpuTimingPtr cpu_timing_ptr =
        page_load_metrics::mojom::CpuTiming::New();
    page_load_metrics::mojom::FontLoadingMetricsPtr font_loading_metrics_ptr =
        page_load_metrics::mojom::FontLoadingMetrics::New();

    page_load_metrics::MetricsWebContentsObserver::FromWebContents(web_contents)
        ->OnTimingUpdated(
            web_contents->GetPrimaryMainFrame(), std::move(timing_ptr),
            std::move(metadata_ptr), std::vector<blink::UseCounterFeature>(),
            std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>(),
            std::move(render_data_ptr), std::move(cpu_timing_ptr),
            std::vector<page_load_metrics::mojom::EventTimingPtr>(),
            std::nullopt,
            std::vector<page_load_metrics::mojom::SoftNavigationMetricsPtr>(),
            std::vector<
                page_load_metrics::mojom::LargestContentfulPaintTimingPtr>(),
            std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>(),
            std::move(font_loading_metrics_ptr));
  }

  void SimulatePageLoad(content::WebContents* web_contents,
                        BrowserWindowInterface* browser = nullptr) {
    auto mock_tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab, GetContents())
        .WillByDefault(testing::Return(web_contents));
    ON_CALL(*mock_tab, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    if (browser) {
      ON_CALL(*mock_tab, GetBrowserWindowInterface())
          .WillByDefault(testing::Return(browser));
    }
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents,
                                                         mock_tab.get());
    mock_tabs_.push_back(std::move(mock_tab));

    // Initialize page load metrics for the WebContents.
    InitializePageLoadMetricsForWebContents(web_contents);

    // Simulate navigation and paint timing updates.
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateBrowserInitiated(
            GURL("https://google.com"), web_contents);
    navigation->Start();
    navigation->Commit();

    SendTimingUpdate(web_contents, base::Milliseconds(100),
                     base::Milliseconds(200),
                     base::Time::FromSecondsSinceUnixEpoch(1));

    // Navigate away to force LCP logging.
    std::unique_ptr<content::NavigationSimulator> navigation2 =
        content::NavigationSimulator::CreateBrowserInitiated(
            GURL("https://example.com"), web_contents);
    navigation2->Start();
    navigation2->Commit();
  }
};

// Verifies that navigations occurring during browser startup are classified as
// kStartup.
TEST_F(PageLoadMetricsInitializeTest, StartupClassification) {
  base::HistogramTester histogram_tester;

  // Make browser startup not complete.
  AfterStartupTaskUtils::UnsafeResetForTesting();

  // Create a tab and perform a page load.
  AddTab(browser(), GURL("about:blank"));
  SimulatePageLoad(browser()->tab_strip_model()->GetActiveWebContents(),
                   browser());

  // Verify Startup suffix histograms are logged.
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Startup",
      base::Milliseconds(100), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Startup",
      base::Milliseconds(200), 1);

  // Other suffix histograms should not be logged.
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NewWindow", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.SameWindow", 0);
}

// Verifies that the first navigation in a new window is classified as
// kNewWindow.
TEST_F(PageLoadMetricsInitializeTest, NewWindowClassification) {
  base::HistogramTester histogram_tester;

  // Startup is complete.
  ASSERT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  // Clear existing default tab from browser() so it is a fresh empty window.
  browser()->tab_strip_model()->CloseAllTabs();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  content::WebContents* raw_web_contents = web_contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /*foreground=*/true);

  SimulatePageLoad(raw_web_contents, browser());

  // Verify NewWindow suffix histograms are logged.
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NewWindow",
      base::Milliseconds(100), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NewWindow",
      base::Milliseconds(200), 1);
}

// Verifies that the first navigation in a new tab, other than the first tab in
// the window, is classified as kSameWindow.
TEST_F(PageLoadMetricsInitializeTest, SameWindowClassification) {
  base::HistogramTester histogram_tester;

  // Startup is complete.
  ASSERT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  // First tab in the window.
  AddTab(browser(), GURL("about:blank"));

  // Open a second tab in the same window.
  AddTab(browser(), GURL("about:blank"));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  SimulatePageLoad(browser()->tab_strip_model()->GetActiveWebContents(),
                   browser());

  // Verify SameWindow suffix histograms are logged.
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.SameWindow",
      base::Milliseconds(100), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.SameWindow",
      base::Milliseconds(200), 1);
}

// Verifies that subsequent navigations in the same WebContents are classified
// as SameWindow.
TEST_F(PageLoadMetricsInitializeTest, SubsequentNavigationIsSameWindow) {
  base::HistogramTester histogram_tester;

  // Startup is complete.
  ASSERT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  // Clear existing default tab from browser() so it starts empty.
  browser()->tab_strip_model()->CloseAllTabs();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  content::WebContents* raw_web_contents = web_contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /*foreground=*/true);

  // Initialize page load metrics for the WebContents.
  auto mock_tab = std::make_unique<tabs::MockTabInterface>();
  ON_CALL(*mock_tab, GetContents())
      .WillByDefault(testing::Return(raw_web_contents));
  ON_CALL(*mock_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(browser()));
  tabs::TabLookupFromWebContents::CreateForWebContents(raw_web_contents,
                                                       mock_tab.get());
  mock_tabs_.push_back(std::move(mock_tab));

  InitializePageLoadMetricsForWebContents(raw_web_contents);

  // --- First Navigation ---
  std::unique_ptr<content::NavigationSimulator> navigation1 =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://google.com"), raw_web_contents);
  navigation1->Start();
  navigation1->Commit();

  SendTimingUpdate(raw_web_contents, base::Milliseconds(100),
                   base::Milliseconds(200),
                   base::Time::FromSecondsSinceUnixEpoch(1));

  // --- Second Navigation ---
  // Committing this navigation will force logging of the first navigation.
  std::unique_ptr<content::NavigationSimulator> navigation2 =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.com"), raw_web_contents);
  navigation2->Start();
  navigation2->Commit();

  // Send timing for the second navigation.
  SendTimingUpdate(raw_web_contents, base::Milliseconds(150),
                   base::Milliseconds(250),
                   base::Time::FromSecondsSinceUnixEpoch(2));

  // --- Third Navigation (to force logging of the second navigation) ---
  std::unique_ptr<content::NavigationSimulator> navigation3 =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://third.com"), raw_web_contents);
  navigation3->Start();
  navigation3->Commit();

  // Verify SameWindow suffix histograms are logged for the second load.
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.SameWindow",
      base::Milliseconds(150), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.SameWindow",
      base::Milliseconds(250), 1);
}

// Verifies that navigations in a WebContents that is not attached to any
// browser (unknown context) are classified as kUnknown (no suffix logged).
TEST_F(PageLoadMetricsInitializeTest, UnknownClassification) {
  base::HistogramTester histogram_tester;

  // Startup is complete.
  ASSERT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  // Create a WebContents but do NOT attach it to a browser.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));

  SimulatePageLoad(web_contents.get());

  // Verify NO suffix histograms are logged (they should be kUnknown).
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Startup", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NewWindow", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.SameWindow", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Startup", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NewWindow", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.SameWindow", 0);
}

// Verifies that a reload of a discarded tab in a single-tab window is
// classified as SameWindow instead of NewWindow.
TEST_F(PageLoadMetricsInitializeTest, DiscardReloadClassification) {
  base::HistogramTester histogram_tester;

  // Startup is complete.
  ASSERT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  // Clear existing default tab from browser() so it starts empty.
  browser()->tab_strip_model()->CloseAllTabs();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  content::WebContents* raw_web_contents = web_contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /*foreground=*/true);

  // Set the WebContents as discarded.
  raw_web_contents->SetWasDiscarded(true);

  SimulatePageLoad(raw_web_contents, browser());

  // Verify SameWindow suffix histograms are logged (instead of NewWindow).
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.SameWindow",
      base::Milliseconds(100), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.SameWindow",
      base::Milliseconds(200), 1);

  // NewWindow suffix histograms should not be logged.
  histogram_tester.ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NewWindow", 0);
}
