// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_tls.h"
#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/partition_alloc_buildflags.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

namespace base {

namespace internal {

class ThreadCache;

extern BASE_EXPORT PartitionTlsKey g_thread_cache_key;

// Most of these are not populated if PA_ENABLE_THREAD_CACHE_STATISTICS is not
// defined.
struct ThreadCacheStats {
  uint64_t alloc_count;   // Total allocation requests.
  uint64_t alloc_hits;    // Thread cache hits.
  uint64_t alloc_misses;  // Thread cache misses.

  // Allocation failure details:
  uint64_t alloc_miss_empty;
  uint64_t alloc_miss_too_large;

  // Cache fill details:
  uint64_t cache_fill_count;
  uint64_t cache_fill_hits;
  uint64_t cache_fill_misses;
  uint64_t cache_fill_bucket_full;
  uint64_t cache_fill_too_large;

  // Memory cost:
  uint64_t bucket_total_memory;
  uint64_t metadata_overhead;
};

// Global registry of all ThreadCache instances.
//
// This class cannot allocate in the (Un)registerThreadCache() functions, as
// they are called from ThreadCache constructor, which is from within the
// allocator. However the other members can allocate.
class BASE_EXPORT ThreadCacheRegistry {
 public:
  static ThreadCacheRegistry& Instance();
  ~ThreadCacheRegistry() = delete;

  void RegisterThreadCache(ThreadCache* cache);
  void UnregisterThreadCache(ThreadCache* cache);
  // Prints statistics for all thread caches, or this thread's only.
  void DumpStats(bool my_thread_only, ThreadCacheStats* stats);
  // Purge() this thread's cache, and asks the other ones to trigger Purge() at
  // a later point (during a deallocation).
  void PurgeAll();

  static Lock& GetLock() { return Instance().lock_; }

 private:
  friend class NoDestructor<ThreadCacheRegistry>;
  ThreadCacheRegistry();

  Lock lock_;
  ThreadCache* list_head_ GUARDED_BY(GetLock()) = nullptr;
};

// Optional statistics collection.
#if DCHECK_IS_ON()
#define PA_ENABLE_THREAD_CACHE_STATISTICS 1
#endif

#if defined(PA_ENABLE_THREAD_CACHE_STATISTICS)
#define INCREMENT_COUNTER(counter) ++counter
#define GET_COUNTER(counter) counter
#else
#define INCREMENT_COUNTER(counter) \
  do {                             \
  } while (0)
#define GET_COUNTER(counter) 0
#endif  // defined(PA_ENABLE_THREAD_CACHE_STATISTICS)

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

  static ThreadCache* Get() {
    return reinterpret_cast<ThreadCache*>(PartitionTlsGet(g_thread_cache_key));
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

  // Tries to put a memory block at |address| into the cache.
  // The block comes from the bucket at index |bucket_index| from the partition
  // this cache is for.
  //
  // Returns true if the memory was put in the cache, and false otherwise. This
  // can happen either because the cache is full or the allocation was too
  // large.
  ALWAYS_INLINE bool MaybePutInCache(void* address, size_t bucket_index);

  // Tries to allocate memory from the cache.
  // Returns nullptr for failure.
  //
  // Has the same behavior as RawAlloc(), that is: no cookie nor tag handling.
  ALWAYS_INLINE void* GetFromCache(size_t bucket_index);

  // Asks this cache to trigger |Purge()| at a later point. Can be called from
  // any thread.
  void SetShouldPurge();
  // Empties the cache.
  // The Partition lock must *not* be held when calling this.
  // Must be called from the thread this cache is for.
  void Purge();
  void AccumulateStats(ThreadCacheStats* stats) const;

  size_t bucket_count_for_testing(size_t index) const {
    return buckets_[index].count;
  }

 private:
  explicit ThreadCache(PartitionRoot<ThreadSafe>* root);

  struct Bucket {
    size_t count;
    PartitionFreelistEntry* freelist_head;
  };

  // TODO(lizeb): Optimize the threshold.
#if defined(ARCH_CPU_64_BITS)
  static constexpr size_t kBucketCount = 41;
#else
  static constexpr size_t kBucketCount = 49;
#endif
  // Checked in ThreadCache::Init(), not with static_assert() as the size is not
  // set at compile-time.
  static constexpr size_t kSizeThreshold = 512;
  static_assert(
      kBucketCount < kNumBuckets,
      "Cannot have more cached buckets than what the allocator supports");
  // TODO(lizeb): Tune this constant, and adapt it to the bucket size /
  // allocation patterns.
  static constexpr size_t kMaxCountPerBucket = 100;

  std::atomic<bool> should_purge_;
  Bucket buckets_[kBucketCount];
  ThreadCacheStats stats_;
  PartitionRoot<ThreadSafe>* root_;

  // Intrusive list since ThreadCacheRegistry::RegisterThreadCache() cannot
  // allocate.
  ThreadCache* next_ GUARDED_BY(ThreadCacheRegistry::GetLock());
  ThreadCache* prev_ GUARDED_BY(ThreadCacheRegistry::GetLock());

  friend class ThreadCacheRegistry;
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, LargeAllocationsAreNotCached);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, MultipleThreadCaches);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, RecordStats);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, ThreadCacheRegistry);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, MultipleThreadCachesAccounting);
};

ALWAYS_INLINE bool ThreadCache::MaybePutInCache(void* address,
                                                size_t bucket_index) {
  if (UNLIKELY(should_purge_.load(std::memory_order_relaxed)))
    Purge();

  INCREMENT_COUNTER(stats_.cache_fill_count);

  if (bucket_index >= kBucketCount) {
    INCREMENT_COUNTER(stats_.cache_fill_too_large);
    INCREMENT_COUNTER(stats_.cache_fill_misses);
    return false;
  }

  auto& bucket = buckets_[bucket_index];

  if (bucket.count >= kMaxCountPerBucket) {
    INCREMENT_COUNTER(stats_.cache_fill_bucket_full);
    INCREMENT_COUNTER(stats_.cache_fill_misses);
    return false;
  }

  PA_DCHECK(bucket.count != 0 || bucket.freelist_head == nullptr);

  auto* entry = reinterpret_cast<PartitionFreelistEntry*>(address);
  entry->next = PartitionFreelistEntry::Encode(bucket.freelist_head);
  bucket.freelist_head = entry;
  bucket.count++;

  INCREMENT_COUNTER(stats_.cache_fill_hits);
  return true;
}

ALWAYS_INLINE void* ThreadCache::GetFromCache(size_t bucket_index) {
  INCREMENT_COUNTER(stats_.alloc_count);
  // Only handle "small" allocations.
  if (bucket_index >= kBucketCount) {
    INCREMENT_COUNTER(stats_.alloc_miss_too_large);
    INCREMENT_COUNTER(stats_.alloc_misses);
    return nullptr;
  }

  auto& bucket = buckets_[bucket_index];
  auto* result = bucket.freelist_head;
  if (!result) {
    PA_DCHECK(bucket.count == 0);
    INCREMENT_COUNTER(stats_.alloc_miss_empty);
    INCREMENT_COUNTER(stats_.alloc_misses);
    return nullptr;
  }

  PA_DCHECK(bucket.count != 0);
  auto* next = EncodedPartitionFreelistEntry::Decode(result->next);
  PA_DCHECK(result != next);
  bucket.count--;
  PA_DCHECK(bucket.count != 0 || !next);
  bucket.freelist_head = next;

  INCREMENT_COUNTER(stats_.alloc_hits);
  return result;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
