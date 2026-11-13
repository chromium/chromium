// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace dino {

TEST(DinoMetricsTest, RecordDinoImpressionSource) {
  base::HistogramTester histogram_tester;

  RecordDinoImpressionSource(DinoImpressionSource::kTyped);
  RecordDinoImpressionSource(DinoImpressionSource::kNetworkErrorPage);

  histogram_tester.ExpectBucketCount(kDinoImpressionSourceHistogram,
                                     DinoImpressionSource::kTyped, 1);
  histogram_tester.ExpectBucketCount(kDinoImpressionSourceHistogram,
                                     DinoImpressionSource::kNetworkErrorPage,
                                     1);
  histogram_tester.ExpectTotalCount(kDinoImpressionSourceHistogram, 2);
}

TEST(DinoMetricsTest, GetDinoImpressionSourceFromTransition) {
  EXPECT_EQ(DinoImpressionSource::kTyped,
            GetDinoImpressionSourceFromTransition(ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(
      DinoImpressionSource::kTyped,
      GetDinoImpressionSourceFromTransition(ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  EXPECT_EQ(
      DinoImpressionSource::kOmniboxGenerated,
      GetDinoImpressionSourceFromTransition(ui::PAGE_TRANSITION_GENERATED));
  EXPECT_EQ(DinoImpressionSource::kLink,
            GetDinoImpressionSourceFromTransition(ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(DinoImpressionSource::kOther,
            GetDinoImpressionSourceFromTransition(ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(
      DinoImpressionSource::kOther,
      GetDinoImpressionSourceFromTransition(ui::PAGE_TRANSITION_AUTO_TOPLEVEL));
}

}  // namespace dino
