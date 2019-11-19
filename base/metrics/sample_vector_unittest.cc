// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sample_vector.h"

#include <limits.h>
#include <stddef.h>

#include <atomic>
#include <memory>
#include <vector>

#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// This framework class has "friend" access to the SampleVector for accessing
// non-public methods and fields.
class SampleVectorTest : public testing::Test {
 public:
  const HistogramBase::AtomicCount* GetSamplesCounts(
      const SampleVectorBase& samples) {
    return samples.counts();
  }
};

TEST_F(SampleVectorTest, Accumulate) {
  // Custom buckets: [1, 5) [5, 10)
  BucketRanges ranges(3);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 10);
  SampleVector samples(1, &ranges);

  samples.Accumulate(1, 200);
  samples.Accumulate(2, -300);
  EXPECT_EQ(-100, samples.GetCountAtIndex(0));

  samples.Accumulate(5, 200);
  EXPECT_EQ(200, samples.GetCountAtIndex(1));

  EXPECT_EQ(600, samples.sum());
  EXPECT_EQ(100, samples.redundant_count());
  EXPECT_EQ(samples.TotalCount(), samples.redundant_count());

  samples.Accumulate(5, -100);
  EXPECT_EQ(100, samples.GetCountAtIndex(1));

  EXPECT_EQ(100, samples.sum());
  EXPECT_EQ(0, samples.redundant_count());
  EXPECT_EQ(samples.TotalCount(), samples.redundant_count());
}

TEST_F(SampleVectorTest, Accumulate_LargeValuesDontOverflow) {
  // Custom buckets: [1, 250000000) [250000000, 500000000)
  BucketRanges ranges(3);
  ranges.set_range(0, 1);
  ranges.set_range(1, 250000000);
  ranges.set_range(2, 500000000);
  SampleVector samples(1, &ranges);

  samples.Accumulate(240000000, 200);
  samples.Accumulate(249999999, -300);
  EXPECT_EQ(-100, samples.GetCountAtIndex(0));

  samples.Accumulate(250000000, 200);
  EXPECT_EQ(200, samples.GetCountAtIndex(1));

  EXPECT_EQ(23000000300LL, samples.sum());
  EXPECT_EQ(100, samples.redundant_count());
  EXPECT_EQ(samples.TotalCount(), samples.redundant_count());

  samples.Accumulate(250000000, -100);
  EXPECT_EQ(100, samples.GetCountAtIndex(1));

  EXPECT_EQ(-1999999700LL, samples.sum());
  EXPECT_EQ(0, samples.redundant_count());
  EXPECT_EQ(samples.TotalCount(), samples.redundant_count());
}

TEST_F(SampleVectorTest, AddSubtract) {
  // Custom buckets: [0, 1) [1, 2) [2, 3) [3, INT_MAX)
  BucketRanges ranges(5);
  ranges.set_range(0, 0);
  ranges.set_range(1, 1);
  ranges.set_range(2, 2);
  ranges.set_range(3, 3);
  ranges.set_range(4, INT_MAX);

  SampleVector samples1(1, &ranges);
  samples1.Accumulate(0, 100);
  samples1.Accumulate(2, 100);
  samples1.Accumulate(4, 100);
  EXPECT_EQ(600, samples1.sum());
  EXPECT_EQ(300, samples1.TotalCount());
  EXPECT_EQ(samples1.redundant_count(), samples1.TotalCount());

  SampleVector samples2(2, &ranges);
  samples2.Accumulate(1, 200);
  samples2.Accumulate(2, 200);
  samples2.Accumulate(4, 200);
  EXPECT_EQ(1400, samples2.sum());
  EXPECT_EQ(600, samples2.TotalCount());
  EXPECT_EQ(samples2.redundant_count(), samples2.TotalCount());

  samples1.Add(samples2);
  EXPECT_EQ(100, samples1.GetCountAtIndex(0));
  EXPECT_EQ(200, samples1.GetCountAtIndex(1));
  EXPECT_EQ(300, samples1.GetCountAtIndex(2));
  EXPECT_EQ(300, samples1.GetCountAtIndex(3));
  EXPECT_EQ(2000, samples1.sum());
  EXPECT_EQ(900, samples1.TotalCount());
  EXPECT_EQ(samples1.redundant_count(), samples1.TotalCount());

  samples1.Subtract(samples2);
  EXPECT_EQ(100, samples1.GetCountAtIndex(0));
  EXPECT_EQ(0, samples1.GetCountAtIndex(1));
  EXPECT_EQ(100, samples1.GetCountAtIndex(2));
  EXPECT_EQ(100, samples1.GetCountAtIndex(3));
  EXPECT_EQ(600, samples1.sum());
  EXPECT_EQ(300, samples1.TotalCount());
  EXPECT_EQ(samples1.redundant_count(), samples1.TotalCount());
}

TEST_F(SampleVectorTest, BucketIndexDeath) {
  // 8 buckets with exponential layout:
  // [0, 1) [1, 2) [2, 4) [4, 8) [8, 16) [16, 32) [32, 64) [64, INT_MAX)
  BucketRanges ranges(9);
  Histogram::InitializeBucketRanges(1, 64, &ranges);
  SampleVector samples(1, &ranges);

  // Normal case
  samples.Accumulate(0, 1);
  samples.Accumulate(3, 2);
  samples.Accumulate(64, 3);
  EXPECT_EQ(1, samples.GetCount(0));
  EXPECT_EQ(2, samples.GetCount(2));
  EXPECT_EQ(3, samples.GetCount(65));

  // Extreme case.
  EXPECT_DEATH_IF_SUPPORTED(samples.Accumulate(INT_MIN, 100), "");
  EXPECT_DEATH_IF_SUPPORTED(samples.Accumulate(-1, 100), "");
  EXPECT_DEATH_IF_SUPPORTED(samples.Accumulate(INT_MAX, 100), "");

  // Custom buckets: [1, 5) [5, 10)
  // Note, this is not a valid BucketRanges for Histogram because it does not
  // have overflow buckets.
  BucketRanges ranges2(3);
  ranges2.set_range(0, 1);
  ranges2.set_range(1, 5);
  ranges2.set_range(2, 10);
  SampleVector samples2(2, &ranges2);

  // Normal case.
  samples2.Accumulate(1, 1);
  samples2.Accumulate(4, 1);
  samples2.Accumulate(5, 2);
  samples2.Accumulate(9, 2);
  EXPECT_EQ(2, samples2.GetCount(1));
  EXPECT_EQ(4, samples2.GetCount(5));

  // Extreme case.
  EXPECT_DEATH_IF_SUPPORTED(samples2.Accumulate(0, 100), "");
  EXPECT_DEATH_IF_SUPPORTED(samples2.Accumulate(10, 100), "");
}

TEST_F(SampleVectorTest, AddSubtractBucketNotMatchDeath) {
  // Custom buckets 1: [1, 3) [3, 5)
  BucketRanges ranges1(3);
  ranges1.set_range(0, 1);
  ranges1.set_range(1, 3);
  ranges1.set_range(2, 5);
  SampleVector samples1(1, &ranges1);

  // Custom buckets 2: [0, 1) [1, 3) [3, 6) [6, 7)
  BucketRanges ranges2(5);
  ranges2.set_range(0, 0);
  ranges2.set_range(1, 1);
  ranges2.set_range(2, 3);
  ranges2.set_range(3, 6);
  ranges2.set_range(4, 7);
  SampleVector samples2(2, &ranges2);

  samples2.Accumulate(1, 100);
  samples1.Add(samples2);
  EXPECT_EQ(100, samples1.GetCountAtIndex(0));

  // Extra bucket in the beginning. These should CHECK in GetBucketIndex.
  samples2.Accumulate(0, 100);
  EXPECT_DEATH_IF_SUPPORTED(samples1.Add(samples2), "");
  EXPECT_DEATH_IF_SUPPORTED(samples1.Subtract(samples2), "");

  // Extra bucket in the end. These should cause AddSubtractImpl to fail, and
  // Add to DCHECK as a result.
  samples2.Accumulate(0, -100);
  samples2.Accumulate(6, 100);
  EXPECT_DCHECK_DEATH(samples1.Add(samples2));
  EXPECT_DCHECK_DEATH(samples1.Subtract(samples2));

  // Bucket not match: [3, 5) VS [3, 6). These should cause AddSubtractImpl to
  // DCHECK.
  samples2.Accumulate(6, -100);
  samples2.Accumulate(3, 100);
  EXPECT_DCHECK_DEATH(samples1.Add(samples2));
  EXPECT_DCHECK_DEATH(samples1.Subtract(samples2));
}

TEST_F(SampleVectorTest, Iterate) {
  BucketRanges ranges(5);
  ranges.set_range(0, 0);
  ranges.set_range(1, 1);
  ranges.set_range(2, 2);
  ranges.set_range(3, 3);
  ranges.set_range(4, 4);

  std::vector<HistogramBase::Count> counts(3);
  counts[0] = 1;
  counts[1] = 0;  // Iterator will bypass this empty bucket.
  counts[2] = 2;

  // BucketRanges can have larger size than counts.
  SampleVectorIterator it(&counts, &ranges);
  size_t index;

  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  it.Get(&min, &max, &count);
  EXPECT_EQ(0, min);
  EXPECT_EQ(1, max);
  EXPECT_EQ(1, count);
  EXPECT_TRUE(it.GetBucketIndex(&index));
  EXPECT_EQ(0u, index);

  it.Next();
  it.Get(&min, &max, &count);
  EXPECT_EQ(2, min);
  EXPECT_EQ(3, max);
  EXPECT_EQ(2, count);
  EXPECT_TRUE(it.GetBucketIndex(&index));
  EXPECT_EQ(2u, index);

  it.Next();
  EXPECT_TRUE(it.Done());

  // Create iterator from SampleVector.
  SampleVector samples(1, &ranges);
  samples.Accumulate(0, 0);
  samples.Accumulate(1, 1);
  samples.Accumulate(2, 2);
  samples.Accumulate(3, 3);
  std::unique_ptr<SampleCountIterator> it2 = samples.Iterator();

  int i;
  for (i = 1; !it2->Done(); i++, it2->Next()) {
    it2->Get(&min, &max, &count);
    EXPECT_EQ(i, min);
    EXPECT_EQ(i + 1, max);
    EXPECT_EQ(i, count);

    EXPECT_TRUE(it2->GetBucketIndex(&index));
    EXPECT_EQ(static_cast<size_t>(i), index);
  }
  EXPECT_EQ(4, i);
}

TEST_F(SampleVectorTest, IterateDoneDeath) {
  BucketRanges ranges(5);
  ranges.set_range(0, 0);
  ranges.set_range(1, 1);
  ranges.set_range(2, 2);
  ranges.set_range(3, 3);
  ranges.set_range(4, INT_MAX);
  SampleVector samples(1, &ranges);

  std::unique_ptr<SampleCountIterator> it = samples.Iterator();

  EXPECT_TRUE(it->Done());

  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  EXPECT_DCHECK_DEATH(it->Get(&min, &max, &count));

  EXPECT_DCHECK_DEATH(it->Next());

  samples.Accumulate(2, 100);
  it = samples.Iterator();
  EXPECT_FALSE(it->Done());
}

TEST_F(SampleVectorTest, SingleSample) {
  // Custom buckets: [1, 5) [5, 10)
  BucketRanges ranges(3);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 10);
  SampleVector samples(&ranges);

  // Ensure that a single value accumulates correctly.
  EXPECT_FALSE(GetSamplesCounts(samples));
  samples.Accumulate(3, 200);
  EXPECT_EQ(200, samples.GetCount(3));
  EXPECT_FALSE(GetSamplesCounts(samples));
  samples.Accumulate(3, 400);
  EXPECT_EQ(600, samples.GetCount(3));
  EXPECT_FALSE(GetSamplesCounts(samples));
  EXPECT_EQ(3 * 600, samples.sum());
  EXPECT_EQ(600, samples.TotalCount());
  EXPECT_EQ(600, samples.redundant_count());

  // Ensure that the iterator returns only one value.
  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  std::unique_ptr<SampleCountIterator> it = samples.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(600, count);
  it->Next();
  EXPECT_TRUE(it->Done());

  // Ensure that it can be merged to another single-sample vector.
  SampleVector samples_copy(&ranges);
  samples_copy.Add(samples);
  EXPECT_FALSE(GetSamplesCounts(samples_copy));
  EXPECT_EQ(3 * 600, samples_copy.sum());
  EXPECT_EQ(600, samples_copy.TotalCount());
  EXPECT_EQ(600, samples_copy.redundant_count());

  // A different value should cause creation of the counts array.
  samples.Accumulate(8, 100);
  EXPECT_TRUE(GetSamplesCounts(samples));
  EXPECT_EQ(600, samples.GetCount(3));
  EXPECT_EQ(100, samples.GetCount(8));
  EXPECT_EQ(3 * 600 + 8 * 100, samples.sum());
  EXPECT_EQ(600 + 100, samples.TotalCount());
  EXPECT_EQ(600 + 100, samples.redundant_count());

  // The iterator should now return both values.
  it = samples.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(600, count);
  it->Next();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(5, min);
  EXPECT_EQ(10, max);
  EXPECT_EQ(100, count);
  it->Next();
  EXPECT_TRUE(it->Done());

  // Ensure that it can merged to a single-sample vector.
  samples_copy.Add(samples);
  EXPECT_TRUE(GetSamplesCounts(samples_copy));
  EXPECT_EQ(3 * 1200 + 8 * 100, samples_copy.sum());
  EXPECT_EQ(1200 + 100, samples_copy.TotalCount());
  EXPECT_EQ(1200 + 100, samples_copy.redundant_count());
}

TEST_F(SampleVectorTest, PersistentSampleVector) {
  LocalPersistentMemoryAllocator allocator(64 << 10, 0, "");
  std::atomic<PersistentMemoryAllocator::Reference> samples_ref;
  samples_ref.store(0, std::memory_order_relaxed);
  HistogramSamples::Metadata samples_meta;
  memset(&samples_meta, 0, sizeof(samples_meta));

  // Custom buckets: [1, 5) [5, 10)
  BucketRanges ranges(3);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 10);

  // Persistent allocation.
  const size_t counts_bytes =
      sizeof(HistogramBase::AtomicCount) * ranges.bucket_count();
  const DelayedPersistentAllocation allocation(&allocator, &samples_ref, 1,
                                               counts_bytes, false);

  PersistentSampleVector samples1(0, &ranges, &samples_meta, allocation);
  EXPECT_FALSE(GetSamplesCounts(samples1));
  samples1.Accumulate(3, 200);
  EXPECT_EQ(200, samples1.GetCount(3));
  EXPECT_FALSE(GetSamplesCounts(samples1));
  EXPECT_EQ(0, samples1.GetCount(8));
  EXPECT_FALSE(GetSamplesCounts(samples1));

  PersistentSampleVector samples2(0, &ranges, &samples_meta, allocation);
  EXPECT_EQ(200, samples2.GetCount(3));
  EXPECT_FALSE(GetSamplesCounts(samples2));

  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  std::unique_ptr<SampleCountIterator> it = samples2.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(200, count);
  it->Next();
  EXPECT_TRUE(it->Done());

  samples1.Accumulate(8, 100);
  EXPECT_TRUE(GetSamplesCounts(samples1));

  EXPECT_FALSE(GetSamplesCounts(samples2));
  EXPECT_EQ(200, samples2.GetCount(3));
  EXPECT_EQ(100, samples2.GetCount(8));
  EXPECT_TRUE(GetSamplesCounts(samples2));
  EXPECT_EQ(3 * 200 + 8 * 100, samples2.sum());
  EXPECT_EQ(300, samples2.TotalCount());
  EXPECT_EQ(300, samples2.redundant_count());

  it = samples2.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(200, count);
  it->Next();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(5, min);
  EXPECT_EQ(10, max);
  EXPECT_EQ(100, count);
  it->Next();
  EXPECT_TRUE(it->Done());

  PersistentSampleVector samples3(0, &ranges, &samples_meta, allocation);
  EXPECT_TRUE(GetSamplesCounts(samples2));
  EXPECT_EQ(200, samples3.GetCount(3));
  EXPECT_EQ(100, samples3.GetCount(8));
  EXPECT_EQ(3 * 200 + 8 * 100, samples3.sum());
  EXPECT_EQ(300, samples3.TotalCount());
  EXPECT_EQ(300, samples3.redundant_count());

  it = samples3.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(200, count);
  it->Next();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(5, min);
  EXPECT_EQ(10, max);
  EXPECT_EQ(100, count);
  it->Next();
  EXPECT_TRUE(it->Done());
}

TEST_F(SampleVectorTest, PersistentSampleVectorTestWithOutsideAlloc) {
  LocalPersistentMemoryAllocator allocator(64 << 10, 0, "");
  std::atomic<PersistentMemoryAllocator::Reference> samples_ref;
  samples_ref.store(0, std::memory_order_relaxed);
  HistogramSamples::Metadata samples_meta;
  memset(&samples_meta, 0, sizeof(samples_meta));

  // Custom buckets: [1, 5) [5, 10)
  BucketRanges ranges(3);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 10);

  // Persistent allocation.
  const size_t counts_bytes =
      sizeof(HistogramBase::AtomicCount) * ranges.bucket_count();
  const DelayedPersistentAllocation allocation(&allocator, &samples_ref, 1,
                                               counts_bytes, false);

  PersistentSampleVector samples1(0, &ranges, &samples_meta, allocation);
  EXPECT_FALSE(GetSamplesCounts(samples1));
  samples1.Accumulate(3, 200);
  EXPECT_EQ(200, samples1.GetCount(3));
  EXPECT_FALSE(GetSamplesCounts(samples1));

  // Because the delayed allocation can be shared with other objects (the
  // |offset| parameter allows concatinating multiple data blocks into the
  // same allocation), it's possible that the allocation gets realized from
  // the outside even though the data block being accessed is all zero.
  allocation.Get();
  EXPECT_EQ(200, samples1.GetCount(3));
  EXPECT_FALSE(GetSamplesCounts(samples1));

  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  std::unique_ptr<SampleCountIterator> it = samples1.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(200, count);
  it->Next();
  EXPECT_TRUE(it->Done());

  // A duplicate samples object should still see the single-sample entry even
  // when storage is available.
  PersistentSampleVector samples2(0, &ranges, &samples_meta, allocation);
  EXPECT_EQ(200, samples2.GetCount(3));

  // New accumulations, in both directions, of the existing value should work.
  samples1.Accumulate(3, 50);
  EXPECT_EQ(250, samples1.GetCount(3));
  EXPECT_EQ(250, samples2.GetCount(3));
  samples2.Accumulate(3, 50);
  EXPECT_EQ(300, samples1.GetCount(3));
  EXPECT_EQ(300, samples2.GetCount(3));

  it = samples1.Iterator();
  ASSERT_FALSE(it->Done());
  it->Get(&min, &max, &count);
  EXPECT_EQ(1, min);
  EXPECT_EQ(5, max);
  EXPECT_EQ(300, count);
  it->Next();
  EXPECT_TRUE(it->Done());

  samples1.Accumulate(8, 100);
  EXPECT_TRUE(GetSamplesCounts(samples1));
  EXPECT_EQ(300, samples1.GetCount(3));
  EXPECT_EQ(300, samples2.GetCount(3));
  EXPECT_EQ(100, samples1.GetCount(8));
  EXPECT_EQ(100, samples2.GetCount(8));
  samples2.Accumulate(8, 100);
  EXPECT_EQ(300, samples1.GetCount(3));
  EXPECT_EQ(300, samples2.GetCount(3));
  EXPECT_EQ(200, samples1.GetCount(8));
  EXPECT_EQ(200, samples2.GetCount(8));
}

}  // namespace base
