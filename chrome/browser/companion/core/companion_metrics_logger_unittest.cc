// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
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
    auto* entry = ukm_recorder()->GetEntriesByName(entry_name)[0];

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
    auto* entry = entries[entries.size() - 1];

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

TEST_F(CompanionMetricsLoggerTest, RecordUiSurfaceShown) {
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
}

}  // namespace companion
