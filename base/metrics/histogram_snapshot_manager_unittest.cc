// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_snapshot_manager.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/macros.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class HistogramFlattenerDeltaRecorder : public HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() = default;

  void RecordDelta(const HistogramBase& histogram,
                   const HistogramSamples& snapshot) override {
    recorded_delta_histogram_names_.push_back(histogram.histogram_name());
    // Use CHECK instead of ASSERT to get full stack-trace and thus origin.
    CHECK(!Contains(recorded_delta_histogram_sum_, histogram.histogram_name()));
    // Keep pointer to snapshot for testing. This really isn't ideal but the
    // snapshot-manager keeps the snapshot alive until it's "forgotten".
    recorded_delta_histogram_sum_[histogram.histogram_name()] = snapshot.sum();
  }

  void Reset() {
    recorded_delta_histogram_names_.clear();
    recorded_delta_histogram_sum_.clear();
  }

  std::vector<std::string> GetRecordedDeltaHistogramNames() {
    return recorded_delta_histogram_names_;
  }

  int64_t GetRecordedDeltaHistogramSum(const std::string& name) {
    EXPECT_TRUE(Contains(recorded_delta_histogram_sum_, name));
    return recorded_delta_histogram_sum_[name];
  }

 private:
  std::vector<std::string> recorded_delta_histogram_names_;
  std::map<std::string, int64_t> recorded_delta_histogram_sum_;

  DISALLOW_COPY_AND_ASSIGN(HistogramFlattenerDeltaRecorder);
};

class HistogramSnapshotManagerTest : public testing::Test {
 protected:
  HistogramSnapshotManagerTest()
      : statistics_recorder_(StatisticsRecorder::CreateTemporaryForTesting()),
        histogram_snapshot_manager_(&histogram_flattener_delta_recorder_) {}

  ~HistogramSnapshotManagerTest() override = default;

  std::unique_ptr<StatisticsRecorder> statistics_recorder_;
  HistogramFlattenerDeltaRecorder histogram_flattener_delta_recorder_;
  HistogramSnapshotManager histogram_snapshot_manager_;
};

TEST_F(HistogramSnapshotManagerTest, PrepareDeltasNoFlagsFilter) {
  // kNoFlags filter should record all histograms.
  UMA_HISTOGRAM_ENUMERATION("UmaHistogram", 1, 4);
  UMA_STABILITY_HISTOGRAM_ENUMERATION("UmaStabilityHistogram", 1, 2);

  StatisticsRecorder::PrepareDeltas(false, HistogramBase::kNoFlags,
                                    HistogramBase::kNoFlags,
                                    &histogram_snapshot_manager_);

  const std::vector<std::string>& histograms =
      histogram_flattener_delta_recorder_.GetRecordedDeltaHistogramNames();
  EXPECT_EQ(2U, histograms.size());
  EXPECT_EQ("UmaHistogram", histograms[0]);
  EXPECT_EQ("UmaStabilityHistogram", histograms[1]);
}

TEST_F(HistogramSnapshotManagerTest, PrepareDeltasUmaHistogramFlagFilter) {
  // Note that kUmaStabilityHistogramFlag includes kUmaTargetedHistogramFlag.
  UMA_HISTOGRAM_ENUMERATION("UmaHistogram", 1, 4);
  UMA_STABILITY_HISTOGRAM_ENUMERATION("UmaStabilityHistogram", 1, 2);

  StatisticsRecorder::PrepareDeltas(false, HistogramBase::kNoFlags,
                                    HistogramBase::kUmaTargetedHistogramFlag,
                                    &histogram_snapshot_manager_);

  const std::vector<std::string>& histograms =
      histogram_flattener_delta_recorder_.GetRecordedDeltaHistogramNames();
  EXPECT_EQ(2U, histograms.size());
  EXPECT_EQ("UmaHistogram", histograms[0]);
  EXPECT_EQ("UmaStabilityHistogram", histograms[1]);
}

TEST_F(HistogramSnapshotManagerTest,
       PrepareDeltasUmaStabilityHistogramFlagFilter) {
  UMA_HISTOGRAM_ENUMERATION("UmaHistogram", 1, 4);
  UMA_STABILITY_HISTOGRAM_ENUMERATION("UmaStabilityHistogram", 1, 2);

  StatisticsRecorder::PrepareDeltas(false, HistogramBase::kNoFlags,
                                    HistogramBase::kUmaStabilityHistogramFlag,
                                    &histogram_snapshot_manager_);

  const std::vector<std::string>& histograms =
      histogram_flattener_delta_recorder_.GetRecordedDeltaHistogramNames();
  EXPECT_EQ(1U, histograms.size());
  EXPECT_EQ("UmaStabilityHistogram", histograms[0]);
}

}  // namespace base
