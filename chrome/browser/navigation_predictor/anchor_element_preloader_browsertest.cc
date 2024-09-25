// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/preloading.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace {

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;

class AnchorElementPreloaderBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public predictors::PreconnectManager::Observer {
 public:
  static constexpr char kOrigin1[] = "https://www.origin1.com/";
  static constexpr char kOrigin2[] = "https://www.origin2.com/";

  virtual base::FieldTrialParams GetAnchorElementInteractionFieldTrialParams() {
    return {};
  }

  virtual base::FieldTrialParams GetNavigationPredictorFieldTrialParams() {
    return {};
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kNavigationPredictor,
          GetNavigationPredictorFieldTrialParams()},
         {blink::features::kAnchorElementInteraction,
          GetAnchorElementInteractionFieldTrialParams()}},
        {});
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
    ukm_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            chrome_preloading_predictor::kPointerDownOnAnchor);
    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();
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

  void GiveItSomeTime(const base::TimeDelta& t) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), t);
    run_loop.Run();
  }

  // predictors::PreconnectManager::Observer
  // We observe DNS preresolution instead of preconnect, because test
  // servers all resolve to localhost and Chrome won't preconnect
  // given it already has a warm connection.
  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
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

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder& ukm_entry_builder() {
    return *ukm_entry_builder_;
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
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
  // Disable sampling of UKM preloading logs.
  content::test::PreloadingConfigOverride preloading_config_override_;
};

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, OneAnchor) {
  const GURL& url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SimulateMouseDownElementWithId("anchor1");

  WaitForPreresolveCountForURL(1);
  EXPECT_EQ(1, preresolve_count_);
  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 1);

  // Navigate away to the same origin that was preconnected. This should flush
  // the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kOrigin1) + "foo")));
  ukm::SourceId ukm_source_id = GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 1u);
  UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
      ukm_source_id, content::PreloadingType::kPreconnect,
      content::PreloadingEligibility::kEligible,
      content::PreloadingHoldbackStatus::kAllowed,
      content::PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
      content::PreloadingFailureReason::kUnspecified,
      /*accurate=*/true);
  EXPECT_EQ(ukm_entries[0], expected_entry)
      << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[0],
                                                         expected_entry);
}

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, OneAnchorInaccurate) {
  const GURL& url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SimulateMouseDownElementWithId("anchor1");

  WaitForPreresolveCountForURL(1);
  EXPECT_EQ(1, preresolve_count_);
  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 1);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 1);

  // Navigate away to an origin that was not preconnected. This should flush
  // the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kOrigin2) + "foo")));
  ukm::SourceId ukm_source_id = GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 1u);
  UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
      ukm_source_id, content::PreloadingType::kPreconnect,
      content::PreloadingEligibility::kEligible,
      content::PreloadingHoldbackStatus::kAllowed,
      content::PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
      content::PreloadingFailureReason::kUnspecified,
      /*accurate=*/false);
  EXPECT_EQ(ukm_entries[0], expected_entry)
      << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[0],
                                                         expected_entry);
}

// TODO(crbug.com/40255727): Flaky on Win10
#if BUILDFLAG(IS_WIN)
#define MAYBE_Duplicates DISABLED_Duplicates
#else
#define MAYBE_Duplicates Duplicates
#endif
IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest, MAYBE_Duplicates) {
  const GURL& url = GetTestURL("/many_anchors.html");

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // First link with mousedown event should get preconnected.
  SimulateMouseDownElementWithId("anchor1_origin1");
  WaitForPreresolveCountForURL(1);

  // Second mousedown event to same origin: should not trigger a preconnect.
  SimulateMouseDownElementWithId("anchor2_origin1");

  // Third mousedown event to a different origin: should trigger a preconnect.
  SimulateMouseDownElementWithId("anchor1_origin2");
  WaitForPreresolveCountForURL(2);

  // Navigate away to the first origin that was preconnected. This should flush
  // the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kOrigin1) + "foo")));
  ukm::SourceId ukm_source_id = GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 3u);
  std::vector<UkmEntry> expected_entries = {
      // Successful preconnect to first origin.
      ukm_entry_builder().BuildEntry(
          ukm_source_id, content::PreloadingType::kPreconnect,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kAllowed,
          content::PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/true),
      // Duplicate preconnect to first origin.
      ukm_entry_builder().BuildEntry(
          ukm_source_id, content::PreloadingType::kPreconnect,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kAllowed,
          content::PreloadingTriggeringOutcome::kDuplicate,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/true),
      // Preconnect to first second origin.
      ukm_entry_builder().BuildEntry(
          ukm_source_id, content::PreloadingType::kPreconnect,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kAllowed,
          content::PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/false),
  };
  EXPECT_THAT(ukm_entries, testing::UnorderedElementsAreArray(expected_entries))
      << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                           expected_entries);
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

  // Navigate away. This should flush the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 0u);
}

// TODO(crbug.com/40835708): Re-enable this test
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
}

IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderBrowserTest,
                       UserSettingDisabledTest) {
  prefetch::SetPreloadPagesState(browser()->profile()->GetPrefs(),
                                 prefetch::PreloadPagesState::kNoPreloading);
  const GURL& url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SimulateMouseDownElementWithId("anchor1");
  EXPECT_EQ(0, preresolve_count_);

  // Give some time for Preloading APIs creation.
  GiveItSomeTime(base::Milliseconds(100));

  histogram_tester()->ExpectTotalCount(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, 0);

  histogram_tester()->ExpectUniqueSample(
      kPreloadingAnchorElementPreloaderPreloadingTriggered,
      AnchorElementPreloaderType::kPreconnect, 0);

  // Navigate away to the same origin that was preconnected. This should flush
  // the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kOrigin1) + "foo")));
  ukm::SourceId ukm_source_id = GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 1u);
  UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
      ukm_source_id, content::PreloadingType::kPreconnect,
      content::PreloadingEligibility::kPreloadingDisabled,
      content::PreloadingHoldbackStatus::kUnspecified,
      content::PreloadingTriggeringOutcome::kUnspecified,
      content::PreloadingFailureReason::kUnspecified,
      /*accurate=*/true);
  EXPECT_EQ(ukm_entries[0], expected_entry)
      << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[0],
                                                         expected_entry);
}

class AnchorElementPreloaderHoldbackBrowserTest
    : public AnchorElementPreloaderBrowserTest {
 public:
  base::FieldTrialParams GetAnchorElementInteractionFieldTrialParams()
      override {
    return {{"preconnect_holdback", "true"}};
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

  // Navigate away to the same origin that was preconnected. This should flush
  // the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kOrigin1) + "foo")));
  ukm::SourceId ukm_source_id = GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 1u);
  UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
      ukm_source_id, content::PreloadingType::kPreconnect,
      content::PreloadingEligibility::kEligible,
      content::PreloadingHoldbackStatus::kHoldback,
      content::PreloadingTriggeringOutcome::kUnspecified,
      content::PreloadingFailureReason::kUnspecified,
      /*accurate=*/true);
  EXPECT_EQ(ukm_entries[0], expected_entry)
      << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[0],
                                                         expected_entry);
}

class AnchorElementPreloaderLimitedBrowserTest
    : public AnchorElementPreloaderBrowserTest {
 public:
  base::FieldTrialParams GetAnchorElementInteractionFieldTrialParams()
      override {
    return {{"max_preloading_attempts", "1"}};
  }
};

// TODO(crbug.com/40878140): Re-enable this test
IN_PROC_BROWSER_TEST_F(AnchorElementPreloaderLimitedBrowserTest,
                       DISABLED_LimitExceeded) {
  const GURL& url = GetTestURL("/many_anchors.html");

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // First link with mousedown event should get preconnected.
  SimulateMouseDownElementWithId("anchor1_origin1");
  WaitForPreresolveCountForURL(1);

  // Second mousedown event to a different origin: limit should be exceeded.
  SimulateMouseDownElementWithId("anchor1_origin2");

  // Navigate away to the first origin that was preconnected. This should flush
  // the Preloading UKM logs.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kOrigin1) + "foo")));
  ukm::SourceId ukm_source_id = GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Attempt::kEntryName,
      content::test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), 2u);
  std::vector<UkmEntry> expected_entries = {
      // Successful preconnect to first origin.
      ukm_entry_builder().BuildEntry(
          ukm_source_id, content::PreloadingType::kPreconnect,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kAllowed,
          content::PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/true),
      // LimitExceeded for second origin.
      ukm_entry_builder().BuildEntry(
          ukm_source_id, content::PreloadingType::kPreconnect,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kAllowed,
          content::PreloadingTriggeringOutcome::kFailure,
          ToFailureReason(AnchorPreloadingFailureReason::kLimitExceeded),
          /*accurate=*/false),
  };
  EXPECT_THAT(ukm_entries, testing::UnorderedElementsAreArray(expected_entries))
      << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                           expected_entries);
}

class AnchorElementSetIsNavigationInDomainBrowserTest
    : public AnchorElementPreloaderBrowserTest {
 public:
  base::FieldTrialParams GetNavigationPredictorFieldTrialParams() override {
    return {{"random_anchor_sampling_period", "1"}};
  }
};

IN_PROC_BROWSER_TEST_F(AnchorElementSetIsNavigationInDomainBrowserTest,
                       TestPointerDownOnAnchor) {
  base::HistogramTester histogram_tester;
  GURL url = GetTestURL("/one_anchor.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Add a new link and trigger a mouse down event.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              R"(
    let a = document.createElement("a");
    a.id = "link";
    a.href = "https://www.example.com";
    a.innerHTML = '<div style="width:100vw;height:100vh;">Example<div>';
    document.body.appendChild(a);
    )"));
  base::RunLoop().RunUntilIdle();
  content::InputEventAckWaiter waiter(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kMouseDown);
  SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseDown,
                     blink::WebPointerProperties::Button::kLeft,
                     gfx::Point(150, 150));
  waiter.Wait();
  // Add another link and click on it.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              R"(
    let google = document.createElement("a");
    google.id = "link";
    google.href = "https://www.google.com";
    google.innerHTML = "google";
    document.body.appendChild(google);
    google.click();
    )"));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.PointerDownOnAnchor.Recall",
      /*content::PredictorConfusionMatrix::kFalseNegative*/ 3, 1);
}

}  // namespace
