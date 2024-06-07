// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {

class CompanionMetricsLoggerTest : public testing::Test {
 public:
  CompanionMetricsLoggerTest() = default;
  ~CompanionMetricsLoggerTest() override = default;

  void SetUp() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    logger_ = std::make_unique<CompanionMetricsLogger>(/*ukm_source_id=*/2);
  }

  void TearDown() override { ukm_recorder_.reset(); }

  ukm::TestUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectUkmEntry(const char* metric_name, int expected_value) {
    // There should be only one UKM entry of Companion_PageView type.
    const char* entry_name = ukm::builders::Companion_PageView::kEntryName;
    EXPECT_EQ(ukm_recorder()->GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder()->GetEntriesByName(entry_name)[0].get();

    // Verify the metric.
    ukm_recorder()->EntryHasMetric(entry, metric_name);
    ukm_recorder()->ExpectEntryMetric(entry, metric_name, expected_value);
  }

  void TestPromoEvent(PromoType promo_type,
                      PromoAction promo_action,
                      PromoEvent expected_promo_event) {
    logger_ = std::make_unique<CompanionMetricsLogger>(/*ukm_source_id=*/2);
    base::HistogramTester histogram_tester;

    // Show a promo.
    logger_->OnPromoAction(promo_type, promo_action);
    histogram_tester.ExpectBucketCount("Companion.PromoEvent",
                                       expected_promo_event,
                                       /*expected_count=*/1);
    // Destroy the logger. Verify the last UKM event of Companion_PageView type.
    logger_.reset();
    const char* entry_name = ukm::builders::Companion_PageView::kEntryName;
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);
    auto* entry = entries[entries.size() - 1].get();

    ukm_recorder()->EntryHasMetric(
        entry, ukm::builders::Companion_PageView::kPromoEventName);
    ukm_recorder()->ExpectEntryMetric(
        entry, ukm::builders::Companion_PageView::kPromoEventName,
        static_cast<int>(expected_promo_event));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  std::unique_ptr<CompanionMetricsLogger> logger_;
};

TEST_F(CompanionMetricsLoggerTest, RecordOpenTrigger) {
  base::HistogramTester histogram_tester;
  logger_->RecordOpenTrigger(SidePanelOpenTrigger::kContextMenuSearchOption);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kOpenTriggerName,
      static_cast<int>(SidePanelOpenTrigger::kContextMenuSearchOption));
}

TEST_F(CompanionMetricsLoggerTest, TextSearch) {
  logger_->RecordUiSurfaceClicked(UiSurface::kSearchBox, kInvalidPosition);
  logger_.reset();
  ExpectUkmEntry(ukm::builders::Companion_PageView::kTextSearchCountName, 1);
}

TEST_F(CompanionMetricsLoggerTest, TextSearchMaxClamp) {
  for (auto i = 1; i <= 11; i++) {
    logger_->RecordUiSurfaceClicked(UiSurface::kSearchBox, kInvalidPosition);
  }
  logger_.reset();
  ExpectUkmEntry(ukm::builders::Companion_PageView::kTextSearchCountName, 10);
}

TEST_F(CompanionMetricsLoggerTest, RegionSearchClicks) {
  base::HistogramTester histogram_tester;

  logger_->RecordUiSurfaceClicked(UiSurface::kRegionSearch, kInvalidPosition);
  logger_->RecordUiSurfaceClicked(UiSurface::kRegionSearch, kInvalidPosition);
  logger_->RecordUiSurfaceClicked(UiSurface::kRegionSearch, kInvalidPosition);

  histogram_tester.ExpectBucketCount("Companion.RegionSearch.Clicked",
                                     /*sample=*/true, /*expected_count=*/3);
  logger_.reset();
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kRegionSearch_ClickCountName, 3);
}

TEST_F(CompanionMetricsLoggerTest, RecordPhFeedback) {
  base::HistogramTester histogram_tester;

  // Show a promo, user accepts it.
  logger_->OnPhFeedback(PhFeedback::kThumbsDown);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  ExpectUkmEntry(ukm::builders::Companion_PageView::kPH_FeedbackName,
                 static_cast<int>(PhFeedback::kThumbsDown));
  histogram_tester.ExpectBucketCount("Companion.PHFeedback.Result",
                                     PhFeedback::kThumbsDown, 1);
}

TEST_F(CompanionMetricsLoggerTest, RecordVQSResultCount) {
  VisualSuggestionsMetrics metrics;
  metrics.results_count = 6;
  metrics.eligible_count = 200;
  metrics.shoppy_count = 100;
  metrics.sensitive_count = 65;
  metrics.shoppy_nonsensitive_count = 17;
  logger_->OnVisualSuggestionsResult(metrics);

  // Bucketed counts of the above metrics with 1.3 exponential factor,
  // rounded down to uint32.
  uint32_t result_bucketed = 5;
  uint32_t eligible_bucketed = 191;
  uint32_t shoppy_bucketed = 87;
  uint32_t sensitive_bucketed = 52;
  uint32_t shoppy_nonsensitive_bucketed = 14;

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kVQS_VisualSearchTriggeredCountName,
      static_cast<unsigned int>(result_bucketed));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kVQS_VisualEligibleImagesCountName,
      static_cast<unsigned int>(eligible_bucketed));
  ExpectUkmEntry(ukm::builders::Companion_PageView::kVQS_ImageShoppyCountName,
                 static_cast<unsigned int>(shoppy_bucketed));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kVQS_ImageSensitiveCountName,
      static_cast<unsigned int>(sensitive_bucketed));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kVQS_ImageShoppyNotSensitiveCountName,
      static_cast<unsigned int>(shoppy_nonsensitive_bucketed));
}

TEST_F(CompanionMetricsLoggerTest, TwoSurfaces_PH_and_CQ) {
  base::HistogramTester histogram_tester;

  // Show two surfaces, user clicks one.
  logger_->RecordUiSurfaceShown(UiSurface::kPH, /*ui_surface_position=*/2,
                                /*child_element_available_count=*/-1,
                                /*child_element_shown_count=*/-1);
  logger_->RecordUiSurfaceShown(UiSurface::kCQ, /*ui_surface_position=*/4,
                                /*child_element_available_count=*/8,
                                /*child_element_shown_count=*/3);
  logger_->RecordUiSurfaceClicked(UiSurface::kCQ, /*click_position=*/2);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.PH.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.CQ.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.CQ.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.CQ.ClickPosition",
                                     /*sample=*/2, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.CQ.ChildElementCount",
                                     /*sample=*/3, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // PH metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPH_LastEventName,
                 static_cast<int>(UiEvent::kShown));
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPH_ComponentPositionName,
                 2);

  // CQ metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kCQ_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(ukm::builders::Companion_PageView::kCQ_NumEntriesAvailableName,
                 8);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kCQ_NumEntriesShownName, 3);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kCQ_ComponentPositionName,
                 4);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kCQ_ClickPositionName, 2);
}

TEST_F(CompanionMetricsLoggerTest, VQ) {
  base::HistogramTester histogram_tester;

  // Show VQ.
  logger_->RecordUiSurfaceShown(UiSurface::kVQ, /*ui_surface_position=*/4,
                                /*child_element_available_count=*/8,
                                /*child_element_shown_count=*/3);
  logger_->RecordUiSurfaceClicked(UiSurface::kVQ, /*click_position=*/2);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.VQ.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.VQ.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.VQ.ClickPosition",
                                     /*sample=*/2, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.VQ.ChildElementCount",
                                     /*sample=*/3, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // VQ metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kVQ_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(ukm::builders::Companion_PageView::kVQ_NumEntriesAvailableName,
                 8);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kVQ_NumEntriesShownName, 3);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kVQ_ComponentPositionName,
                 4);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kVQ_ClickPositionName, 2);
}

TEST_F(CompanionMetricsLoggerTest, RelQr) {
  base::HistogramTester histogram_tester;

  // Show RelQr.
  logger_->RecordUiSurfaceShown(UiSurface::kRelQr, /*ui_surface_position=*/4,
                                /*child_element_available_count=*/8,
                                /*child_element_shown_count=*/3);
  logger_->RecordUiSurfaceClicked(UiSurface::kRelQr, /*click_position=*/2);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.RelQr.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.RelQr.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.RelQr.ClickPosition",
                                     /*sample=*/2, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.RelQr.ChildElementCount",
                                     /*sample=*/3, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // RelQr metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kRelQr_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kRelQr_NumEntriesAvailableName, 8);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kRelQr_NumEntriesShownName,
                 3);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kRelQr_ComponentPositionName, 4);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kRelQr_ClickPositionName,
                 2);
}

TEST_F(CompanionMetricsLoggerTest, RelQs) {
  base::HistogramTester histogram_tester;

  // Show RelQs.
  logger_->RecordUiSurfaceShown(UiSurface::kRelQs, /*ui_surface_position=*/4,
                                /*child_element_available_count=*/8,
                                /*child_element_shown_count=*/3);
  logger_->RecordUiSurfaceClicked(UiSurface::kRelQs, /*click_position=*/2);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.RelQs.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.RelQs.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.RelQs.ClickPosition",
                                     /*sample=*/2, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.RelQs.ChildElementCount",
                                     /*sample=*/3, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // RelQs metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kRelQs_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kRelQs_NumEntriesAvailableName, 8);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kRelQs_NumEntriesShownName,
                 3);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kRelQs_ComponentPositionName, 4);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kRelQs_ClickPositionName,
                 2);
}

TEST_F(CompanionMetricsLoggerTest, PageEntities) {
  base::HistogramTester histogram_tester;

  // Show PageEntities.
  logger_->RecordUiSurfaceShown(UiSurface::kPageEntities,
                                /*ui_surface_position=*/4,
                                /*child_element_available_count=*/8,
                                /*child_element_shown_count=*/3);
  logger_->RecordUiSurfaceClicked(UiSurface::kPageEntities,
                                  /*click_position=*/2);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.PageEntities.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.PageEntities.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.PageEntities.ClickPosition",
                                     /*sample=*/2, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.PageEntities.ChildElementCount",
                                     /*sample=*/3, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // PageEntities metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPageEntities_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPageEntities_NumEntriesAvailableName,
      8);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPageEntities_NumEntriesShownName, 3);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPageEntities_ComponentPositionName,
      4);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPageEntities_ClickPositionName, 2);
}

TEST_F(CompanionMetricsLoggerTest, ATX) {
  base::HistogramTester histogram_tester;

  // Show ATX.
  logger_->RecordUiSurfaceShown(UiSurface::kATX, /*ui_surface_position=*/4,
                                /*child_element_available_count=*/-1,
                                /*child_element_shown_count=*/-1);
  logger_->RecordUiSurfaceClicked(UiSurface::kATX, /*click_position=*/-1);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.ATX.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.ATX.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // RelQs metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kATX_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(ukm::builders::Companion_PageView::kATX_ComponentPositionName,
                 4);
}

TEST_F(CompanionMetricsLoggerTest, RecordPromoEvent) {
  TestPromoEvent(PromoType::kSignin, PromoAction::kAccepted,
                 PromoEvent::kSignInAccepted);
  TestPromoEvent(PromoType::kSignin, PromoAction::kRejected,
                 PromoEvent::kSignInRejected);
  TestPromoEvent(PromoType::kSignin, PromoAction::kShown,
                 PromoEvent::kSignInShown);
  TestPromoEvent(PromoType::kMsbb, PromoAction::kAccepted,
                 PromoEvent::kMsbbAccepted);
  TestPromoEvent(PromoType::kMsbb, PromoAction::kRejected,
                 PromoEvent::kMsbbRejected);
  TestPromoEvent(PromoType::kMsbb, PromoAction::kShown, PromoEvent::kMsbbShown);
  TestPromoEvent(PromoType::kExps, PromoAction::kAccepted,
                 PromoEvent::kExpsAccepted);
  TestPromoEvent(PromoType::kExps, PromoAction::kRejected,
                 PromoEvent::kExpsRejected);
  TestPromoEvent(PromoType::kExps, PromoAction::kShown, PromoEvent::kExpsShown);
  TestPromoEvent(PromoType::kPco, PromoAction::kAccepted,
                 PromoEvent::kPcoAccepted);
  TestPromoEvent(PromoType::kPco, PromoAction::kRejected,
                 PromoEvent::kPcoRejected);
  TestPromoEvent(PromoType::kPco, PromoAction::kShown, PromoEvent::kPcoShown);
}

TEST_F(CompanionMetricsLoggerTest, PHShown) {
  base::HistogramTester histogram_tester;

  logger_->RecordUiSurfaceShown(UiSurface::kPH, /*ui_surface_position=*/2,
                                /*child_element_available_count=*/-1,
                                /*child_element_shown_count=*/-1);
  logger_->RecordUiSurfaceShown(UiSurface::kPHResult, /*ui_surface_position=*/2,
                                /*child_element_available_count=*/3,
                                /*child_element_shown_count=*/1);
  logger_->RecordUiSurfaceClicked(UiSurface::kPHResult, /*click_position=*/1);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.PH.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.PHResult.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.PHResult.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  // PH metrics.
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPH_LastEventName,
                 static_cast<int>(UiEvent::kShown));
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPH_ComponentPositionName,
                 2);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPHResult_LastEventName,
                 static_cast<int>(UiEvent::kClicked));
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPHResult_ComponentPositionName, 2);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPHResult_NumEntriesAvailableName, 3);
  ExpectUkmEntry(
      ukm::builders::Companion_PageView::kPHResult_NumEntriesShownName, 1);
  ExpectUkmEntry(ukm::builders::Companion_PageView::kPHResult_ClickPositionName,
                 1);
}

}  // namespace companion
