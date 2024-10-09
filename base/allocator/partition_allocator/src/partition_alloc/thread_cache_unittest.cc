// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/thread_cache.h"

#include <algorithm>
#include <atomic>
#include <vector>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/extended_api.h"
#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/tagging.h"
#include "testing/gtest/include/gtest/gtest.h"

// With *SAN, PartitionAlloc is replaced in partition_alloc.h by ASAN, so we
// cannot test the thread cache.
//
// Finally, the thread cache is not supported on all platforms.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)

namespace partition_alloc {

using BucketDistribution = PartitionRoot::BucketDistribution;
using PartitionFreelistEncoding = internal::PartitionFreelistEncoding;

struct ThreadCacheTestParam {
  BucketDistribution bucket_distribution;
  PartitionFreelistEncoding freelist_encoding;
};

const std::vector<ThreadCacheTestParam> params = {
    {ThreadCacheTestParam{
        BucketDistribution::kNeutral,
        internal::PartitionFreelistEncoding::kPoolOffsetFreeList}},
    {ThreadCacheTestParam{
        BucketDistribution::kDenser,
        internal::PartitionFreelistEncoding::kEncodedFreeList}},
    {ThreadCacheTestParam{
        BucketDistribution::kNeutral,
        internal::PartitionFreelistEncoding::kPoolOffsetFreeList}},
    {ThreadCacheTestParam{
        BucketDistribution::kDenser,
        internal::PartitionFreelistEncoding::kEncodedFreeList}}};

namespace {

constexpr size_t kSmallSize = 33;  // Must be large enough to fit extras.
constexpr size_t kDefaultCountForSmallBucket =
    ThreadCache::kSmallBucketBaseCount * ThreadCache::kDefaultMultiplier;
constexpr size_t kFillCountForSmallBucket =
    kDefaultCountForSmallBucket / ThreadCache::kBatchFillRatio;

constexpr size_t kMediumSize = 200;
constexpr size_t kDefaultCountForMediumBucket = kDefaultCountForSmallBucket / 2;
constexpr size_t kFillCountForMediumBucket =
    kDefaultCountForMediumBucket / ThreadCache::kBatchFillRatio;

static_assert(kMediumSize <= ThreadCache::kDefaultSizeThreshold, "");

class DeltaCounter {
 public:
  explicit DeltaCounter(uint64_t& value)
      : current_value_(value), initial_value_(value) {}
  void Reset() { initial_value_ = current_value_; }
  uint64_t Delta() const { return current_value_ - initial_value_; }

 private:
  uint64_t& current_value_;
  uint64_t initial_value_;
};

// Forbid extras, since they make finding out which bucket is used harder.
std::unique_ptr<PartitionAllocatorForTesting> CreateAllocator(
    internal::PartitionFreelistEncoding encoding =
        internal::PartitionFreelistEncoding::kEncodedFreeList) {
  PartitionOptions opts;
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  opts.thread_cache = PartitionOptions::kEnabled;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  opts.star_scan_quarantine = PartitionOptions::kAllowed;
  opts.use_pool_offset_freelists =
      (encoding == internal::PartitionFreelistEncoding::kPoolOffsetFreeList)
          ? PartitionOptions::kEnabled
          : PartitionOptions::kDisabled;
  std::unique_ptr<PartitionAllocatorForTesting> allocator =
      std::make_unique<PartitionAllocatorForTesting>(opts);
  allocator->root()->UncapEmptySlotSpanMemoryForTesting();

  return allocator;
}
}  // namespace

class PartitionAllocThreadCacheTest
    : public ::testing::TestWithParam<ThreadCacheTestParam> {
 public:
  PartitionAllocThreadCacheTest()
      : allocator_(CreateAllocator(GetParam().freelist_encoding)),
        scope_(allocator_->root()) {}

  ~PartitionAllocThreadCacheTest() override {
    ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);

    // Cleanup the global state so next test can recreate ThreadCache.
    if (ThreadCache::IsTombstone(ThreadCache::Get())) {
      ThreadCache::RemoveTombstoneForTesting();
    }
  }
 protected:
  void SetUp() override {
    PartitionRoot* root = allocator_->root();
    switch (GetParam().bucket_distribution) {
      case BucketDistribution::kNeutral:
        root->ResetBucketDistributionForTesting();
        break;
      case BucketDistribution::kDenser:
        root->SwitchToDenserBucketDistribution();
        break;
    }

    ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
        ThreadCache::kDefaultMultiplier);
    ThreadCacheRegistry::Instance().SetPurgingConfiguration(
        kMinPurgeInterval, kMaxPurgeInterval, kDefaultPurgeInterval,
        kMinCachedMemoryForPurgingBytes);
    ThreadCache::SetLargestCachedSize(ThreadCache::kLargeSizeThreshold);

    // Make sure that enough slot spans have been touched, otherwise cache fill
    // becomes unpredictable (because it doesn't take slow paths in the
    // allocator), which is an issue for tests.
    FillThreadCacheAndReturnIndex(kSmallSize, 1000);
    FillThreadCacheAndReturnIndex(kMediumSize, 1000);

    // There are allocations, a thread cache is created.
    auto* tcache = root->thread_cache_for_testing();
    ASSERT_TRUE(tcache);

    ThreadCacheRegistry::Instance().ResetForTesting();
    tcache->ResetForTesting();
  }

  void TearDown() override {
    auto* tcache = root()->thread_cache_for_testing();
    ASSERT_TRUE(tcache);
    tcache->Purge();

    ASSERT_EQ(root()->get_total_size_of_allocated_bytes(), 0u);
  }

  PartitionRoot* root() { return allocator_->root(); }

  // Returns the size of the smallest bucket fitting an allocation of
  // |sizeof(ThreadCache)| bytes.
  size_t GetBucketSizeForThreadCache() {
    size_t tc_bucket_index = root()->SizeToBucketIndex(
        sizeof(ThreadCache), PartitionRoot::BucketDistribution::kNeutral);
    auto* tc_bucket = &root()->buckets[tc_bucket_index];
    return tc_bucket->slot_size;
  }

  static size_t SizeToIndex(size_t size) {
    return PartitionRoot::SizeToBucketIndex(size,
                                            GetParam().bucket_distribution);
  }

  size_t FillThreadCacheAndReturnIndex(size_t raw_size, size_t count = 1) {
    uint16_t bucket_index = SizeToIndex(raw_size);
    std::vector<void*> allocated_data;

    for (size_t i = 0; i < count; ++i) {
      allocated_data.push_back(
          root()->Alloc(root()->AdjustSizeForExtrasSubtract(raw_size), ""));
    }
    for (void* ptr : allocated_data) {
      root()->Free(ptr);
    }

    return bucket_index;
  }

  void FillThreadCacheWithMemory(size_t target_cached_memory) {
    for (int batch : {1, 2, 4, 8, 16}) {
      for (size_t raw_size = root()->AdjustSizeForExtrasAdd(1);
           raw_size <= ThreadCache::kLargeSizeThreshold; raw_size++) {
        FillThreadCacheAndReturnIndex(raw_size, batch);

        if (ThreadCache::Get()->CachedMemory() >= target_cached_memory) {
          return;
        }
      }
    }

    ASSERT_GE(ThreadCache::Get()->CachedMemory(), target_cached_memory);
  }

  std::unique_ptr<PartitionAllocatorForTesting> allocator_;
  internal::ThreadCacheProcessScopeForTesting scope_;
};

INSTANTIATE_TEST_SUITE_P(AlternateBucketDistributionAndPartitionFreeList,
                         PartitionAllocThreadCacheTest,
                         testing::ValuesIn(params));

TEST_P(PartitionAllocThreadCacheTest, Simple) {
  // There is a cache.
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_TRUE(tcache);
  DeltaCounter batch_fill_counter(tcache->stats_for_testing().batch_fill_count);

  void* ptr =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kSmallSize), "");
  ASSERT_TRUE(ptr);

  uint16_t index = SizeToIndex(kSmallSize);
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));

  root()->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket, tcache->bucket_count_for_testing(index));

  void* ptr2 =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kSmallSize), "");
  // MTE-untag, because Free() changes tag.
  EXPECT_EQ(UntagPtr(ptr), UntagPtr(ptr2));
  // Allocated from the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));

  EXPECT_EQ(1u, batch_fill_counter.Delta());

  root()->Free(ptr2);
}

TEST_P(PartitionAllocThreadCacheTest, InexactSizeMatch) {
  void* ptr =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kSmallSize), "");
  ASSERT_TRUE(ptr);

  // There is a cache.
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_TRUE(tcache);

  uint16_t index = SizeToIndex(kSmallSize);
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));

  root()->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket, tcache->bucket_count_for_testing(index));

  void* ptr2 =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kSmallSize + 1), "");
  // MTE-untag, because Free() changes tag.
  EXPECT_EQ(UntagPtr(ptr), UntagPtr(ptr2));
  // Allocated from the thread cache.
  EXPECT_EQ(kFillCountForSmallBucket - 1,
            tcache->bucket_count_for_testing(index));
  root()->Free(ptr2);
}

TEST_P(PartitionAllocThreadCacheTest, MultipleObjectsCachedPerBucket) {
  auto* tcache = root()->thread_cache_for_testing();
  DeltaCounter batch_fill_counter{tcache->stats_for_testing().batch_fill_count};
  size_t bucket_index =
      FillThreadCacheAndReturnIndex(kMediumSize, kFillCountForMediumBucket + 2);
  EXPECT_EQ(2 * kFillCountForMediumBucket,
            tcache->bucket_count_for_testing(bucket_index));
  // 2 batches, since there were more than |kFillCountForMediumBucket|
  // allocations.
  EXPECT_EQ(2u, batch_fill_counter.Delta());
}

TEST_P(PartitionAllocThreadCacheTest, ObjectsCachedCountIsLimited) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(kMediumSize, 1000);
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_LT(tcache->bucket_count_for_testing(bucket_index), 1000u);
}

TEST_P(PartitionAllocThreadCacheTest, Purge) {
  size_t allocations = 10;
  size_t bucket_index = FillThreadCacheAndReturnIndex(kMediumSize, allocations);
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_EQ(
      (1 + allocations / kFillCountForMediumBucket) * kFillCountForMediumBucket,
      tcache->bucket_count_for_testing(bucket_index));
  tcache->Purge();
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(bucket_index));
}

TEST_P(PartitionAllocThreadCacheTest, NoCrossPartitionCache) {
  PartitionOptions opts;
  opts.star_scan_quarantine = PartitionOptions::kAllowed;
  PartitionAllocatorForTesting allocator(opts);

  size_t bucket_index = FillThreadCacheAndReturnIndex(kSmallSize);
  void* ptr = allocator.root()->Alloc(
      allocator.root()->AdjustSizeForExtrasSubtract(kSmallSize), "");
  ASSERT_TRUE(ptr);

  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_EQ(kFillCountForSmallBucket,
            tcache->bucket_count_for_testing(bucket_index));

  allocator.root()->Free(ptr);
  EXPECT_EQ(kFillCountForSmallBucket,
            tcache->bucket_count_for_testing(bucket_index));
}

// Required to record hits and misses.
#if PA_CONFIG(THREAD_CACHE_ENABLE_STATISTICS)
TEST_P(PartitionAllocThreadCacheTest, LargeAllocationsAreNotCached) {
  auto* tcache = root()->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_for_testing().alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_for_testing().alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_for_testing().cache_fill_count};
  DeltaCounter cache_fill_misses_counter{
      tcache->stats_for_testing().cache_fill_misses};

  FillThreadCacheAndReturnIndex(100 * 1024);
  tcache = root()->thread_cache_for_testing();
  EXPECT_EQ(1u, alloc_miss_counter.Delta());
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());
  EXPECT_EQ(1u, cache_fill_counter.Delta());
  EXPECT_EQ(1u, cache_fill_misses_counter.Delta());
}
#endif  // PA_CONFIG(THREAD_CACHE_ENABLE_STATISTICS)

TEST_P(PartitionAllocThreadCacheTest, DirectMappedAllocationsAreNotCached) {
  FillThreadCacheAndReturnIndex(1024 * 1024);
  // The line above would crash due to out of bounds access if this wasn't
  // properly handled.
}

// This tests that Realloc properly handles bookkeeping, specifically the path
// that reallocates in place.
TEST_P(PartitionAllocThreadCacheTest, DirectMappedReallocMetrics) {
  root()->ResetBookkeepingForTesting();

  size_t expected_allocated_size = root()->get_total_size_of_allocated_bytes();

  EXPECT_EQ(expected_allocated_size,
            root()->get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_allocated_size, root()->get_max_size_of_allocated_bytes());

  void* ptr = root()->Alloc(
      root()->AdjustSizeForExtrasSubtract(10 * internal::kMaxBucketed), "");

  EXPECT_EQ(expected_allocated_size + 10 * internal::kMaxBucketed,
            root()->get_total_size_of_allocated_bytes());

  void* ptr2 = root()->Realloc(
      ptr, root()->AdjustSizeForExtrasSubtract(9 * internal::kMaxBucketed), "");

  ASSERT_EQ(ptr, ptr2);
  EXPECT_EQ(expected_allocated_size + 9 * internal::kMaxBucketed,
            root()->get_total_size_of_allocated_bytes());

  ptr2 = root()->Realloc(
      ptr, root()->AdjustSizeForExtrasSubtract(10 * internal::kMaxBucketed),
      "");

  ASSERT_EQ(ptr, ptr2);
  EXPECT_EQ(expected_allocated_size + 10 * internal::kMaxBucketed,
            root()->get_total_size_of_allocated_bytes());

  root()->Free(ptr);
}

namespace {

size_t FillThreadCacheAndReturnIndex(PartitionRoot* root,
                                     size_t size,
                                     BucketDistribution bucket_distribution,
                                     size_t count = 1) {
  uint16_t bucket_index =
      PartitionRoot::SizeToBucketIndex(size, bucket_distribution);
  std::vector<void*> allocated_data;

  for (size_t i = 0; i < count; ++i) {
    allocated_data.push_back(
        root->Alloc(root->AdjustSizeForExtrasSubtract(size), ""));
  }
  for (void* ptr : allocated_data) {
    root->Free(ptr);
  }

  return bucket_index;
}

// TODO(crbug.com/40158212): To remove callback from PartitionAlloc's DEPS,
// rewrite the tests without BindLambdaForTesting and RepeatingClosure.
// However this makes a little annoying to add more tests using their
// own threads. Need to support an easier way to implement tests using
// PlatformThreadForTesting::Create().
class ThreadDelegateForMultipleThreadCaches
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForMultipleThreadCaches(ThreadCache* parent_thread_cache,
                                        PartitionRoot* root,
                                        BucketDistribution bucket_distribution)
      : parent_thread_tcache_(parent_thread_cache),
        root_(root),
        bucket_distribution_(bucket_distribution) {}

  void ThreadMain() override {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(root_, kMediumSize, bucket_distribution_);
    auto* tcache = root_->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    EXPECT_NE(parent_thread_tcache_, tcache);
  }

 private:
  ThreadCache* parent_thread_tcache_ = nullptr;
  PartitionRoot* root_ = nullptr;
  PartitionRoot::BucketDistribution bucket_distribution_;
};

}  // namespace

TEST_P(PartitionAllocThreadCacheTest, MultipleThreadCaches) {
  FillThreadCacheAndReturnIndex(kMediumSize);
  auto* parent_thread_tcache = root()->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

  ThreadDelegateForMultipleThreadCaches delegate(
      parent_thread_tcache, root(), GetParam().bucket_distribution);

  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);
  internal::base::PlatformThreadForTesting::Join(thread_handle);
}

namespace {

class ThreadDelegateForThreadCacheReclaimedWhenThreadExits
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForThreadCacheReclaimedWhenThreadExits(PartitionRoot* root,
                                                       void*& other_thread_ptr)
      : root_(root), other_thread_ptr_(other_thread_ptr) {}

  void ThreadMain() override {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    other_thread_ptr_ =
        root_->Alloc(root_->AdjustSizeForExtrasSubtract(kMediumSize), "");
    root_->Free(other_thread_ptr_);
    // |other_thread_ptr| is now in the thread cache.
  }

 private:
  PartitionRoot* root_ = nullptr;
  void*& other_thread_ptr_;
};

}  // namespace

TEST_P(PartitionAllocThreadCacheTest, ThreadCacheReclaimedWhenThreadExits) {
  // Make sure that there is always at least one object allocated in the test
  // bucket, so that the PartitionPage is no reclaimed.
  //
  // Allocate enough objects to force a cache fill at the next allocation.
  std::vector<void*> tmp;
  for (size_t i = 0; i < kDefaultCountForMediumBucket / 4; i++) {
    tmp.push_back(
        root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), ""));
  }

  void* other_thread_ptr = nullptr;
  ThreadDelegateForThreadCacheReclaimedWhenThreadExits delegate(
      root(), other_thread_ptr);

  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);
  internal::base::PlatformThreadForTesting::Join(thread_handle);

  void* this_thread_ptr =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), "");
  // |other_thread_ptr| was returned to the central allocator, and is returned
  // here, as it comes from the freelist.
  EXPECT_EQ(UntagPtr(this_thread_ptr), UntagPtr(other_thread_ptr));
  root()->Free(other_thread_ptr);

  for (void* ptr : tmp) {
    root()->Free(ptr);
  }
}

namespace {

class ThreadDelegateForThreadCacheRegistry
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForThreadCacheRegistry(ThreadCache* parent_thread_cache,
                                       PartitionRoot* root,
                                       BucketDistribution bucket_distribution)
      : parent_thread_tcache_(parent_thread_cache),
        root_(root),
        bucket_distribution_(bucket_distribution) {}

  void ThreadMain() override {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(root_, kSmallSize, bucket_distribution_);
    auto* tcache = root_->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    internal::ScopedGuard lock(ThreadCacheRegistry::GetLock());
    EXPECT_EQ(tcache->prev_for_testing(), nullptr);
    EXPECT_EQ(tcache->next_for_testing(), parent_thread_tcache_);
  }

 private:
  ThreadCache* parent_thread_tcache_ = nullptr;
  PartitionRoot* root_ = nullptr;
  BucketDistribution bucket_distribution_;
};

}  // namespace

TEST_P(PartitionAllocThreadCacheTest, ThreadCacheRegistry) {
  auto* parent_thread_tcache = root()->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

#if !(PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(IS_ANDROID) ||   \
      PA_BUILDFLAG(IS_CHROMEOS) || PA_BUILDFLAG(IS_LINUX)) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // iOS and MacOS 15 create worker threads internally(start_wqthread).
  // So thread caches are created for the worker threads, because the threads
  // allocate memory for initialization (_dispatch_calloc is invoked).
  // We cannot assume that there is only 1 thread cache here.

  // Regarding Linux, ChromeOS and Android, some other tests may create
  // non-joinable threads. E.g. FilePathWatcherTest will create
  // non-joinable thread at InotifyReader::StartThread(). The thread will
  // be still running after the tests are finished, and will break
  // an assumption that there exists only main thread here.
  {
    internal::ScopedGuard lock(ThreadCacheRegistry::GetLock());
    EXPECT_EQ(parent_thread_tcache->prev_for_testing(), nullptr);
    EXPECT_EQ(parent_thread_tcache->next_for_testing(), nullptr);
  }
#endif

  ThreadDelegateForThreadCacheRegistry delegate(parent_thread_tcache, root(),
                                                GetParam().bucket_distribution);

  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);
  internal::base::PlatformThreadForTesting::Join(thread_handle);

#if !(PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(IS_ANDROID) ||   \
      PA_BUILDFLAG(IS_CHROMEOS) || PA_BUILDFLAG(IS_LINUX)) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  internal::ScopedGuard lock(ThreadCacheRegistry::GetLock());
  EXPECT_EQ(parent_thread_tcache->prev_for_testing(), nullptr);
  EXPECT_EQ(parent_thread_tcache->next_for_testing(), nullptr);
#endif
}

#if PA_CONFIG(THREAD_CACHE_ENABLE_STATISTICS)
TEST_P(PartitionAllocThreadCacheTest, RecordStats) {
  auto* tcache = root()->thread_cache_for_testing();
  DeltaCounter alloc_counter{tcache->stats_for_testing().alloc_count};
  DeltaCounter alloc_hits_counter{tcache->stats_for_testing().alloc_hits};
  DeltaCounter alloc_miss_counter{tcache->stats_for_testing().alloc_misses};

  DeltaCounter alloc_miss_empty_counter{
      tcache->stats_for_testing().alloc_miss_empty};

  DeltaCounter cache_fill_counter{tcache->stats_for_testing().cache_fill_count};
  DeltaCounter cache_fill_hits_counter{
      tcache->stats_for_testing().cache_fill_hits};
  DeltaCounter cache_fill_misses_counter{
      tcache->stats_for_testing().cache_fill_misses};

  // Cache has been purged, first allocation is a miss.
  void* data =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), "");
  EXPECT_EQ(1u, alloc_counter.Delta());
  EXPECT_EQ(1u, alloc_miss_counter.Delta());
  EXPECT_EQ(0u, alloc_hits_counter.Delta());

  // Cache fill worked.
  root()->Free(data);
  EXPECT_EQ(1u, cache_fill_counter.Delta());
  EXPECT_EQ(1u, cache_fill_hits_counter.Delta());
  EXPECT_EQ(0u, cache_fill_misses_counter.Delta());

  tcache->Purge();
  cache_fill_counter.Reset();
  // Buckets are never full, fill always succeeds.
  size_t allocations = 10;
  size_t bucket_index = FillThreadCacheAndReturnIndex(
      kMediumSize, kDefaultCountForMediumBucket + allocations);
  EXPECT_EQ(kDefaultCountForMediumBucket + allocations,
            cache_fill_counter.Delta());
  EXPECT_EQ(0u, cache_fill_misses_counter.Delta());

  // Memory footprint.
  ThreadCacheStats stats;
  ThreadCacheRegistry::Instance().DumpStats(true, &stats);
  // Bucket was cleared (set to kDefaultCountForMediumBucket / 2) after going
  // above the limit (-1), then refilled by batches (1 + floor(allocations /
  // kFillCountForSmallBucket) times).
  size_t expected_count =
      kDefaultCountForMediumBucket / 2 - 1 +
      (1 + allocations / kFillCountForMediumBucket) * kFillCountForMediumBucket;
  EXPECT_EQ(root()->buckets[bucket_index].slot_size * expected_count,
            stats.bucket_total_memory);
  EXPECT_EQ(sizeof(ThreadCache), stats.metadata_overhead);
}

namespace {

class ThreadDelegateForMultipleThreadCachesAccounting
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForMultipleThreadCachesAccounting(
      PartitionRoot* root,
      const ThreadCacheStats& wqthread_stats,
      int alloc_count,
      BucketDistribution bucket_distribution)
      : root_(root),
        bucket_distribution_(bucket_distribution),
        wqthread_stats_(wqthread_stats),
        alloc_count_(alloc_count) {}

  void ThreadMain() override {
    EXPECT_FALSE(root_->thread_cache_for_testing());  // No allocations yet.
    size_t bucket_index =
        FillThreadCacheAndReturnIndex(root_, kMediumSize, bucket_distribution_);

    ThreadCacheStats stats;
    ThreadCacheRegistry::Instance().DumpStats(false, &stats);
    // 2* for this thread and the parent one.
    EXPECT_EQ(
        2 * root_->buckets[bucket_index].slot_size * kFillCountForMediumBucket,
        stats.bucket_total_memory - wqthread_stats_.bucket_total_memory);
    EXPECT_EQ(2 * sizeof(ThreadCache),
              stats.metadata_overhead - wqthread_stats_.metadata_overhead);

    ThreadCacheStats this_thread_cache_stats{};
    root_->thread_cache_for_testing()->AccumulateStats(
        &this_thread_cache_stats);
    EXPECT_EQ(alloc_count_ + this_thread_cache_stats.alloc_count,
              stats.alloc_count - wqthread_stats_.alloc_count);
  }

 private:
  PartitionRoot* root_ = nullptr;
  BucketDistribution bucket_distribution_;
  const ThreadCacheStats wqthread_stats_;
  const int alloc_count_;
};

}  // namespace

TEST_P(PartitionAllocThreadCacheTest, MultipleThreadCachesAccounting) {
  ThreadCacheStats wqthread_stats{0};
#if (PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(IS_ANDROID) ||   \
     PA_BUILDFLAG(IS_CHROMEOS) || PA_BUILDFLAG(IS_LINUX)) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    // iOS and MacOS 15 create worker threads internally(start_wqthread).
    // So thread caches are created for the worker threads, because the threads
    // allocate memory for initialization (_dispatch_calloc is invoked).
    // We need to count worker threads created by iOS and Mac system.

    // Regarding Linux, ChromeOS and Android, some other tests may create
    // non-joinable threads. E.g. FilePathWatcherTest will create
    // non-joinable thread at InotifyReader::StartThread(). The thread will
    // be still running after the tests are finished. We need to count
    // the joinable threads here.
    ThreadCacheRegistry::Instance().DumpStats(false, &wqthread_stats);

    // Remove this thread's thread cache stats from wqthread_stats.
    ThreadCacheStats this_stats;
    ThreadCacheRegistry::Instance().DumpStats(true, &this_stats);

    wqthread_stats.alloc_count -= this_stats.alloc_count;
    wqthread_stats.metadata_overhead -= this_stats.metadata_overhead;
    wqthread_stats.bucket_total_memory -= this_stats.bucket_total_memory;
  }
#endif
  FillThreadCacheAndReturnIndex(kMediumSize);
  uint64_t alloc_count =
      root()->thread_cache_for_testing()->stats_for_testing().alloc_count;

  ThreadDelegateForMultipleThreadCachesAccounting delegate(
      root(), wqthread_stats, alloc_count, GetParam().bucket_distribution);

  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);
  internal::base::PlatformThreadForTesting::Join(thread_handle);
}

#endif  // PA_CONFIG(THREAD_CACHE_ENABLE_STATISTICS)

// TODO(crbug.com/40816487): Flaky on IOS.
#if PA_BUILDFLAG(IS_IOS)
#define MAYBE_PurgeAll DISABLED_PurgeAll
#else
#define MAYBE_PurgeAll PurgeAll
#endif

namespace {

class ThreadDelegateForPurgeAll
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForPurgeAll(PartitionRoot* root,
                            ThreadCache*& other_thread_tcache,
                            std::atomic<bool>& other_thread_started,
                            std::atomic<bool>& purge_called,
                            int bucket_index,
                            BucketDistribution bucket_distribution)
      : root_(root),
        other_thread_tcache_(other_thread_tcache),
        other_thread_started_(other_thread_started),
        purge_called_(purge_called),
        bucket_index_(bucket_index),
        bucket_distribution_(bucket_distribution) {}

  void ThreadMain() override PA_NO_THREAD_SAFETY_ANALYSIS {
    FillThreadCacheAndReturnIndex(root_, kSmallSize, bucket_distribution_);
    other_thread_tcache_ = root_->thread_cache_for_testing();

    other_thread_started_.store(true, std::memory_order_release);
    while (!purge_called_.load(std::memory_order_acquire)) {
    }

    // Purge() was not triggered from the other thread.
    EXPECT_EQ(kFillCountForSmallBucket,
              other_thread_tcache_->bucket_count_for_testing(bucket_index_));
    // Allocations do not trigger Purge().
    void* data =
        root_->Alloc(root_->AdjustSizeForExtrasSubtract(kSmallSize), "");
    EXPECT_EQ(kFillCountForSmallBucket - 1,
              other_thread_tcache_->bucket_count_for_testing(bucket_index_));
    // But deallocations do.
    root_->Free(data);
    EXPECT_EQ(0u,
              other_thread_tcache_->bucket_count_for_testing(bucket_index_));
  }

 private:
  PartitionRoot* root_ = nullptr;
  ThreadCache*& other_thread_tcache_;
  std::atomic<bool>& other_thread_started_;
  std::atomic<bool>& purge_called_;
  const int bucket_index_;
  BucketDistribution bucket_distribution_;
};

}  // namespace

TEST_P(PartitionAllocThreadCacheTest, MAYBE_PurgeAll)
PA_NO_THREAD_SAFETY_ANALYSIS {
  std::atomic<bool> other_thread_started{false};
  std::atomic<bool> purge_called{false};

  size_t bucket_index = FillThreadCacheAndReturnIndex(kSmallSize);
  ThreadCache* this_thread_tcache = root()->thread_cache_for_testing();
  ThreadCache* other_thread_tcache = nullptr;

  ThreadDelegateForPurgeAll delegate(
      root(), other_thread_tcache, other_thread_started, purge_called,
      bucket_index, GetParam().bucket_distribution);
  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);

  while (!other_thread_started.load(std::memory_order_acquire)) {
  }

  EXPECT_EQ(kFillCountForSmallBucket,
            this_thread_tcache->bucket_count_for_testing(bucket_index));
  EXPECT_EQ(kFillCountForSmallBucket,
            other_thread_tcache->bucket_count_for_testing(bucket_index));

  ThreadCacheRegistry::Instance().PurgeAll();
  // This thread is synchronously purged.
  EXPECT_EQ(0u, this_thread_tcache->bucket_count_for_testing(bucket_index));
  // Not the other one.
  EXPECT_EQ(kFillCountForSmallBucket,
            other_thread_tcache->bucket_count_for_testing(bucket_index));

  purge_called.store(true, std::memory_order_release);
  internal::base::PlatformThreadForTesting::Join(thread_handle);
}

TEST_P(PartitionAllocThreadCacheTest, PeriodicPurge) {
  auto& registry = ThreadCacheRegistry::Instance();
  auto NextInterval = [&registry] {
    return internal::base::Microseconds(
        registry.GetPeriodicPurgeNextIntervalInMicroseconds());
  };

  EXPECT_EQ(NextInterval(), registry.default_purge_interval());

  // Small amount of memory, the period gets longer.
  auto* tcache = ThreadCache::Get();
  ASSERT_LT(tcache->CachedMemory(),
            registry.min_cached_memory_for_purging_bytes());
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), 2 * registry.default_purge_interval());
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), 4 * registry.default_purge_interval());

  // Check that the purge interval is clamped at the maximum value.
  while (NextInterval() < registry.max_purge_interval()) {
    registry.RunPeriodicPurge();
  }
  registry.RunPeriodicPurge();

  // Not enough memory to decrease the interval.
  FillThreadCacheWithMemory(registry.min_cached_memory_for_purging_bytes() + 1);
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), registry.max_purge_interval());

  FillThreadCacheWithMemory(2 * registry.min_cached_memory_for_purging_bytes() +
                            1);
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), registry.max_purge_interval() / 2);

  // Enough memory, interval doesn't change.
  FillThreadCacheWithMemory(registry.min_cached_memory_for_purging_bytes());
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), registry.max_purge_interval() / 2);

  // No cached memory, increase the interval.
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), registry.max_purge_interval());

  // Cannot test the very large size with only one thread, this is tested below
  // in the multiple threads test.
}

namespace {

void FillThreadCacheWithMemory(PartitionRoot* root,
                               size_t target_cached_memory,
                               BucketDistribution bucket_distribution) {
  for (int batch : {1, 2, 4, 8, 16}) {
    for (size_t allocation_size = 1;
         allocation_size <= ThreadCache::kLargeSizeThreshold;
         allocation_size++) {
      FillThreadCacheAndReturnIndex(
          root, root->AdjustSizeForExtrasAdd(allocation_size),
          bucket_distribution, batch);

      if (ThreadCache::Get()->CachedMemory() >= target_cached_memory) {
        return;
      }
    }
  }

  ASSERT_GE(ThreadCache::Get()->CachedMemory(), target_cached_memory);
}

class ThreadDelegateForPeriodicPurgeSumsOverAllThreads
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForPeriodicPurgeSumsOverAllThreads(
      PartitionRoot* root,
      std::atomic<int>& allocations_done,
      std::atomic<bool>& can_finish,
      BucketDistribution bucket_distribution)
      : root_(root),
        allocations_done_(allocations_done),
        can_finish_(can_finish),
        bucket_distribution_(bucket_distribution) {}

  void ThreadMain() override {
    FillThreadCacheWithMemory(root_,
                              5 * ThreadCacheRegistry::Instance()
                                      .min_cached_memory_for_purging_bytes(),
                              bucket_distribution_);
    allocations_done_.fetch_add(1, std::memory_order_release);

    // This thread needs to be alive when the next periodic purge task runs.
    while (!can_finish_.load(std::memory_order_acquire)) {
    }
  }

 private:
  PartitionRoot* root_ = nullptr;
  std::atomic<int>& allocations_done_;
  std::atomic<bool>& can_finish_;
  BucketDistribution bucket_distribution_;
};

}  // namespace

// Disabled due to flakiness: crbug.com/1220371
TEST_P(PartitionAllocThreadCacheTest,
       DISABLED_PeriodicPurgeSumsOverAllThreads) {
  auto& registry = ThreadCacheRegistry::Instance();
  auto NextInterval = [&registry] {
    return internal::base::Microseconds(
        registry.GetPeriodicPurgeNextIntervalInMicroseconds());
  };
  EXPECT_EQ(NextInterval(), registry.default_purge_interval());

  // Small amount of memory, the period gets longer.
  auto* tcache = ThreadCache::Get();
  ASSERT_LT(tcache->CachedMemory(),
            registry.min_cached_memory_for_purging_bytes());
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), 2 * registry.default_purge_interval());
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), 4 * registry.default_purge_interval());

  // Check that the purge interval is clamped at the maximum value.
  while (NextInterval() < registry.max_purge_interval()) {
    registry.RunPeriodicPurge();
  }
  registry.RunPeriodicPurge();

  // Not enough memory on this thread to decrease the interval.
  FillThreadCacheWithMemory(registry.min_cached_memory_for_purging_bytes() / 2);
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), registry.max_purge_interval());

  std::atomic<int> allocations_done{0};
  std::atomic<bool> can_finish{false};
  ThreadDelegateForPeriodicPurgeSumsOverAllThreads delegate(
      root(), allocations_done, can_finish, GetParam().bucket_distribution);

  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);
  internal::base::PlatformThreadHandle thread_handle_2;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle_2);

  while (allocations_done.load(std::memory_order_acquire) != 2) {
    internal::base::PlatformThreadForTesting::YieldCurrentThread();
  }

  // Many allocations on the other thread.
  registry.RunPeriodicPurge();
  EXPECT_EQ(NextInterval(), registry.default_purge_interval());

  can_finish.store(true, std::memory_order_release);
  internal::base::PlatformThreadForTesting::Join(thread_handle);
  internal::base::PlatformThreadForTesting::Join(thread_handle_2);
}

// TODO(crbug.com/40816487): Flaky on IOS.
#if PA_BUILDFLAG(IS_IOS)
#define MAYBE_DynamicCountPerBucket DISABLED_DynamicCountPerBucket
#else
#define MAYBE_DynamicCountPerBucket DynamicCountPerBucket
#endif
TEST_P(PartitionAllocThreadCacheTest, MAYBE_DynamicCountPerBucket) {
  auto* tcache = root()->thread_cache_for_testing();
  size_t bucket_index =
      FillThreadCacheAndReturnIndex(kMediumSize, kDefaultCountForMediumBucket);

  EXPECT_EQ(kDefaultCountForMediumBucket,
            tcache->bucket_for_testing(bucket_index).count);

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier / 2);
  // No immediate batch deallocation.
  EXPECT_EQ(kDefaultCountForMediumBucket,
            tcache->bucket_for_testing(bucket_index).count);
  void* data =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), "");
  // Not triggered by allocations.
  EXPECT_EQ(kDefaultCountForMediumBucket - 1,
            tcache->bucket_for_testing(bucket_index).count);

  // Free() triggers the purge within limits.
  root()->Free(data);
  EXPECT_LE(tcache->bucket_for_testing(bucket_index).count,
            kDefaultCountForMediumBucket / 2);

  // Won't go above anymore.
  FillThreadCacheAndReturnIndex(kMediumSize, 1000);
  EXPECT_LE(tcache->bucket_for_testing(bucket_index).count,
            kDefaultCountForMediumBucket / 2);

  // Limit can be raised.
  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier * 2);
  FillThreadCacheAndReturnIndex(kMediumSize, 1000);
  EXPECT_GT(tcache->bucket_for_testing(bucket_index).count,
            kDefaultCountForMediumBucket / 2);
}

TEST_P(PartitionAllocThreadCacheTest, DynamicCountPerBucketClamping) {
  auto* tcache = root()->thread_cache_for_testing();

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier / 1000.);
  for (size_t i = 0; i < ThreadCache::kBucketCount; i++) {
    // Invalid bucket.
    if (!tcache->bucket_for_testing(i).limit.load(std::memory_order_relaxed)) {
      EXPECT_EQ(root()->buckets[i].active_slot_spans_head, nullptr);
      continue;
    }
    EXPECT_GE(
        tcache->bucket_for_testing(i).limit.load(std::memory_order_relaxed),
        1u);
  }

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier * 1000.);
  for (size_t i = 0; i < ThreadCache::kBucketCount; i++) {
    // Invalid bucket.
    if (!tcache->bucket_for_testing(i).limit.load(std::memory_order_relaxed)) {
      EXPECT_EQ(root()->buckets[i].active_slot_spans_head, nullptr);
      continue;
    }
    EXPECT_LT(
        tcache->bucket_for_testing(i).limit.load(std::memory_order_relaxed),
        0xff);
  }
}

// TODO(crbug.com/40816487): Flaky on IOS.
#if PA_BUILDFLAG(IS_IOS)
#define MAYBE_DynamicCountPerBucketMultipleThreads \
  DISABLED_DynamicCountPerBucketMultipleThreads
#else
#define MAYBE_DynamicCountPerBucketMultipleThreads \
  DynamicCountPerBucketMultipleThreads
#endif

namespace {

class ThreadDelegateForDynamicCountPerBucketMultipleThreads
    : public internal::base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForDynamicCountPerBucketMultipleThreads(
      PartitionRoot* root,
      std::atomic<bool>& other_thread_started,
      std::atomic<bool>& threshold_changed,
      int bucket_index,
      BucketDistribution bucket_distribution)
      : root_(root),
        other_thread_started_(other_thread_started),
        threshold_changed_(threshold_changed),
        bucket_index_(bucket_index),
        bucket_distribution_(bucket_distribution) {}

  void ThreadMain() override {
    FillThreadCacheAndReturnIndex(root_, kSmallSize, bucket_distribution_,
                                  kDefaultCountForSmallBucket + 10);
    auto* this_thread_tcache = root_->thread_cache_for_testing();
    // More than the default since the multiplier has changed.
    EXPECT_GT(this_thread_tcache->bucket_count_for_testing(bucket_index_),
              kDefaultCountForSmallBucket + 10);

    other_thread_started_.store(true, std::memory_order_release);
    while (!threshold_changed_.load(std::memory_order_acquire)) {
    }

    void* data =
        root_->Alloc(root_->AdjustSizeForExtrasSubtract(kSmallSize), "");
    // Deallocations trigger limit enforcement.
    root_->Free(data);
    // Since the bucket is too full, it gets halved by batched deallocation.
    EXPECT_EQ(static_cast<uint8_t>(ThreadCache::kSmallBucketBaseCount / 2),
              this_thread_tcache->bucket_count_for_testing(bucket_index_));
  }

 private:
  PartitionRoot* root_ = nullptr;
  std::atomic<bool>& other_thread_started_;
  std::atomic<bool>& threshold_changed_;
  const int bucket_index_;
  PartitionRoot::BucketDistribution bucket_distribution_;
};

}  // namespace

TEST_P(PartitionAllocThreadCacheTest,
       MAYBE_DynamicCountPerBucketMultipleThreads) {
  std::atomic<bool> other_thread_started{false};
  std::atomic<bool> threshold_changed{false};

  auto* tcache = root()->thread_cache_for_testing();
  size_t bucket_index =
      FillThreadCacheAndReturnIndex(kSmallSize, kDefaultCountForSmallBucket);
  EXPECT_EQ(kDefaultCountForSmallBucket,
            tcache->bucket_for_testing(bucket_index).count);

  // Change the ratio before starting the threads, checking that it will applied
  // to newly-created threads.
  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
      ThreadCache::kDefaultMultiplier + 1);

  ThreadDelegateForDynamicCountPerBucketMultipleThreads delegate(
      root(), other_thread_started, threshold_changed, bucket_index,
      GetParam().bucket_distribution);

  internal::base::PlatformThreadHandle thread_handle;
  internal::base::PlatformThreadForTesting::Create(0, &delegate,
                                                   &thread_handle);

  while (!other_thread_started.load(std::memory_order_acquire)) {
  }

  ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(1.);
  threshold_changed.store(true, std::memory_order_release);

  internal::base::PlatformThreadForTesting::Join(thread_handle);
}

TEST_P(PartitionAllocThreadCacheTest, DynamicSizeThreshold) {
  auto* tcache = root()->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_for_testing().alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_for_testing().alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_for_testing().cache_fill_count};
  DeltaCounter cache_fill_misses_counter{
      tcache->stats_for_testing().cache_fill_misses};

  // Default threshold at first.
  ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold);

  EXPECT_EQ(0u, alloc_miss_too_large_counter.Delta());
  EXPECT_EQ(1u, cache_fill_counter.Delta());

  // Too large to be cached.
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold + 1);
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());

  // Increase.
  ThreadCache::SetLargestCachedSize(ThreadCache::kLargeSizeThreshold);
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold + 1);
  // No new miss.
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());

  // Lower.
  ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
  FillThreadCacheAndReturnIndex(ThreadCache::kDefaultSizeThreshold + 1);
  EXPECT_EQ(2u, alloc_miss_too_large_counter.Delta());

  // Value is clamped.
  size_t too_large = 1024 * 1024;
  ThreadCache::SetLargestCachedSize(too_large);
  FillThreadCacheAndReturnIndex(too_large);
  EXPECT_EQ(3u, alloc_miss_too_large_counter.Delta());
}

// Disabled due to flakiness: crbug.com/1287811
TEST_P(PartitionAllocThreadCacheTest, DISABLED_DynamicSizeThresholdPurge) {
  auto* tcache = root()->thread_cache_for_testing();
  DeltaCounter alloc_miss_counter{tcache->stats_for_testing().alloc_misses};
  DeltaCounter alloc_miss_too_large_counter{
      tcache->stats_for_testing().alloc_miss_too_large};
  DeltaCounter cache_fill_counter{tcache->stats_for_testing().cache_fill_count};
  DeltaCounter cache_fill_misses_counter{
      tcache->stats_for_testing().cache_fill_misses};

  // Cache large allocations.
  size_t large_allocation_size = ThreadCache::kLargeSizeThreshold;
  ThreadCache::SetLargestCachedSize(ThreadCache::kLargeSizeThreshold);
  size_t index = FillThreadCacheAndReturnIndex(large_allocation_size);
  EXPECT_EQ(0u, alloc_miss_too_large_counter.Delta());

  // Lower.
  ThreadCache::SetLargestCachedSize(ThreadCache::kDefaultSizeThreshold);
  FillThreadCacheAndReturnIndex(large_allocation_size);
  EXPECT_EQ(1u, alloc_miss_too_large_counter.Delta());

  // There is memory trapped in the cache bucket.
  EXPECT_GT(tcache->bucket_for_testing(index).count, 0u);

  // Which is reclaimed by Purge().
  tcache->Purge();
  EXPECT_EQ(0u, tcache->bucket_for_testing(index).count);
}

TEST_P(PartitionAllocThreadCacheTest, ClearFromTail) {
  auto count_items = [this](ThreadCache* tcache, size_t index) {
    const internal::PartitionFreelistDispatcher* freelist_dispatcher =
        this->root()->get_freelist_dispatcher();
    uint8_t count = 0;
    auto* head = tcache->bucket_for_testing(index).freelist_head;
    while (head) {
#if PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
      head = freelist_dispatcher->GetNextForThreadCacheTrue(
          head, tcache->bucket_for_testing(index).slot_size);
#else
      head = freelist_dispatcher->GetNextForThreadCache<true>(
          head, tcache->bucket_for_testing(index).slot_size);
#endif  // PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
      count++;
    }
    return count;
  };

  auto* tcache = root()->thread_cache_for_testing();
  size_t index = FillThreadCacheAndReturnIndex(kSmallSize, 10);
  ASSERT_GE(count_items(tcache, index), 10);
  void* head = tcache->bucket_for_testing(index).freelist_head;

  for (size_t limit : {8, 3, 1}) {
    tcache->ClearBucketForTesting(tcache->bucket_for_testing(index), limit);
    EXPECT_EQ(head, static_cast<void*>(
                        tcache->bucket_for_testing(index).freelist_head));
    EXPECT_EQ(count_items(tcache, index), limit);
  }
  tcache->ClearBucketForTesting(tcache->bucket_for_testing(index), 0);
  EXPECT_EQ(nullptr, static_cast<void*>(
                         tcache->bucket_for_testing(index).freelist_head));
}

// TODO(crbug.com/40816487): Flaky on IOS.
#if PA_BUILDFLAG(IS_IOS)
#define MAYBE_Bookkeeping DISABLED_Bookkeeping
#else
#define MAYBE_Bookkeeping Bookkeeping
#endif
TEST_P(PartitionAllocThreadCacheTest, MAYBE_Bookkeeping) {
  void* arr[kFillCountForMediumBucket] = {};
  auto* tcache = root()->thread_cache_for_testing();

  root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                      PurgeFlags::kDiscardUnusedSystemPages);
  root()->ResetBookkeepingForTesting();

  size_t expected_allocated_size = 0;
  size_t expected_committed_size = 0;

  EXPECT_EQ(expected_committed_size, root()->total_size_of_committed_pages);
  EXPECT_EQ(expected_committed_size, root()->max_size_of_committed_pages);
  EXPECT_EQ(expected_allocated_size,
            root()->get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_allocated_size, root()->get_max_size_of_allocated_bytes());

  void* ptr =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), "");

  auto* medium_bucket = root()->buckets + SizeToIndex(kMediumSize);
  size_t medium_alloc_size = medium_bucket->slot_size;
  expected_allocated_size += medium_alloc_size;
  expected_committed_size += kUseLazyCommit
                                 ? internal::SystemPageSize()
                                 : medium_bucket->get_bytes_per_span();

  EXPECT_EQ(expected_committed_size, root()->total_size_of_committed_pages);
  EXPECT_EQ(expected_committed_size, root()->max_size_of_committed_pages);
  EXPECT_EQ(expected_allocated_size,
            root()->get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_allocated_size, root()->get_max_size_of_allocated_bytes());

  expected_allocated_size += kFillCountForMediumBucket * medium_alloc_size;

  // These allocations all come from the thread-cache.
  for (size_t i = 0; i < kFillCountForMediumBucket; i++) {
    arr[i] =
        root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), "");
    EXPECT_EQ(expected_committed_size, root()->total_size_of_committed_pages);
    EXPECT_EQ(expected_committed_size, root()->max_size_of_committed_pages);
    EXPECT_EQ(expected_allocated_size,
              root()->get_total_size_of_allocated_bytes());
    EXPECT_EQ(expected_allocated_size,
              root()->get_max_size_of_allocated_bytes());
    EXPECT_EQ((kFillCountForMediumBucket - 1 - i) * medium_alloc_size,
              tcache->CachedMemory());
  }

  EXPECT_EQ(0U, tcache->CachedMemory());

  root()->Free(ptr);

  for (auto*& el : arr) {
    root()->Free(el);
  }
  EXPECT_EQ(root()->get_total_size_of_allocated_bytes(),
            expected_allocated_size);
  tcache->Purge();
}

TEST_P(PartitionAllocThreadCacheTest, TryPurgeNoAllocs) {
  auto* tcache = root()->thread_cache_for_testing();
  tcache->TryPurge();
}

TEST_P(PartitionAllocThreadCacheTest, TryPurgeMultipleCorrupted) {
  auto* tcache = root()->thread_cache_for_testing();

  void* ptr =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kMediumSize), "");

  auto* medium_bucket = root()->buckets + SizeToIndex(kMediumSize);

  auto* curr = medium_bucket->active_slot_spans_head->get_freelist_head();
  const internal::PartitionFreelistDispatcher* freelist_dispatcher =
      root()->get_freelist_dispatcher();
#if PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
  curr = freelist_dispatcher->GetNextForThreadCacheTrue(curr, kMediumSize);
#else
  curr = freelist_dispatcher->GetNextForThreadCache<true>(curr, kMediumSize);
#endif  // PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
  freelist_dispatcher->CorruptNextForTesting(curr, 0x12345678);
  tcache->TryPurge();
  freelist_dispatcher->SetNext(curr, nullptr);
  root()->Free(ptr);
}

TEST(AlternateBucketDistributionTest, SizeToIndex) {
  using internal::BucketIndexLookup;

  // The first 12 buckets are the same as the default bucket index.
  for (size_t i = 1 << 0; i < 1 << 8; i <<= 1) {
    for (size_t offset = 0; offset < 4; offset++) {
      size_t n = i * (4 + offset) / 4;
      EXPECT_EQ(BucketIndexLookup::GetIndex(n),
                BucketIndexLookup::GetIndexForNeutralBuckets(n));
    }
  }

  // The alternate bucket distribution is different in the middle values.
  //
  // For each order, the top two buckets are removed compared with the default
  // distribution. Values that would be allocated in those two buckets are
  // instead allocated in the next power of two bucket.
  //
  // The first two buckets (each power of two and the next bucket up) remain
  // the same between the two bucket distributions.
  size_t expected_index = BucketIndexLookup::GetIndex(1 << 8);
  for (size_t i = 1 << 8; i < internal::kHighThresholdForAlternateDistribution;
       i <<= 1) {
    // The first two buckets in the order should match up to the normal bucket
    // distribution.
    for (size_t offset = 0; offset < 2; offset++) {
      size_t n = i * (4 + offset) / 4;
      EXPECT_EQ(BucketIndexLookup::GetIndex(n),
                BucketIndexLookup::GetIndexForNeutralBuckets(n));
      EXPECT_EQ(BucketIndexLookup::GetIndex(n), expected_index);
      expected_index += 2;
    }
    // The last two buckets in the order are "rounded up" to the same bucket
    // as the next power of two.
    expected_index += 4;
    for (size_t offset = 2; offset < 4; offset++) {
      size_t n = i * (4 + offset) / 4;
      // These two are rounded up in the alternate distribution, so we expect
      // the bucket index to be larger than the bucket index for the same
      // allocation under the default distribution.
      EXPECT_GT(BucketIndexLookup::GetIndex(n),
                BucketIndexLookup::GetIndexForNeutralBuckets(n));
      // We expect both allocations in this loop to be rounded up to the next
      // power of two bucket.
      EXPECT_EQ(BucketIndexLookup::GetIndex(n), expected_index);
    }
  }

  // The rest of the buckets all match up exactly with the existing
  // bucket distribution.
  for (size_t i = internal::kHighThresholdForAlternateDistribution;
       i < internal::kMaxBucketed; i <<= 1) {
    for (size_t offset = 0; offset < 4; offset++) {
      size_t n = i * (4 + offset) / 4;
      EXPECT_EQ(BucketIndexLookup::GetIndex(n),
                BucketIndexLookup::GetIndexForNeutralBuckets(n));
    }
  }
}

TEST_P(PartitionAllocThreadCacheTest, AllocationRecording) {
  // There is a cache.
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_TRUE(tcache);
  tcache->ResetPerThreadAllocationStatsForTesting();

  constexpr size_t kBucketedNotCached = 1 << 12;
  constexpr size_t kDirectMapped = 4 * (1 << 20);
  // Not a "nice" size on purpose, to check that the raw size accounting works.
  const size_t kSingleSlot = internal::PartitionPageSize() + 1;

  size_t expected_total_size = 0;
  void* ptr =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kSmallSize), "");
  ASSERT_TRUE(ptr);
  expected_total_size += root()->GetUsableSize(ptr);
  void* ptr2 = root()->Alloc(
      root()->AdjustSizeForExtrasSubtract(kBucketedNotCached), "");
  ASSERT_TRUE(ptr2);
  expected_total_size += root()->GetUsableSize(ptr2);
  void* ptr3 =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kDirectMapped), "");
  ASSERT_TRUE(ptr3);
  expected_total_size += root()->GetUsableSize(ptr3);
  void* ptr4 =
      root()->Alloc(root()->AdjustSizeForExtrasSubtract(kSingleSlot), "");
  ASSERT_TRUE(ptr4);
  expected_total_size += root()->GetUsableSize(ptr4);

  EXPECT_EQ(4u, tcache->thread_alloc_stats().alloc_count);
  EXPECT_EQ(expected_total_size, tcache->thread_alloc_stats().alloc_total_size);

  root()->Free(ptr);
  root()->Free(ptr2);
  root()->Free(ptr3);
  root()->Free(ptr4);

  EXPECT_EQ(4u, tcache->thread_alloc_stats().alloc_count);
  EXPECT_EQ(expected_total_size, tcache->thread_alloc_stats().alloc_total_size);
  EXPECT_EQ(4u, tcache->thread_alloc_stats().dealloc_count);
  EXPECT_EQ(expected_total_size,
            tcache->thread_alloc_stats().dealloc_total_size);

  auto stats = internal::GetAllocStatsForCurrentThread();
  EXPECT_EQ(4u, stats.alloc_count);
  EXPECT_EQ(expected_total_size, stats.alloc_total_size);
  EXPECT_EQ(4u, stats.dealloc_count);
  EXPECT_EQ(expected_total_size, stats.dealloc_total_size);
}

TEST_P(PartitionAllocThreadCacheTest, AllocationRecordingAligned) {
  // There is a cache.
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_TRUE(tcache);
  tcache->ResetPerThreadAllocationStatsForTesting();

  // Aligned allocations take different paths depending on whether they are (in
  // the same order as the test cases below):
  // - Not really aligned (since alignment is always good-enough)
  // - Already satisfied by PA's alignment guarantees
  // - Requiring extra padding
  // - Already satisfied by PA's alignment guarantees
  // - In need of a special slot span (very large alignment)
  // - Direct-mapped with large alignment
  size_t alloc_count = 0;
  size_t total_size = 0;
  size_t size_alignments[][2] = {{128, 4},
                                 {128, 128},
                                 {1024, 128},
                                 {128, 1024},
                                 {128, 2 * internal::PartitionPageSize()},
                                 {(4 << 20) + 1, 1 << 19}};
  for (auto [requested_size, alignment] : size_alignments) {
    void* ptr = root()->AlignedAlloc(alignment, requested_size);
    ASSERT_TRUE(ptr);
    alloc_count++;
    total_size += root()->GetUsableSize(ptr);
    EXPECT_EQ(alloc_count, tcache->thread_alloc_stats().alloc_count);
    EXPECT_EQ(total_size, tcache->thread_alloc_stats().alloc_total_size);
    root()->Free(ptr);
    EXPECT_EQ(alloc_count, tcache->thread_alloc_stats().dealloc_count);
    EXPECT_EQ(total_size, tcache->thread_alloc_stats().dealloc_total_size);
  }

  EXPECT_EQ(tcache->thread_alloc_stats().alloc_total_size,
            tcache->thread_alloc_stats().dealloc_total_size);

  auto stats = internal::GetAllocStatsForCurrentThread();
  EXPECT_EQ(alloc_count, stats.alloc_count);
  EXPECT_EQ(total_size, stats.alloc_total_size);
  EXPECT_EQ(alloc_count, stats.dealloc_count);
  EXPECT_EQ(total_size, stats.dealloc_total_size);
}

TEST_P(PartitionAllocThreadCacheTest, AllocationRecordingRealloc) {
  // There is a cache.
  auto* tcache = root()->thread_cache_for_testing();
  EXPECT_TRUE(tcache);
  tcache->ResetPerThreadAllocationStatsForTesting();

  size_t alloc_count = 0;
  size_t dealloc_count = 0;
  size_t total_alloc_size = 0;
  size_t total_dealloc_size = 0;
  size_t size_new_sizes[][2] = {
      {16, 15},
      {16, 64},
      {16, internal::PartitionPageSize() + 1},
      {4 << 20, 8 << 20},
      {8 << 20, 4 << 20},
      {(8 << 20) - internal::SystemPageSize(), 8 << 20}};
  for (auto [size, new_size] : size_new_sizes) {
    void* ptr = root()->Alloc(size);
    ASSERT_TRUE(ptr);
    alloc_count++;
    size_t usable_size = root()->GetUsableSize(ptr);
    total_alloc_size += usable_size;

    ptr = root()->Realloc(ptr, new_size, "");
    ASSERT_TRUE(ptr);
    total_dealloc_size += usable_size;
    dealloc_count++;
    usable_size = root()->GetUsableSize(ptr);
    total_alloc_size += usable_size;
    alloc_count++;

    EXPECT_EQ(alloc_count, tcache->thread_alloc_stats().alloc_count);
    EXPECT_EQ(total_alloc_size, tcache->thread_alloc_stats().alloc_total_size);
    EXPECT_EQ(dealloc_count, tcache->thread_alloc_stats().dealloc_count);
    EXPECT_EQ(total_dealloc_size,
              tcache->thread_alloc_stats().dealloc_total_size)
        << new_size;

    root()->Free(ptr);
    dealloc_count++;
    total_dealloc_size += usable_size;

    EXPECT_EQ(alloc_count, tcache->thread_alloc_stats().alloc_count);
    EXPECT_EQ(total_alloc_size, tcache->thread_alloc_stats().alloc_total_size);
    EXPECT_EQ(dealloc_count, tcache->thread_alloc_stats().dealloc_count);
    EXPECT_EQ(total_dealloc_size,
              tcache->thread_alloc_stats().dealloc_total_size);
  }
  EXPECT_EQ(tcache->thread_alloc_stats().alloc_total_size,
            tcache->thread_alloc_stats().dealloc_total_size);
}

// This test makes sure it's safe to switch to the alternate bucket distribution
// at runtime. This is intended to happen once, near the start of Chrome,
// once we have enabled features.
TEST(AlternateBucketDistributionTest, SwitchBeforeAlloc) {
  std::unique_ptr<PartitionAllocatorForTesting> allocator(CreateAllocator());
  PartitionRoot* root = allocator->root();

  root->SwitchToDenserBucketDistribution();
  constexpr size_t n = (1 << 12) * 3 / 2;
  EXPECT_NE(internal::BucketIndexLookup::GetIndex(n),
            internal::BucketIndexLookup::GetIndexForNeutralBuckets(n));

  void* ptr = root->Alloc(n);

  root->ResetBucketDistributionForTesting();

  root->Free(ptr);
}

// This test makes sure it's safe to switch to the alternate bucket distribution
// at runtime. This is intended to happen once, near the start of Chrome,
// once we have enabled features.
TEST(AlternateBucketDistributionTest, SwitchAfterAlloc) {
  std::unique_ptr<PartitionAllocatorForTesting> allocator(CreateAllocator());
  constexpr size_t n = (1 << 12) * 3 / 2;
  EXPECT_NE(internal::BucketIndexLookup::GetIndex(n),
            internal::BucketIndexLookup::GetIndexForNeutralBuckets(n));

  PartitionRoot* root = allocator->root();
  void* ptr = root->Alloc(n);

  root->SwitchToDenserBucketDistribution();

  void* ptr2 = root->Alloc(n);

  root->Free(ptr2);
  root->Free(ptr);
}

}  // namespace partition_alloc

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED)
