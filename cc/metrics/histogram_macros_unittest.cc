// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/histogram_macros.h"

#include <memory>
#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"

namespace cc {
namespace {

class HistogramMacroTest : public testing::Test {
 public:
  HistogramMacroTest() { ResetHistograms(); }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(HistogramMacroTest, PercentageGroupHistogramOverflowBucket) {
  int overflow_percentile_value = 130;
  std::string obsolete_histogram_name = "Metrics.ObsoletePercentileMetric";
  std::string new_histogram_name = "Metrics.NewPercentileMetric";
  STATIC_HISTOGRAM_POINTER_GROUP(
      obsolete_histogram_name, 0, 1, Add(overflow_percentile_value),
      base::LinearHistogram::FactoryGet(
          obsolete_histogram_name, 1, 100, 101,
          base::HistogramBase::kUmaTargetedHistogramFlag));
  STATIC_HISTOGRAM_PERCENTAGE_POINTER_GROUP(new_histogram_name, 0, 1,
                                            overflow_percentile_value);
  // Obsolete metrics show the overflow value as being in buckets 100 and 101
  EXPECT_EQ(histogram_tester_->GetBucketCount(obsolete_histogram_name, 100),
            1.0);
  EXPECT_EQ(histogram_tester_->GetBucketCount(obsolete_histogram_name, 101),
            1.0);
  // New metrics show overflow value as being in bucket 101 (overflow)
  // explicitly.
  EXPECT_EQ(histogram_tester_->GetBucketCount(new_histogram_name, 100), 0.0);
  EXPECT_EQ(histogram_tester_->GetBucketCount(new_histogram_name, 101), 1.0);
}

// Tests that the new macro has the same behaviours as
// base::UmaHistogramPercentage.
TEST_F(HistogramMacroTest, PercentageGroupHistogramSameAsUmaPercentage) {
  std::string uma_histogram_name = "Metrics.UmaHistogramPercentage";
  std::string macro_histogram_name = "Metrics.MacroHistogramPercentage";
  for (int percentage = 0; percentage <= 100; percentage++) {
    // Write to both the UMA histogram directly and via macro.
    base::UmaHistogramPercentage(uma_histogram_name, percentage);
    STATIC_HISTOGRAM_PERCENTAGE_POINTER_GROUP(macro_histogram_name, 0, 1,
                                              percentage);
  }
  // Check that data produced by both methods is identical.
  EXPECT_EQ(histogram_tester_->GetAllSamples(macro_histogram_name),
            histogram_tester_->GetAllSamples(uma_histogram_name));
}

}  // namespace
}  // namespace cc
