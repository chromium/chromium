// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_base.h"

#include <limits>
#include <string_view>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class HistogramBaseTest : public testing::Test {
 public:
  HistogramBaseTest() {
    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    ResetStatisticsRecorder();
  }

  HistogramBaseTest(const HistogramBaseTest&) = delete;
  HistogramBaseTest& operator=(const HistogramBaseTest&) = delete;
  ~HistogramBaseTest() override = default;

 protected:
  void ResetStatisticsRecorder() {
    // It is necessary to fully destruct any existing StatisticsRecorder
    // before creating a new one.
    statistics_recorder_.reset();
    statistics_recorder_ = StatisticsRecorder::CreateTemporaryForTesting();
  }

 private:
  std::unique_ptr<StatisticsRecorder> statistics_recorder_;
};

TEST_F(HistogramBaseTest, DeserializeHistogram) {
  HistogramBase* histogram =
      Histogram::FactoryGet("TestHistogram", 1, 1000, 10,
                            (HistogramBase::kUmaTargetedHistogramFlag |
                             HistogramBase::kIPCSerializationSourceFlag));

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);
  HistogramBase* deserialized =
      HistogramBase::DeserializeInfo(&iter, base::NullCallback());
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = HistogramBase::DeserializeInfo(&iter2, base::NullCallback());
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(1, 1000, 10));

  // kIPCSerializationSourceFlag will be cleared.
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeLinearHistogram) {
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);
  HistogramBase* deserialized =
      HistogramBase::DeserializeInfo(&iter, base::NullCallback());
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = HistogramBase::DeserializeInfo(&iter2, base::NullCallback());
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(1, 1000, 10));
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeBooleanHistogram) {
  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      "TestHistogram", HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);
  HistogramBase* deserialized =
      HistogramBase::DeserializeInfo(&iter, base::NullCallback());
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = HistogramBase::DeserializeInfo(&iter2, base::NullCallback());
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(1, 2, 3));
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeCustomHistogram) {
  std::vector<HistogramBase::Sample32> ranges;
  ranges.push_back(13);
  ranges.push_back(5);
  ranges.push_back(9);

  HistogramBase* histogram = CustomHistogram::FactoryGet(
      "TestHistogram", ranges, HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);
  HistogramBase* deserialized =
      HistogramBase::DeserializeInfo(&iter, base::NullCallback());
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = HistogramBase::DeserializeInfo(&iter2, base::NullCallback());
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(5, 13, 4));
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeSparseHistogram) {
  HistogramBase* histogram = SparseHistogram::FactoryGet(
      "TestHistogram", HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);
  HistogramBase* deserialized =
      HistogramBase::DeserializeInfo(&iter, base::NullCallback());
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = HistogramBase::DeserializeInfo(&iter2, base::NullCallback());
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, AddTimeMillisecondsGranularityOverflow) {
  const HistogramBase::Sample32 sample_max =
      std::numeric_limits<HistogramBase::Sample32>::max() / 2;
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      "TestAddTimeMillisecondsGranularity1", 1, sample_max, 100, 0);
  int64_t large_positive = std::numeric_limits<int64_t>::max();
  // |add_count| is the number of large values that have been added to the
  // histogram. We consider a number to be 'large' if it cannot be represented
  // in a HistogramBase::Sample32.
  int add_count = 0;
  while (large_positive > std::numeric_limits<HistogramBase::Sample32>::max()) {
    // Add the TimeDelta corresponding to |large_positive| milliseconds to the
    // histogram.
    histogram->AddTimeMillisecondsGranularity(Milliseconds(large_positive));
    ++add_count;
    // Reduce the value of |large_positive|. The choice of 7 here is
    // arbitrary.
    large_positive /= 7;
  }
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  // All of the reported values must have gone into the max overflow bucket.
  EXPECT_EQ(add_count, samples->GetCount(sample_max));

  // We now perform the analoguous operations, now with negative values with a
  // large absolute value.
  histogram = LinearHistogram::FactoryGet("TestAddTimeMillisecondsGranularity2",
                                          1, sample_max, 100, 0);
  int64_t large_negative = std::numeric_limits<int64_t>::min();
  add_count = 0;
  while (large_negative < std::numeric_limits<HistogramBase::Sample32>::min()) {
    histogram->AddTimeMillisecondsGranularity(Milliseconds(large_negative));
    ++add_count;
    large_negative /= 7;
  }
  samples = histogram->SnapshotSamples();
  // All of the reported values must have gone into the min overflow bucket.
  EXPECT_EQ(add_count, samples->GetCount(0));
}

TEST_F(HistogramBaseTest, AddTimeMicrosecondsGranularityOverflow) {
  // Nothing to test if we don't have a high resolution clock.
  if (!TimeTicks::IsHighResolution()) {
    return;
  }

  const HistogramBase::Sample32 sample_max =
      std::numeric_limits<HistogramBase::Sample32>::max() / 2;
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      "TestAddTimeMicrosecondsGranularity1", 1, sample_max, 100, 0);
  int64_t large_positive = std::numeric_limits<int64_t>::max();
  // |add_count| is the number of large values that have been added to the
  // histogram. We consider a number to be 'large' if it cannot be represented
  // in a HistogramBase::Sample32.
  int add_count = 0;
  while (large_positive > std::numeric_limits<HistogramBase::Sample32>::max()) {
    // Add the TimeDelta corresponding to |large_positive| microseconds to the
    // histogram.
    histogram->AddTimeMicrosecondsGranularity(Microseconds(large_positive));
    ++add_count;
    // Reduce the value of |large_positive|. The choice of 7 here is
    // arbitrary.
    large_positive /= 7;
  }
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  // All of the reported values must have gone into the max overflow bucket.
  EXPECT_EQ(add_count, samples->GetCount(sample_max));

  // We now perform the analoguous operations, now with negative values with a
  // large absolute value.
  histogram = LinearHistogram::FactoryGet("TestAddTimeMicrosecondsGranularity2",
                                          1, sample_max, 100, 0);
  int64_t large_negative = std::numeric_limits<int64_t>::min();
  add_count = 0;
  while (large_negative < std::numeric_limits<HistogramBase::Sample32>::min()) {
    histogram->AddTimeMicrosecondsGranularity(Microseconds(large_negative));
    ++add_count;
    large_negative /= 7;
  }
  samples = histogram->SnapshotSamples();
  // All of the reported values must have gone into the min overflow bucket.
  EXPECT_EQ(add_count, samples->GetCount(0));
}

TEST_F(HistogramBaseTest, DeserializeTypeMismatch) {
  // Create a LinearHistogram and register it in the StatisticsRecorder.
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      "TestMismatchedHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
  Pickle real_pickle;
  histogram->SerializeInfo(&real_pickle);

  // Read the serialized fields using a PickleIterator.
  int type;
  std::string name;
  int flags;
  int declared_min;
  int declared_max;
  uint32_t bucket_count;
  uint32_t range_checksum;

  PickleIterator iter(real_pickle);
  ASSERT_TRUE(iter.ReadInt(&type));
  ASSERT_TRUE(iter.ReadString(&name));
  ASSERT_TRUE(iter.ReadInt(&flags));
  ASSERT_TRUE(iter.ReadInt(&declared_min));
  ASSERT_TRUE(iter.ReadInt(&declared_max));
  ASSERT_TRUE(iter.ReadUInt32(&bucket_count));
  ASSERT_TRUE(iter.ReadUInt32(&range_checksum));

  EXPECT_EQ(LINEAR_HISTOGRAM, type);

  // Verify that deserializing the unmodified base pickle succeeds.
  PickleIterator real_iter(real_pickle);
  HistogramBase* deserialized_real =
      HistogramBase::DeserializeInfo(&real_iter, base::NullCallback());
  EXPECT_EQ(histogram, deserialized_real);
  // And that it succeeds a second time too.
  PickleIterator real_iter2(real_pickle);
  HistogramBase* deserialized_real2 =
      HistogramBase::DeserializeInfo(&real_iter2, base::NullCallback());
  EXPECT_EQ(histogram, deserialized_real2);

  // Construct a modified pickle claiming the type is HISTOGRAM instead of
  // LINEAR_HISTOGRAM.
  Pickle mismatched_pickle;
  mismatched_pickle.WriteInt(HISTOGRAM);  // Mismatched type!
  mismatched_pickle.WriteString(name);
  mismatched_pickle.WriteInt(flags);
  mismatched_pickle.WriteInt(declared_min);
  mismatched_pickle.WriteInt(declared_max);
  mismatched_pickle.WriteUInt32(bucket_count);
  mismatched_pickle.WriteUInt32(range_checksum);

  // Deserialize the mismatched pickle. Because "TestMismatchedHistogram"
  // is already registered as a LINEAR_HISTOGRAM, looking it up as a
  // HISTOGRAM should detect the type mismatch and return nullptr.
  PickleIterator mismatched_iter(mismatched_pickle);
  HistogramBase* deserialized =
      HistogramBase::DeserializeInfo(&mismatched_iter, base::NullCallback());
  EXPECT_FALSE(deserialized);
}

}  // namespace base
