// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace {
class AnchorElementPreloaderBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public predictors::PreconnectManager::Observer {
 public:
  static constexpr char kOrigin1[] = "https://www.origin1.com/";
  static constexpr char kOrigin2[] = "https://www.origin2.com/";

  virtual void SetFeatures() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAnchorElementInteraction);
  }

  void SetUp() override {
    SetFeatures();
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/preload");
    EXPECT_TRUE(https_server_->Start());
    preresolve_count_ = 0;
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Without this flag, mouse events are suppressed in these tests.
    command_line->AppendSwitch("allow-pre-commit-input");
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            browser()->profile());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);
  }

  void SimulateMouseDownElementWithId(const std::string& id) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::Point point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents, id));

    content::SimulateMouseEvent(web_contents,
                                blink::WebMouseEvent::Type::kMouseDown,
                                blink::WebMouseEvent::Button::kLeft, point);
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void WaitForPreresolveCountForURL(int expected_count) {
    while (preresolve_count_ < expected_count) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // predictors::PreconnectManager::Observer
  // We observe DNS preresolution instead of preconnect, because test
  // servers all resolve to localhost and Chrome won't preconnect
  // given it already has a warm connection.
  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      bool success) override {
    if (url != GURL(kOrigin1) && url != GURL(kOrigin2)) {
      return;
    }

    ++preresolve_count_;
    if (run_loop_)
      run_loop_->Quit();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 protected:
  int preresolve_count_;
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, OneAnchor) {
  const GURL& url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SimulateMouseDownElementWithId("anchor1");

  WaitForPreresolveCountForURL(1);
  EXPECT_EQ(1, preresolve_count_);
  ukm::SourceId ukm_source_id = browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame()
                                    ->GetPageUkmSourceId();

  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 1);

  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});

  EXPECT_EQ(ukm_entries.size(), 1u);

  EXPECT_EQ(ukm_entries[0].source_id, ukm_source_id);
}

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, Duplicates) {
  const GURL& url = GetTestURL("/many_anchors.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ukm::SourceId ukm_source_id = browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetMainFrame()
                                    ->GetPageUkmSourceId();

  // First link with mousedown event should get preconnected.
  SimulateMouseDownElementWithId("anchor1_origin1");
  WaitForPreresolveCountForURL(1);
  EXPECT_EQ(1, preresolve_count_);
  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);
  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 1);
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});
  EXPECT_EQ(ukm_entries.size(), 1u);
  EXPECT_EQ(ukm_entries[0].source_id, ukm_source_id);

  // Second mousedown event to same origin: should not trigger a preconnect.
  SimulateMouseDownElementWithId("anchor2_origin1");
  EXPECT_EQ(1, preresolve_count_);
  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);
  ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});
  EXPECT_EQ(ukm_entries.size(), 1u);

  // Third mousedown event to a different origin: should trigger a preconnect.
  SimulateMouseDownElementWithId("anchor1_origin2");
  WaitForPreresolveCountForURL(2);
  EXPECT_EQ(2, preresolve_count_);
  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 2);
  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 2);
  ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});
  EXPECT_EQ(ukm_entries.size(), 2u);
  EXPECT_EQ(ukm_entries[1].source_id, ukm_source_id);
}

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, InvalidHref) {
  const GURL& url = GetTestURL("/invalid_href_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SimulateMouseDownElementWithId("anchor2");
  EXPECT_EQ(0, preresolve_count_);

  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 0);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 0);

  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});

  EXPECT_EQ(ukm_entries.size(), 0u);
}

// TODO(crbug.com/1318937): Re-enable this test
IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, DISABLED_IframeTest) {
  const GURL& url = GetTestURL("/iframe_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::SimulateMouseEvent(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::WebMouseEvent::Type::kMouseDown,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(200, 200));
  WaitForPreresolveCountForURL(1);
  EXPECT_EQ(1, preresolve_count_);

  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 1);

  ukm::SourceId ukm_source_id = browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame()
                                    ->GetPageUkmSourceId();

  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});

  EXPECT_EQ(ukm_entries.size(), 1u);

  EXPECT_EQ(ukm_entries[0].source_id, ukm_source_id);
}

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest,
                       UserSettingDisabledTest) {
  prefetch::SetPreloadPagesState(browser()->profile()->GetPrefs(),
                                 prefetch::PreloadPagesState::kNoPreloading);
  const GURL& url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SimulateMouseDownElementWithId("anchor1");
  EXPECT_EQ(0, preresolve_count_);

  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 0);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 0);

  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});

  EXPECT_EQ(ukm_entries.size(), 0u);
}

class AnchorElementPreloaderHoldbackBrowserTest
    : public AnchorElementPreloaderBrowserTest {
 public:
  void SetFeatures() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kAnchorElementInteraction,
        {{"preconnect_holdback", "true"}});
  }
};

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderHoldbackBrowserTest,
                       PreconnectHoldbackTest) {
  const GURL& url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  SimulateMouseDownElementWithId("anchor1");
  EXPECT_EQ(0, preresolve_count_);

  while (
      histogram_tester()
          ->GetAllSamples(kPreloadingAnchorElementPreloaderPreloadingTriggered)
          .empty()) {
    base::RunLoop().RunUntilIdle();
  }
  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 1);

  ukm::SourceId ukm_source_id = browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame()
                                    ->GetPageUkmSourceId();

  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_AnchorInteraction::kEntryName,
      {ukm::builders::Preloading_AnchorInteraction::
           kAnchorElementPreloaderTypeName});

  EXPECT_EQ(ukm_entries.size(), 1u);

  EXPECT_EQ(ukm_entries[0].source_id, ukm_source_id);
}
}  // namespace
