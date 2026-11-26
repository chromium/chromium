// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_delta_serialization.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(HistogramDeltaSerializationTest, DeserializeHistogramAndAddSamples) {
  std::unique_ptr<StatisticsRecorder> statistic_recorder(
      StatisticsRecorder::CreateTemporaryForTesting());
  HistogramDeltaSerialization serializer;
  std::vector<std::string> deltas;
  // Nothing was changed yet.
  serializer.PrepareAndSerializeDeltas(&deltas, true);
  EXPECT_TRUE(deltas.empty());

  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kIPCSerializationSourceFlag);
  histogram->Add(1);
  histogram->Add(10);
  histogram->Add(100);
  histogram->Add(1000);

  serializer.PrepareAndSerializeDeltas(&deltas, true);
  EXPECT_FALSE(deltas.empty());

  HistogramDeltaSerialization::DeserializeAndAddSamples(deltas,
                                                        base::NullCallback());

  // The histogram has kIPCSerializationSourceFlag. So samples will be ignored.
  std::unique_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(1, snapshot->GetCount(1));
  EXPECT_EQ(1, snapshot->GetCount(10));
  EXPECT_EQ(1, snapshot->GetCount(100));
  EXPECT_EQ(1, snapshot->GetCount(1000));

  // Clear kIPCSerializationSourceFlag to emulate multi-process usage.
  histogram->ClearFlags(HistogramBase::kIPCSerializationSourceFlag);
  HistogramDeltaSerialization::DeserializeAndAddSamples(deltas,
                                                        base::NullCallback());

  std::unique_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(2, snapshot2->GetCount(1));
  EXPECT_EQ(2, snapshot2->GetCount(10));
  EXPECT_EQ(2, snapshot2->GetCount(100));
  EXPECT_EQ(2, snapshot2->GetCount(1000));
}

// Tests that the MapperCallback can be used to conditionally rename or drop
// metrics as they are deserialized. This validates the pipeline's ability to
// intercept child process IPC metrics (like Webium) and fork them into separate
// histogram names without needing to hardcode the separation into the sender.
TEST(HistogramDeltaSerializationTest, DeserializeWithMapper) {
  std::unique_ptr<StatisticsRecorder> statistic_recorder(
      StatisticsRecorder::CreateTemporaryForTesting());
  HistogramDeltaSerialization serializer;
  std::vector<std::string> deltas;

  HistogramBase* histogram1 =
      Histogram::FactoryGet("TestHistogram1", 1, 1000, 10,
                            HistogramBase::kIPCSerializationSourceFlag);
  histogram1->Add(1);

  HistogramBase* histogram2 =
      Histogram::FactoryGet("TestHistogram2", 1, 1000, 10,
                            HistogramBase::kIPCSerializationSourceFlag);
  histogram2->Add(2);

  serializer.PrepareAndSerializeDeltas(&deltas, true);
  EXPECT_FALSE(deltas.empty());

  histogram1->ClearFlags(HistogramBase::kIPCSerializationSourceFlag);
  histogram2->ClearFlags(HistogramBase::kIPCSerializationSourceFlag);

  auto mapper = base::BindRepeating(
      [](std::string_view name, int32_t /*flags*/) -> std::string_view {
        if (name == "TestHistogram1") {
          return "";
        }
        if (name == "TestHistogram2") {
          return "RenamedHistogram2";
        }
        return name;
      });

  HistogramDeltaSerialization::DeserializeAndAddSamples(deltas, mapper);

  EXPECT_EQ(1, histogram1->SnapshotSamples()->GetCount(1));
  EXPECT_EQ(1, histogram2->SnapshotSamples()->GetCount(2));

  HistogramBase* renamed =
      StatisticsRecorder::FindHistogram("RenamedHistogram2");
  ASSERT_TRUE(renamed);
  EXPECT_EQ(1, renamed->SnapshotSamples()->GetCount(2));
}

// Verifies that deserialization passes pickled flags (with
// `kIPCSerializationSourceFlag` already cleared) to `mapper` and clears
// `kExemptFromNameMappingFlag` on the resulting browser histograms.
TEST(HistogramDeltaSerializationTest, MapperReceivesFlagsAndClearsExemptFlag) {
  auto recorder = StatisticsRecorder::CreateTemporaryForTesting();
  std::vector<std::string> deltas;

  for (bool exempt : {true, false}) {
    std::string prefix = exempt ? "Exempt" : "Mapped";
    int32_t flags = HistogramBase::kUmaTargetedHistogramFlag |
                    (exempt ? HistogramBase::kExemptFromNameMappingFlag : 0);
    Histogram::FactoryGet(prefix + "H", 1, 100, 10, flags)->Add(1);
    LinearHistogram::FactoryGet(prefix + "L", 1, 10, 10, flags)->Add(1);
    BooleanHistogram::FactoryGet(prefix + "B", flags)->Add(1);
    CustomHistogram::FactoryGet(prefix + "C", {1, 5}, flags)->Add(1);
    SparseHistogram::FactoryGet(prefix + "S", flags)->Add(1);
  }

  HistogramDeltaSerialization().PrepareAndSerializeDeltas(&deltas, true);
  ASSERT_EQ(10u, deltas.size());
  recorder.reset();
  recorder = StatisticsRecorder::CreateTemporaryForTesting();

  base::flat_map<std::string, int32_t> seen_flags;
  auto mapper = base::BindLambdaForTesting(
      [&](std::string_view name, int32_t flags) -> std::string_view {
        seen_flags[std::string(name)] = flags;
        return (flags & HistogramBase::kExemptFromNameMappingFlag) ? name : "";
      });
  HistogramDeltaSerialization::DeserializeAndAddSamples(deltas, mapper);

  for (std::string suffix : {"H", "L", "B", "C", "S"}) {
    auto exempt_it = seen_flags.find("Exempt" + suffix);
    ASSERT_NE(exempt_it, seen_flags.end()) << suffix;
    EXPECT_TRUE(exempt_it->second & HistogramBase::kExemptFromNameMappingFlag);
    EXPECT_FALSE(exempt_it->second &
                 HistogramBase::kIPCSerializationSourceFlag);

    auto mapped_it = seen_flags.find("Mapped" + suffix);
    ASSERT_NE(mapped_it, seen_flags.end()) << suffix;
    EXPECT_FALSE(mapped_it->second & HistogramBase::kExemptFromNameMappingFlag);

    HistogramBase* histogram =
        StatisticsRecorder::FindHistogram("Exempt" + suffix);
    ASSERT_TRUE(histogram) << suffix;
    EXPECT_FALSE(
        histogram->HasFlags(HistogramBase::kExemptFromNameMappingFlag));
    EXPECT_TRUE(histogram->HasFlags(HistogramBase::kUmaTargetedHistogramFlag));
    EXPECT_EQ(1, histogram->SnapshotSamples()->TotalCount());
    EXPECT_FALSE(StatisticsRecorder::FindHistogram("Mapped" + suffix));
  }
}

}  // namespace base
