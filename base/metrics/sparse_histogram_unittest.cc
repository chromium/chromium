// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sparse_histogram.h"

#include <memory>
#include <string>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Test parameter indicates if a persistent memory allocator should be used
// for histogram allocation. False will allocate histograms from the process
// heap.
class SparseHistogramTest : public testing::TestWithParam<bool> {
 public:
  SparseHistogramTest() : use_persistent_histogram_allocator_(GetParam()) {}
  SparseHistogramTest(const SparseHistogramTest&) = delete;
  SparseHistogramTest& operator=(const SparseHistogramTest&) = delete;

 protected:
  const int32_t kAllocatorMemorySize = 8 << 20;  // 8 MiB

  using CountAndBucketData = base::SparseHistogram::CountAndBucketData;

  void SetUp() override {
    if (use_persistent_histogram_allocator_)
      CreatePersistentMemoryAllocator();

    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    InitializeStatisticsRecorder();
  }

  void TearDown() override {
    if (allocator_) {
      ASSERT_FALSE(allocator_->IsFull());
      ASSERT_FALSE(allocator_->IsCorrupt());
    }
    UninitializeStatisticsRecorder();
    DestroyPersistentMemoryAllocator();
  }

  void InitializeStatisticsRecorder() {
    DCHECK(!statistics_recorder_);
    statistics_recorder_ = StatisticsRecorder::CreateTemporaryForTesting();
  }

  void UninitializeStatisticsRecorder() {
    statistics_recorder_.reset();
  }

  void CreatePersistentMemoryAllocator() {
    GlobalHistogramAllocator::CreateWithLocalMemory(
        kAllocatorMemorySize, 0, "SparseHistogramAllocatorTest");
    allocator_ = GlobalHistogramAllocator::Get()->memory_allocator();
  }

  void DestroyPersistentMemoryAllocator() {
    allocator_ = nullptr;
    GlobalHistogramAllocator::ReleaseForTesting();
  }

  std::unique_ptr<SparseHistogram> NewSparseHistogram(const char* name) {
    // std::make_unique can't access protected ctor so do it manually. This
    // test class is a friend so can access it.
    return std::unique_ptr<SparseHistogram>(new SparseHistogram(name));
  }

  CountAndBucketData GetCountAndBucketData(SparseHistogram* histogram) {
    // A simple wrapper around |GetCountAndBucketData| to make it visible for
    // testing.
    return histogram->GetCountAndBucketData();
  }

  const bool use_persistent_histogram_allocator_;

  std::unique_ptr<StatisticsRecorder> statistics_recorder_;
  PersistentMemoryAllocator* allocator_ = nullptr;
};

// Run all HistogramTest cases with both heap and persistent memory.
INSTANTIATE_TEST_SUITE_P(HeapAndPersistent,
                         SparseHistogramTest,
                         testing::Bool());

TEST_P(SparseHistogramTest, BasicTest) {
  std::unique_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  std::unique_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->Add(100);
  std::unique_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(1, snapshot1->TotalCount());
  EXPECT_EQ(1, snapshot1->GetCount(100));

  histogram->Add(100);
  histogram->Add(101);
  std::unique_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(3, snapshot2->TotalCount());
  EXPECT_EQ(2, snapshot2->GetCount(100));
  EXPECT_EQ(1, snapshot2->GetCount(101));
}

TEST_P(SparseHistogramTest, BasicTestAddCount) {
  std::unique_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  std::unique_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->AddCount(100, 15);
  std::unique_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(15, snapshot1->TotalCount());
  EXPECT_EQ(15, snapshot1->GetCount(100));

  histogram->AddCount(100, 15);
  histogram->AddCount(101, 25);
  std::unique_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(55, snapshot2->TotalCount());
  EXPECT_EQ(30, snapshot2->GetCount(100));
  EXPECT_EQ(25, snapshot2->GetCount(101));
}

TEST_P(SparseHistogramTest, AddCount_LargeValuesDontOverflow) {
  std::unique_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  std::unique_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->AddCount(1000000000, 15);
  std::unique_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(15, snapshot1->TotalCount());
  EXPECT_EQ(15, snapshot1->GetCount(1000000000));

  histogram->AddCount(1000000000, 15);
  histogram->AddCount(1010000000, 25);
  std::unique_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(55, snapshot2->TotalCount());
  EXPECT_EQ(30, snapshot2->GetCount(1000000000));
  EXPECT_EQ(25, snapshot2->GetCount(1010000000));
  EXPECT_EQ(55250000000LL, snapshot2->sum());
}

// Make sure that counts returned by Histogram::SnapshotDelta do not overflow
// even when a total count (returned by Histogram::SnapshotSample) does.
TEST_P(SparseHistogramTest, AddCount_LargeCountsDontOverflow) {
  std::unique_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  std::unique_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  const int count = (1 << 30) - 1;

  // Repeat N times to make sure that there is no internal value overflow.
  for (int i = 0; i < 10; ++i) {
    histogram->AddCount(42, count);
    std::unique_ptr<HistogramSamples> samples = histogram->SnapshotDelta();
    EXPECT_EQ(count, samples->TotalCount());
    EXPECT_EQ(count, samples->GetCount(42));
  }
}

TEST_P(SparseHistogramTest, MacroBasicTest) {
  UmaHistogramSparse("Sparse", 100);
  UmaHistogramSparse("Sparse", 200);
  UmaHistogramSparse("Sparse", 100);

  const StatisticsRecorder::Histograms histograms =
      StatisticsRecorder::GetHistograms();

  ASSERT_THAT(histograms, testing::SizeIs(1));
  const HistogramBase* const sparse_histogram = histograms[0];

  EXPECT_EQ(SPARSE_HISTOGRAM, sparse_histogram->GetHistogramType());
  EXPECT_EQ("Sparse", StringPiece(sparse_histogram->histogram_name()));
  EXPECT_EQ(
      HistogramBase::kUmaTargetedHistogramFlag |
          (use_persistent_histogram_allocator_ ? HistogramBase::kIsPersistent
                                               : 0),
      sparse_histogram->flags());

  std::unique_ptr<HistogramSamples> samples =
      sparse_histogram->SnapshotSamples();
  EXPECT_EQ(3, samples->TotalCount());
  EXPECT_EQ(2, samples->GetCount(100));
  EXPECT_EQ(1, samples->GetCount(200));
}

TEST_P(SparseHistogramTest, MacroInLoopTest) {
  // Unlike the macros in histogram.h, SparseHistogram macros can have a
  // variable as histogram name.
  for (int i = 0; i < 2; i++) {
    UmaHistogramSparse(StringPrintf("Sparse%d", i), 100);
  }

  const StatisticsRecorder::Histograms histograms =
      StatisticsRecorder::Sort(StatisticsRecorder::GetHistograms());
  ASSERT_THAT(histograms, testing::SizeIs(2));
  EXPECT_STREQ(histograms[0]->histogram_name(), "Sparse0");
  EXPECT_STREQ(histograms[1]->histogram_name(), "Sparse1");
}

TEST_P(SparseHistogramTest, Serialize) {
  std::unique_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  histogram->SetFlags(HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);

  int type;
  EXPECT_TRUE(iter.ReadInt(&type));
  EXPECT_EQ(SPARSE_HISTOGRAM, type);

  std::string name;
  EXPECT_TRUE(iter.ReadString(&name));
  EXPECT_EQ("Sparse", name);

  int flag;
  EXPECT_TRUE(iter.ReadInt(&flag));
  EXPECT_EQ(HistogramBase::kIPCSerializationSourceFlag, flag);

  // No more data in the pickle.
  EXPECT_FALSE(iter.SkipBytes(1));
}

// Ensure that race conditions that cause multiple, identical sparse histograms
// to be created will safely resolve to a single one.
TEST_P(SparseHistogramTest, DuplicationSafety) {
  const char histogram_name[] = "Duplicated";
  size_t histogram_count = StatisticsRecorder::GetHistogramCount();

  // Create a histogram that we will later duplicate.
  HistogramBase* original =
      SparseHistogram::FactoryGet(histogram_name, HistogramBase::kNoFlags);
  ++histogram_count;
  DCHECK_EQ(histogram_count, StatisticsRecorder::GetHistogramCount());
  original->Add(1);

  // Create a duplicate. This has to happen differently depending on where the
  // memory is taken from.
  if (use_persistent_histogram_allocator_) {
    // To allocate from persistent memory, clear the last_created reference in
    // the GlobalHistogramAllocator. This will cause an Import to recreate
    // the just-created histogram which will then be released as a duplicate.
    GlobalHistogramAllocator::Get()->ClearLastCreatedReferenceForTesting();
    // Creating a different histogram will first do an Import to ensure it
    // hasn't been created elsewhere, triggering the duplication and release.
    SparseHistogram::FactoryGet("something.new", HistogramBase::kNoFlags);
    ++histogram_count;
  } else {
    // To allocate from the heap, just call the (private) constructor directly.
    // Delete it immediately like would have happened within FactoryGet();
    std::unique_ptr<SparseHistogram> something =
        NewSparseHistogram(histogram_name);
    DCHECK_NE(original, something.get());
  }
  DCHECK_EQ(histogram_count, StatisticsRecorder::GetHistogramCount());

  // Re-creating the histogram via FactoryGet() will return the same one.
  HistogramBase* duplicate =
      SparseHistogram::FactoryGet(histogram_name, HistogramBase::kNoFlags);
  DCHECK_EQ(original, duplicate);
  DCHECK_EQ(histogram_count, StatisticsRecorder::GetHistogramCount());
  duplicate->Add(2);

  // Ensure that original histograms are still cross-functional.
  original->Add(2);
  duplicate->Add(1);
  std::unique_ptr<HistogramSamples> snapshot_orig = original->SnapshotSamples();
  std::unique_ptr<HistogramSamples> snapshot_dup = duplicate->SnapshotSamples();
  DCHECK_EQ(2, snapshot_orig->GetCount(2));
  DCHECK_EQ(2, snapshot_dup->GetCount(1));
}

TEST_P(SparseHistogramTest, FactoryTime) {
  const int kTestCreateCount = 1 << 10;  // Must be power-of-2.
  const int kTestLookupCount = 100000;
  const int kTestAddCount = 100000;

  // Create all histogram names in advance for accurate timing below.
  std::vector<std::string> histogram_names;
  for (int i = 0; i < kTestCreateCount; ++i) {
    histogram_names.push_back(
        StringPrintf("TestHistogram.%d", i % kTestCreateCount));
  }

  // Calculate cost of creating histograms.
  TimeTicks create_start = TimeTicks::Now();
  for (int i = 0; i < kTestCreateCount; ++i)
    SparseHistogram::FactoryGet(histogram_names[i], HistogramBase::kNoFlags);
  TimeDelta create_ticks = TimeTicks::Now() - create_start;
  int64_t create_ms = create_ticks.InMilliseconds();

  VLOG(1) << kTestCreateCount << " histogram creations took " << create_ms
          << "ms or about "
          << (create_ms * 1000000) / kTestCreateCount
          << "ns each.";

  // Calculate cost of looking up existing histograms.
  TimeTicks lookup_start = TimeTicks::Now();
  for (int i = 0; i < kTestLookupCount; ++i) {
    // 6007 is co-prime with kTestCreateCount and so will do lookups in an
    // order less likely to be cacheable (but still hit them all) should the
    // underlying storage use the exact histogram name as the key.
    const int i_mult = 6007;
    static_assert(i_mult < INT_MAX / kTestCreateCount, "Multiplier too big");
    int index = (i * i_mult) & (kTestCreateCount - 1);
    SparseHistogram::FactoryGet(histogram_names[index],
                                HistogramBase::kNoFlags);
  }
  TimeDelta lookup_ticks = TimeTicks::Now() - lookup_start;
  int64_t lookup_ms = lookup_ticks.InMilliseconds();

  VLOG(1) << kTestLookupCount << " histogram lookups took " << lookup_ms
          << "ms or about "
          << (lookup_ms * 1000000) / kTestLookupCount
          << "ns each.";

  // Calculate cost of accessing histograms.
  HistogramBase* histogram =
      SparseHistogram::FactoryGet(histogram_names[0], HistogramBase::kNoFlags);
  ASSERT_TRUE(histogram);
  TimeTicks add_start = TimeTicks::Now();
  for (int i = 0; i < kTestAddCount; ++i)
    histogram->Add(i & 127);
  TimeDelta add_ticks = TimeTicks::Now() - add_start;
  int64_t add_ms = add_ticks.InMilliseconds();

  VLOG(1) << kTestAddCount << " histogram adds took " << add_ms
          << "ms or about "
          << (add_ms * 1000000) / kTestAddCount
          << "ns each.";
}

TEST_P(SparseHistogramTest, ExtremeValues) {
  static const struct {
    Histogram::Sample sample;
    int64_t expected_max;
  } cases[] = {
      // Note: We use -2147483647 - 1 rather than -2147483648 because the later
      // is interpreted as - operator applied to 2147483648 and the latter can't
      // be represented as an int32 and causes a warning.
      {-2147483647 - 1, -2147483647LL},
      {0, 1},
      {2147483647, 2147483648LL},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    HistogramBase* histogram =
        SparseHistogram::FactoryGet(StringPrintf("ExtremeValues_%zu", i),
                                    HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(cases[i].sample);

    std::unique_ptr<HistogramSamples> snapshot = histogram->SnapshotSamples();
    std::unique_ptr<SampleCountIterator> it = snapshot->Iterator();
    ASSERT_FALSE(it->Done());

    base::Histogram::Sample min;
    int64_t max;
    base::Histogram::Count count;
    it->Get(&min, &max, &count);

    EXPECT_EQ(1, count);
    EXPECT_EQ(cases[i].sample, min);
    EXPECT_EQ(cases[i].expected_max, max);

    it->Next();
    EXPECT_TRUE(it->Done());
  }
}

TEST_P(SparseHistogramTest, HistogramNameHash) {
  const char kName[] = "TestName";
  HistogramBase* histogram = SparseHistogram::FactoryGet(
      kName, HistogramBase::kUmaTargetedHistogramFlag);
  EXPECT_EQ(histogram->name_hash(), HashMetricName(kName));
}

TEST_P(SparseHistogramTest, CheckGetCountAndBucketData) {
  std::unique_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  // Add samples in reverse order and make sure the output is in correct order.
  histogram->AddCount(/*sample=*/200, /*count=*/15);
  histogram->AddCount(/*sample=*/100, /*count=*/5);
  // Add samples to the same bucket and make sure they'll be aggregated.
  histogram->AddCount(/*sample=*/100, /*count=*/5);

  const CountAndBucketData count_and_data_bucket =
      GetCountAndBucketData(histogram.get());
  EXPECT_EQ(25, count_and_data_bucket.count);
  EXPECT_EQ(4000, count_and_data_bucket.sum);

  const base::Value::ConstListView buckets_list =
      count_and_data_bucket.buckets.GetList();
  ASSERT_EQ(2u, buckets_list.size());

  // Check the first bucket.
  const base::Value& bucket1 = buckets_list[0];
  ASSERT_TRUE(bucket1.is_dict());
  EXPECT_EQ(bucket1.FindIntKey("low"), absl::optional<int>(100));
  EXPECT_EQ(bucket1.FindIntKey("high"), absl::optional<int>(101));
  EXPECT_EQ(bucket1.FindIntKey("count"), absl::optional<int>(10));

  // Check the second bucket.
  const base::Value& bucket2 = buckets_list[1];
  ASSERT_TRUE(bucket2.is_dict());
  EXPECT_EQ(bucket2.FindIntKey("low"), absl::optional<int>(200));
  EXPECT_EQ(bucket2.FindIntKey("high"), absl::optional<int>(201));
  EXPECT_EQ(bucket2.FindIntKey("count"), absl::optional<int>(15));
}

TEST_P(SparseHistogramTest, WriteAscii) {
  HistogramBase* histogram =
      SparseHistogram::FactoryGet("AsciiOut", HistogramBase::kNoFlags);
  histogram->AddCount(/*sample=*/4, /*count=*/5);
  histogram->AddCount(/*sample=*/10, /*count=*/15);

  std::string output;
  histogram->WriteAscii(&output);

  const char kOutputFormatRe[] =
      R"(Histogram: AsciiOut recorded 20 samples.*\n)"
      R"(4   -+O +\(5 = 25.0%\)\n)"
      R"(10  -+O +\(15 = 75.0%\)\n)";

  EXPECT_THAT(output, testing::MatchesRegex(kOutputFormatRe));
}

TEST_P(SparseHistogramTest, ToGraphDict) {
  HistogramBase* histogram =
      SparseHistogram::FactoryGet("HTMLOut", HistogramBase::kNoFlags);
  histogram->AddCount(/*sample=*/4, /*count=*/5);
  histogram->AddCount(/*sample=*/10, /*count=*/15);

  base::Value output = histogram->ToGraphDict();
  std::string* header = output.FindStringKey("header");
  std::string* body = output.FindStringKey("body");

  const char kOutputHeaderFormatRe[] =
      R"(Histogram: HTMLOut recorded 20 samples.*)";
  const char kOutputBodyFormatRe[] = R"(4   -+O +\(5 = 25.0%\)\n)"
                                     R"(10  -+O +\(15 = 75.0%\)\n)";

  EXPECT_THAT(*header, testing::MatchesRegex(kOutputHeaderFormatRe));
  EXPECT_THAT(*body, testing::MatchesRegex(kOutputBodyFormatRe));
}

}  // namespace base
