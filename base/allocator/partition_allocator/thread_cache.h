// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_

#include <atomic>
#include <memory>

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/allocator/partition_allocator/partition_tls.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"

namespace base {

namespace internal {

class ThreadCache;

extern BASE_EXPORT PartitionTlsKey g_thread_cache_key;

// Global registry of all ThreadCache instances.
//
// This class cannot allocate in the (Un)registerThreadCache() functions, as
// they are called from ThreadCache constructor, which is from within the
// allocator. However the other members can allocate.
class BASE_EXPORT ThreadCacheRegistry {
 public:
  static ThreadCacheRegistry& Instance();
  // Do not instantiate.
  //
  // Several things are surprising here:
  // - The constructor is public even though this is intended to be a singleton:
  //   we cannot use a "static local" variable in |Instance()| as this is
  //   reached too early during CRT initialization on Windows, meaning that
  //   static local variables don't work (as they call into the uninitialized
  //   runtime). To sidestep that, we use a regular global variable in the .cc,
  //   which is fine as this object's constructor is constexpr.
  // - Marked inline so that the chromium style plugin doesn't complain that a
  //   "complex constructor" has an inline body. This warning is disabled when
  //   the constructor is explicitly marked "inline". Note that this is a false
  //   positive of the plugin, since constexpr implies inline.
  inline constexpr ThreadCacheRegistry();

  void RegisterThreadCache(ThreadCache* cache);
  void UnregisterThreadCache(ThreadCache* cache);
  // Prints statistics for all thread caches, or this thread's only.
  void DumpStats(bool my_thread_only, ThreadCacheStats* stats);
  // Purge() this thread's cache, and asks the other ones to trigger Purge() at
  // a later point (during a deallocation).
  void PurgeAll();

  // Starts a periodic timer on the current thread to purge all thread caches.
  void StartPeriodicPurge();

  // Controls the thread cache size, by setting the multiplier to a value above
  // or below |ThreadCache::kDefaultMultiplier|.
  void SetThreadCacheMultiplier(float multiplier);

  static PartitionLock& GetLock() { return Instance().lock_; }
  // Purges all thread caches *now*. This is completely thread-unsafe, and
  // should only be called in a post-fork() handler.
  void ForcePurgeAllThreadAfterForkUnsafe();

  base::TimeDelta purge_interval_for_testing() const { return purge_interval_; }

  void ResetForTesting();

  static constexpr TimeDelta kMinPurgeInterval = TimeDelta::FromSeconds(1);
  static constexpr TimeDelta kMaxPurgeInterval = TimeDelta::FromMinutes(1);
  static constexpr TimeDelta kDefaultPurgeInterval = 2 * kMinPurgeInterval;
  static constexpr size_t kMinCachedMemoryForPurging = 500 * 1024;

 private:
  void PeriodicPurge();
  void PostDelayedPurgeTask();
  friend class NoDestructor<ThreadCacheRegistry>;
  // Not using base::Lock as the object's constructor must be constexpr.
  PartitionLock lock_;
  ThreadCache* list_head_ GUARDED_BY(GetLock()) = nullptr;
  base::TimeDelta purge_interval_ = kDefaultPurgeInterval;
  bool periodic_purge_running_ = false;
};

constexpr ThreadCacheRegistry::ThreadCacheRegistry() = default;

#if defined(PA_THREAD_CACHE_ENABLE_STATISTICS)
#define INCREMENT_COUNTER(counter) ++counter
#define GET_COUNTER(counter) counter
#else
#define INCREMENT_COUNTER(counter) \
  do {                             \
  } while (0)
#define GET_COUNTER(counter) 0
#endif  // defined(PA_THREAD_CACHE_ENABLE_STATISTICS)

ALWAYS_INLINE static constexpr int ConstexprLog2(size_t n) {
  return n < 1 ? -1 : (n < 2 ? 0 : (1 + ConstexprLog2(n >> 1)));
}

ALWAYS_INLINE static constexpr uint16_t BucketIndexForSize(size_t size) {
  return ((ConstexprLog2(size) - kMinBucketedOrder + 1)
          << kNumBucketsPerOrderBits);
}

#if DCHECK_IS_ON()
class ReentrancyGuard {
 public:
  explicit ReentrancyGuard(bool& flag) : flag_(flag) {
    PA_CHECK(!flag_);
    flag_ = true;
  }

  ~ReentrancyGuard() { flag_ = false; }

 private:
  bool& flag_;
};

#define PA_REENTRANCY_GUARD(x) \
  ReentrancyGuard guard { x }

#else

#define PA_REENTRANCY_GUARD(x) \
  do {                         \
  } while (0)

#endif  // DCHECK_IS_ON()

// Per-thread cache. *Not* threadsafe, must only be accessed from a single
// thread.
//
// In practice, this is easily enforced as long as only |instance| is
// manipulated, as it is a thread_local member. As such, any
// |ThreadCache::instance->*()| call will necessarily be done from a single
// thread.
class BASE_EXPORT ThreadCache {
 public:
  // Initializes the thread cache for |root|. May allocate, so should be called
  // with the thread cache disabled on the partition side, and without the
  // partition lock held.
  //
  // May only be called by a single PartitionRoot.
  static void Init(PartitionRoot<ThreadSafe>* root);
  static void Init(PartitionRoot<NotThreadSafe>* root) { IMMEDIATE_CRASH(); }

  // Can be called several times, must be called before any ThreadCache
  // interactions.
  static void EnsureThreadSpecificDataInitialized();

  static ThreadCache* Get() {
    return reinterpret_cast<ThreadCache*>(PartitionTlsGet(g_thread_cache_key));
  }

  static bool IsValid(ThreadCache* tcache) {
    return reinterpret_cast<uintptr_t>(tcache) & kTombstoneMask;
  }

  static bool IsTombstone(ThreadCache* tcache) {
    return reinterpret_cast<uintptr_t>(tcache) == kTombstone;
  }

  // Create a new ThreadCache associated with |root|.
  // Must be called without the partition locked, as this may allocate.
  static ThreadCache* Create(PartitionRoot<ThreadSafe>* root);
  static ThreadCache* Create(PartitionRoot<NotThreadSafe>* root) {
    IMMEDIATE_CRASH();
  }

  ~ThreadCache();

  // Force placement new.
  void* operator new(size_t) = delete;
  void* operator new(size_t, void* buffer) { return buffer; }
  void operator delete(void* ptr) = delete;
  ThreadCache(const ThreadCache&) = delete;
  ThreadCache(const ThreadCache&&) = delete;
  ThreadCache& operator=(const ThreadCache&) = delete;

  // Tries to put a slot at |slot_start| into the cache.
  // The slot comes from the bucket at index |bucket_index| from the partition
  // this cache is for.
  //
  // Returns true if the slot was put in the cache, and false otherwise. This
  // can happen either because the cache is full or the allocation was too
  // large.
  ALWAYS_INLINE bool MaybePutInCache(void* slot_start, size_t bucket_index);

  // Tries to allocate a memory slot from the cache.
  // Returns nullptr on failure.
  //
  // Has the same behavior as RawAlloc(), that is: no cookie nor ref-count
  // handling. Sets |slot_size| to the allocated size upon success.
  ALWAYS_INLINE void* GetFromCache(size_t bucket_index, size_t* slot_size);

  // Asks this cache to trigger |Purge()| at a later point. Can be called from
  // any thread.
  void SetShouldPurge();
  // Empties the cache.
  // The Partition lock must *not* be held when calling this.
  // Must be called from the thread this cache is for.
  void Purge();
  // Amount of cached memory for this thread's cache, in bytes.
  size_t CachedMemory() const;
  void AccumulateStats(ThreadCacheStats* stats) const;

  // Purge the thread cache of the current thread, if one exists.
  static void PurgeCurrentThread();

  size_t bucket_count_for_testing(size_t index) const {
    return buckets_[index].count;
  }

  // Sets the maximum size of allocations that may be cached by the thread
  // cache. This applies to all threads. However, the maximum size is bounded by
  // |kLargeSizeThreshold|.
  static void SetLargestCachedSize(size_t size);

  // Fill 1 / kBatchFillRatio * bucket.limit slots at a time.
  static constexpr uint16_t kBatchFillRatio = 8;

  // Limit for the smallest bucket will be kDefaultMultiplier *
  // kSmallBucketBaseCount by default.
  static constexpr float kDefaultMultiplier = 2.;
  static constexpr uint8_t kSmallBucketBaseCount = 64;

  // When trying to conserve memory, set the thread cache limit to this.
  static constexpr size_t kDefaultSizeThreshold = 512;
  // 32kiB is chosen here as from local experiments, "zone" allocation in
  // V8 is performance-sensitive, and zones can (and do) grow up to 32kiB for
  // each individual allocation.
  static constexpr size_t kLargeSizeThreshold = 1 << 15;
  static_assert(kLargeSizeThreshold <= std::numeric_limits<uint16_t>::max(),
                "");

 private:
  struct Bucket {
    PartitionFreelistEntry* freelist_head = nullptr;
    // Want to keep sizeof(Bucket) small, using small types.
    uint8_t count = 0;
    std::atomic<uint8_t> limit{};  // Can be changed from another thread.
    uint16_t slot_size = 0;

    Bucket();
  };
  static_assert(sizeof(Bucket) <= 2 * sizeof(void*), "Keep Bucket small.");
  enum class Mode { kNormal, kPurge, kNotifyRegistry };

  explicit ThreadCache(PartitionRoot<ThreadSafe>* root);
  static void Delete(void* thread_cache_ptr);
  void PurgeInternal();
  // Fills a bucket from the central allocator.
  void FillBucket(size_t bucket_index);
  // Empties the |bucket| until there are at most |limit| objects in it.
  void ClearBucket(Bucket& bucket, size_t limit);
  ALWAYS_INLINE void PutInBucket(Bucket& bucket, void* slot_start);
  void ResetForTesting();
  // Releases the entire freelist starting at |head| to the root.
  void FreeAfter(PartitionFreelistEntry* head);
  static void SetGlobalLimits(PartitionRoot<ThreadSafe>* root,
                              float multiplier);

  static constexpr uint16_t kBucketCount =
      BucketIndexForSize(kLargeSizeThreshold) + 1;
  static_assert(
      kBucketCount < kNumBuckets,
      "Cannot have more cached buckets than what the allocator supports");

  // On some architectures, ThreadCache::Get() can be called and return
  // something after the thread cache has been destroyed. In this case, we set
  // it to this value, to signal that the thread is being terminated, and the
  // thread cache should not be used.
  //
  // This happens in particular on Windows, during program termination.
  //
  // We choose 0x1 as the value as it is an invalid pointer value, since it is
  // not aligned, and too low. Also, checking !(ptr & kTombstoneMask) checks for
  // nullptr and kTombstone at the same time.
  static constexpr uintptr_t kTombstone = 0x1;
  static constexpr uintptr_t kTombstoneMask = ~kTombstone;

  static uint8_t global_limits_[kBucketCount];
  // Index of the largest active bucket. Not all processes/platforms will use
  // all buckets, as using larger buckets increases the memory footprint.
  //
  // TODO(lizeb): Investigate making this per-thread rather than static, to
  // improve locality, and open the door to per-thread settings.
  static uint16_t largest_active_bucket_index_;

  Bucket buckets_[kBucketCount];
  size_t cached_memory_ = 0;
  std::atomic<bool> should_purge_;
  ThreadCacheStats stats_;
  PartitionRoot<ThreadSafe>* const root_;
#if DCHECK_IS_ON()
  bool is_in_thread_cache_ = false;
#endif

  // Intrusive list since ThreadCacheRegistry::RegisterThreadCache() cannot
  // allocate.
  ThreadCache* next_ GUARDED_BY(ThreadCacheRegistry::GetLock());
  ThreadCache* prev_ GUARDED_BY(ThreadCacheRegistry::GetLock());

  friend class ThreadCacheRegistry;
  friend class ThreadCacheTest;
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, Simple);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, MultipleObjectsCachedPerBucket);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, LargeAllocationsAreNotCached);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, MultipleThreadCaches);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, RecordStats);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, ThreadCacheRegistry);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, MultipleThreadCachesAccounting);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, DynamicCountPerBucket);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, DynamicCountPerBucketClamping);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest,
                           DynamicCountPerBucketMultipleThreads);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, DynamicSizeThreshold);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, DynamicSizeThresholdPurge);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, ClearFromTail);
};

ALWAYS_INLINE bool ThreadCache::MaybePutInCache(void* slot_start,
                                                size_t bucket_index) {
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  INCREMENT_COUNTER(stats_.cache_fill_count);

  if (UNLIKELY(bucket_index > largest_active_bucket_index_)) {
    INCREMENT_COUNTER(stats_.cache_fill_misses);
    return false;
  }

  auto& bucket = buckets_[bucket_index];

  PA_DCHECK(bucket.count != 0 || bucket.freelist_head == nullptr);

  PutInBucket(bucket, slot_start);
  cached_memory_ += bucket.slot_size;
  INCREMENT_COUNTER(stats_.cache_fill_hits);

  // Relaxed ordering: we don't care about having an up-to-date or consistent
  // value, just want it to not change while we are using it, hence using
  // relaxed ordering, and loading into a local variable. Without it, we are
  // gambling that the compiler would not issue multiple loads.
  uint8_t limit = bucket.limit.load(std::memory_order_relaxed);
  // Batched deallocation, amortizing lock acquisitions.
  if (UNLIKELY(bucket.count > limit)) {
    ClearBucket(bucket, limit / 2);
  }

  if (UNLIKELY(should_purge_.load(std::memory_order_relaxed)))
    PurgeInternal();

  return true;
}

ALWAYS_INLINE void* ThreadCache::GetFromCache(size_t bucket_index,
                                              size_t* slot_size) {
#if defined(PA_THREAD_CACHE_ALLOC_STATS)
  stats_.allocs_per_bucket_[bucket_index]++;
#endif

  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  INCREMENT_COUNTER(stats_.alloc_count);
  // Only handle "small" allocations.
  if (UNLIKELY(bucket_index > largest_active_bucket_index_)) {
    INCREMENT_COUNTER(stats_.alloc_miss_too_large);
    INCREMENT_COUNTER(stats_.alloc_misses);
    return nullptr;
  }

  auto& bucket = buckets_[bucket_index];
  if (LIKELY(bucket.freelist_head)) {
    INCREMENT_COUNTER(stats_.alloc_hits);
  } else {
    PA_DCHECK(bucket.count == 0);
    INCREMENT_COUNTER(stats_.alloc_miss_empty);
    INCREMENT_COUNTER(stats_.alloc_misses);

    FillBucket(bucket_index);

    // Very unlikely, means that the central allocator is out of memory. Let it
    // deal with it (may return nullptr, may crash).
    if (UNLIKELY(!bucket.freelist_head))
      return nullptr;
  }

  PA_DCHECK(bucket.count != 0);
  auto* result = bucket.freelist_head;
  auto* next = result->GetNext();
  PA_DCHECK(result != next);
  bucket.count--;
  PA_DCHECK(bucket.count != 0 || !next);
  bucket.freelist_head = next;
  *slot_size = bucket.slot_size;

  PA_DCHECK(cached_memory_ >= bucket.slot_size);
  cached_memory_ -= bucket.slot_size;
  return result;
}

ALWAYS_INLINE void ThreadCache::PutInBucket(Bucket& bucket, void* slot_start) {
  auto* entry = PartitionFreelistEntry::InitForThreadCache(
      slot_start, bucket.freelist_head);
  bucket.freelist_head = entry;
  bucket.count++;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
