// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
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

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  std::unique_ptr<CompanionMetricsLogger> logger_;
};

TEST_F(CompanionMetricsLoggerTest, RecordUiSurfaceShown) {
  base::HistogramTester histogram_tester;

  // Show two surfaces, user clicks one.
  logger_->RecordUiSurfaceShown(UiSurface::kPH, /*child_element_count=*/0);
  logger_->RecordUiSurfaceShown(UiSurface::kCQ, /*child_element_count=*/3);
  logger_->RecordUiSurfaceClicked(UiSurface::kCQ);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.PH.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.CQ.Shown",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Companion.CQ.Clicked",
                                     /*sample=*/true, /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  const char* entry_name = ukm::builders::Companion_PageView::kEntryName;
  EXPECT_EQ(ukm_recorder()->GetEntriesByName(entry_name).size(), 1ul);
  auto* entry = ukm_recorder()->GetEntriesByName(entry_name)[0];
  ukm_recorder()->EntryHasMetric(
      entry, ukm::builders::Companion_PageView::kPH_LastEventName);
  ukm_recorder()->ExpectEntryMetric(
      entry, ukm::builders::Companion_PageView::kPH_LastEventName,
      static_cast<int>(UiEvent::kShown));

  ukm_recorder()->EntryHasMetric(
      entry, ukm::builders::Companion_PageView::kCQ_LastEventName);
  ukm_recorder()->ExpectEntryMetric(
      entry, ukm::builders::Companion_PageView::kCQ_LastEventName,
      static_cast<int>(UiEvent::kClicked));
  ukm_recorder()->EntryHasMetric(
      entry, ukm::builders::Companion_PageView::kCQ_ChildElementCountName);
  ukm_recorder()->ExpectEntryMetric(
      entry, ukm::builders::Companion_PageView::kCQ_ChildElementCountName, 3);
}

TEST_F(CompanionMetricsLoggerTest, RecordPromoEvent) {
  base::HistogramTester histogram_tester;

  // Show a promo, user accepts it.
  logger_->OnPromoAction(PromoType::kSignin, PromoAction::kAccepted);

  // Verify histograms for click and shown events.
  histogram_tester.ExpectBucketCount("Companion.PromoEvent",
                                     PromoEvent::kSignInAccepted,
                                     /*expected_count=*/1);

  // Destroy the logger. Verify that UKM event is recorded.
  logger_.reset();

  const char* entry_name = ukm::builders::Companion_PageView::kEntryName;
  EXPECT_EQ(ukm_recorder()->GetEntriesByName(entry_name).size(), 1ul);
  auto* entry = ukm_recorder()->GetEntriesByName(entry_name)[0];
  ukm_recorder()->EntryHasMetric(
      entry, ukm::builders::Companion_PageView::kPromoEventName);
  ukm_recorder()->ExpectEntryMetric(
      entry, ukm::builders::Companion_PageView::kPromoEventName,
      static_cast<int>(PromoEvent::kSignInAccepted));
}

}  // namespace companion
