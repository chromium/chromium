// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_snapshot_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const std::string kHistogramName = "UmaHistogram";

const std::string kStabilityHistogramName = "UmaStabilityHistogram";

void UmaStabilityHistogramBoolean(const std::string& name, bool sample) {
  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      name, HistogramBase::kUmaStabilityHistogramFlag);
  histogram->Add(sample);
}

}  // namespace

class HistogramFlattenerDeltaRecorder : public HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() = default;

  HistogramFlattenerDeltaRecorder(const HistogramFlattenerDeltaRecorder&) =
      delete;
  HistogramFlattenerDeltaRecorder& operator=(
      const HistogramFlattenerDeltaRecorder&) = delete;

  void RecordDelta(const HistogramBase& histogram,
                   const HistogramSamples& snapshot) override {
    recorded_delta_histograms_.push_back(&histogram);
    // Use CHECK instead of ASSERT to get full stack-trace and thus origin.
    CHECK(!Contains(recorded_delta_histogram_sum_, histogram.histogram_name()));
    // Keep pointer to snapshot for testing. This really isn't ideal but the
    // snapshot-manager keeps the snapshot alive until it's "forgotten".
    recorded_delta_histogram_sum_[histogram.histogram_name()] = snapshot.sum();
  }

  void Reset() {
    recorded_delta_histograms_.clear();
    recorded_delta_histogram_sum_.clear();
  }

  std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
  GetRecordedDeltaHistograms() {
    return recorded_delta_histograms_;
  }

  int64_t GetRecordedDeltaHistogramSum(const std::string& name) {
    EXPECT_TRUE(Contains(recorded_delta_histogram_sum_, name));
    return recorded_delta_histogram_sum_[name];
  }

 private:
  std::vector<raw_ptr<const HistogramBase, VectorExperimental>>
      recorded_delta_histograms_;
  std::map<std::string, int64_t> recorded_delta_histogram_sum_;
};

class HistogramSnapshotManagerTest : public testing::Test {
 protected:
  HistogramSnapshotManagerTest()
      : statistics_recorder_(StatisticsRecorder::CreateTemporaryForTesting()),
        histogram_snapshot_manager_(&histogram_flattener_delta_recorder_) {}

  ~HistogramSnapshotManagerTest() override = default;

  int64_t GetRecordedDeltaHistogramSum(const std::string& name) {
    return histogram_flattener_delta_recorder_.GetRecordedDeltaHistogramSum(
        name);
  }

  std::unique_ptr<StatisticsRecorder> statistics_recorder_;
  HistogramFlattenerDeltaRecorder histogram_flattener_delta_recorder_;
  HistogramSnapshotManager histogram_snapshot_manager_;
};

TEST_F(HistogramSnapshotManagerTest, PrepareDeltasNoFlagsFilter) {
  // kNoFlags filter should record all histograms.
  base::UmaHistogramBoolean(kHistogramName, true);
  UmaStabilityHistogramBoolean(kStabilityHistogramName, true);

  StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/false, /*flags_to_set=*/HistogramBase::kNoFlags,
      /*required_flags=*/HistogramBase::kNoFlags, &histogram_snapshot_manager_);

  // Verify that the snapshots were recorded.
  const std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
      histograms =
          histogram_flattener_delta_recorder_.GetRecordedDeltaHistograms();
  ASSERT_EQ(2U, histograms.size());
  ASSERT_EQ(kHistogramName, histograms[0]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kHistogramName), 1);
  ASSERT_EQ(kStabilityHistogramName, histograms[1]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kStabilityHistogramName), 1);

  // The samples should have been marked as logged.
  EXPECT_EQ(histograms[0]->SnapshotUnloggedSamples()->TotalCount(), 0);
  EXPECT_EQ(histograms[1]->SnapshotUnloggedSamples()->TotalCount(), 0);
}

TEST_F(HistogramSnapshotManagerTest, PrepareDeltasUmaHistogramFlagFilter) {
  // Note that kUmaStabilityHistogramFlag includes kUmaTargetedHistogramFlag.
  base::UmaHistogramBoolean(kHistogramName, true);
  UmaStabilityHistogramBoolean(kStabilityHistogramName, true);

  StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/false, /*flags_to_set=*/HistogramBase::kNoFlags,
      /*required_flags=*/HistogramBase::kUmaTargetedHistogramFlag,
      &histogram_snapshot_manager_);

  // Verify that the snapshots were recorded.
  const std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
      histograms =
          histogram_flattener_delta_recorder_.GetRecordedDeltaHistograms();
  ASSERT_EQ(2U, histograms.size());
  ASSERT_EQ(kHistogramName, histograms[0]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kHistogramName), 1);
  ASSERT_EQ(kStabilityHistogramName, histograms[1]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kStabilityHistogramName), 1);

  // The samples should have been marked as logged.
  EXPECT_EQ(histograms[0]->SnapshotUnloggedSamples()->TotalCount(), 0);
  EXPECT_EQ(histograms[1]->SnapshotUnloggedSamples()->TotalCount(), 0);
}

TEST_F(HistogramSnapshotManagerTest,
       PrepareDeltasUmaStabilityHistogramFlagFilter) {
  base::UmaHistogramBoolean(kHistogramName, true);
  UmaStabilityHistogramBoolean(kStabilityHistogramName, true);

  StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/false, /*flags_to_set=*/HistogramBase::kNoFlags,
      /*required_flags=*/HistogramBase::kUmaStabilityHistogramFlag,
      &histogram_snapshot_manager_);

  // Verify that only the stability histogram was snapshotted and recorded.
  const std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
      histograms =
          histogram_flattener_delta_recorder_.GetRecordedDeltaHistograms();
  ASSERT_EQ(1U, histograms.size());
  ASSERT_EQ(kStabilityHistogramName, histograms[0]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kStabilityHistogramName), 1);

  // The samples should have been marked as logged.
  EXPECT_EQ(histograms[0]->SnapshotUnloggedSamples()->TotalCount(), 0);
}

TEST_F(HistogramSnapshotManagerTest, SnapshotUnloggedSamplesNoFlagsFilter) {
  // kNoFlags filter should record all histograms.
  base::UmaHistogramBoolean(kHistogramName, true);
  UmaStabilityHistogramBoolean(kStabilityHistogramName, true);

  StatisticsRecorder::SnapshotUnloggedSamples(
      /*required_flags=*/HistogramBase::kNoFlags, &histogram_snapshot_manager_);

  // Verify that the snapshots were recorded.
  const std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
      histograms =
          histogram_flattener_delta_recorder_.GetRecordedDeltaHistograms();
  ASSERT_EQ(2U, histograms.size());
  ASSERT_EQ(kHistogramName, histograms[0]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kHistogramName), 1);
  ASSERT_EQ(kStabilityHistogramName, histograms[1]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kStabilityHistogramName), 1);

  // The samples should NOT have been marked as logged.
  std::unique_ptr<HistogramSamples> samples =
      histograms[0]->SnapshotUnloggedSamples();
  EXPECT_EQ(samples->TotalCount(), 1);
  EXPECT_EQ(samples->sum(), 1);
  samples = histograms[1]->SnapshotUnloggedSamples();
  EXPECT_EQ(samples->TotalCount(), 1);
  EXPECT_EQ(samples->sum(), 1);

  // Mark the samples as logged and verify that they are correctly marked as so.
  histogram_snapshot_manager_.MarkUnloggedSamplesAsLogged();
  EXPECT_EQ(histograms[0]->SnapshotUnloggedSamples()->TotalCount(), 0);
  EXPECT_EQ(histograms[1]->SnapshotUnloggedSamples()->TotalCount(), 0);
}

TEST_F(HistogramSnapshotManagerTest,
       SnapshotUnloggedSamplesUmaHistogramFlagFilter) {
  // Note that kUmaStabilityHistogramFlag includes kUmaTargetedHistogramFlag.
  base::UmaHistogramBoolean(kHistogramName, true);
  UmaStabilityHistogramBoolean(kStabilityHistogramName, true);

  StatisticsRecorder::SnapshotUnloggedSamples(
      /*required_flags=*/HistogramBase::kUmaTargetedHistogramFlag,
      &histogram_snapshot_manager_);

  // Verify that the snapshots were recorded.
  const std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
      histograms =
          histogram_flattener_delta_recorder_.GetRecordedDeltaHistograms();
  ASSERT_EQ(2U, histograms.size());
  ASSERT_EQ(kHistogramName, histograms[0]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kHistogramName), 1);
  ASSERT_EQ(kStabilityHistogramName, histograms[1]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kStabilityHistogramName), 1);

  // The samples should NOT have been marked as logged.
  std::unique_ptr<HistogramSamples> samples =
      histograms[0]->SnapshotUnloggedSamples();
  EXPECT_EQ(samples->TotalCount(), 1);
  EXPECT_EQ(samples->sum(), 1);
  samples = histograms[1]->SnapshotUnloggedSamples();
  EXPECT_EQ(samples->TotalCount(), 1);
  EXPECT_EQ(samples->sum(), 1);

  // Mark the samples as logged and verify that they are correctly marked as so.
  histogram_snapshot_manager_.MarkUnloggedSamplesAsLogged();
  EXPECT_EQ(histograms[0]->SnapshotUnloggedSamples()->TotalCount(), 0);
  EXPECT_EQ(histograms[1]->SnapshotUnloggedSamples()->TotalCount(), 0);
}

TEST_F(HistogramSnapshotManagerTest,
       SnapshotUnloggedSamplesUmaStabilityHistogramFlagFilter) {
  base::UmaHistogramBoolean(kHistogramName, true);
  UmaStabilityHistogramBoolean(kStabilityHistogramName, true);

  StatisticsRecorder::SnapshotUnloggedSamples(
      /*required_flags=*/HistogramBase::kUmaStabilityHistogramFlag,
      &histogram_snapshot_manager_);

  // Verify that only the stability histogram was snapshotted and recorded.
  const std::vector<raw_ptr<const HistogramBase, VectorExperimental>>&
      histograms =
          histogram_flattener_delta_recorder_.GetRecordedDeltaHistograms();
  ASSERT_EQ(1U, histograms.size());
  ASSERT_EQ(kStabilityHistogramName, histograms[0]->histogram_name());
  EXPECT_EQ(GetRecordedDeltaHistogramSum(kStabilityHistogramName), 1);

  // The samples should NOT have been marked as logged.
  std::unique_ptr<HistogramSamples> samples =
      histograms[0]->SnapshotUnloggedSamples();
  EXPECT_EQ(samples->TotalCount(), 1);
  EXPECT_EQ(samples->sum(), 1);

  // Mark the samples as logged and verify that they are correctly marked as so.
  histogram_snapshot_manager_.MarkUnloggedSamplesAsLogged();
  EXPECT_EQ(histograms[0]->SnapshotUnloggedSamples()->TotalCount(), 0);
}

}  // namespace base
