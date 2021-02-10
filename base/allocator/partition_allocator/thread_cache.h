// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_

#include <atomic>
#include <memory>

#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/allocator/partition_allocator/partition_tls.h"
#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/partition_alloc_buildflags.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"

// Need TLS support.
#if defined(OS_POSIX) || defined(OS_WIN)
#define PA_THREAD_CACHE_SUPPORTED
#endif

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
  void OnDeallocation();

  static PartitionLock& GetLock() { return Instance().lock_; }
  // Purges all thread caches *now*. This is completely thread-unsafe, and
  // should only be called in a post-fork() handler.
  void ForcePurgeAllThreadAfterForkUnsafe();

  bool has_pending_purge_task() const { return has_pending_purge_task_; }
  void ResetForTesting();

  static constexpr TimeDelta kPurgeInterval = TimeDelta::FromSeconds(1);
  static constexpr int kMinMainThreadAllocationsForPurging = 1000;

 private:
  void PeriodicPurge();
  void PostDelayedPurgeTask();
  friend class NoDestructor<ThreadCacheRegistry>;
  // Not using base::Lock as the object's constructor must be constexpr.
  PartitionLock lock_;
  ThreadCache* list_head_ GUARDED_BY(GetLock()) = nullptr;
  uint64_t allocations_at_last_purge_ = 0;
  int deallocations_ = 0;
  bool has_pending_purge_task_ = false;
};

constexpr ThreadCacheRegistry::ThreadCacheRegistry() = default;

// Optional statistics collection.
#define PA_ENABLE_THREAD_CACHE_STATISTICS 1

#if defined(PA_ENABLE_THREAD_CACHE_STATISTICS)
#define INCREMENT_COUNTER(counter) ++counter
#define GET_COUNTER(counter) counter
#else
#define INCREMENT_COUNTER(counter) \
  do {                             \
  } while (0)
#define GET_COUNTER(counter) 0
#endif  // defined(PA_ENABLE_THREAD_CACHE_STATISTICS)

ALWAYS_INLINE static constexpr int ConstexprLog2(size_t n) {
  return n < 1 ? -1 : (n < 2 ? 0 : (1 + ConstexprLog2(n >> 1)));
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
  void SetNotifiesRegistry(bool enabled);
  // Empties the cache.
  // The Partition lock must *not* be held when calling this.
  // Must be called from the thread this cache is for.
  void Purge();
  void AccumulateStats(ThreadCacheStats* stats) const;

  // Disables the thread cache for its associated root.
  void Disable();
  void Enable();

  size_t bucket_count_for_testing(size_t index) const {
    return buckets_[index].count;
  }

  // TODO(lizeb): Once we have periodic purge, lower the ratio.
  static constexpr uint16_t kBatchFillRatio = 8;
  static constexpr uint8_t kMaxCountPerBucket = 128;

  // TODO(lizeb): Optimize the threshold.
  static constexpr size_t kSizeThreshold = 512;

 private:
  struct Bucket {
    PartitionFreelistEntry* freelist_head;
    // Want to keep sizeof(Bucket) small, using small types.
    uint8_t count;
    uint8_t limit;
    uint16_t slot_size;
  };
  enum class Mode { kNormal, kPurge, kNotifyRegistry };

  explicit ThreadCache(PartitionRoot<ThreadSafe>* root);
  static void Delete(void* thread_cache_ptr);
  void PurgeInternal();
  // Fills a bucket from the central allocator.
  void FillBucket(size_t bucket_index);
  // Empties the |bucket| until there are at most |limit| objects in it.
  void ClearBucket(Bucket& bucket, size_t limit);
  ALWAYS_INLINE void PutInBucket(Bucket& bucket, void* slot_start);
  void HandleNonNormalMode();
  void ResetForTesting();

  static constexpr uint16_t kBucketCount =
      ((ConstexprLog2(kSizeThreshold) - kMinBucketedOrder + 1)
       << kNumBucketsPerOrderBits) +
      1;
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

  std::atomic<Mode> mode_{Mode::kNormal};
  Bucket buckets_[kBucketCount];
  ThreadCacheStats stats_;
  PartitionRoot<ThreadSafe>* const root_;
  ThreadCacheRegistry* const registry_;
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
};

ALWAYS_INLINE bool ThreadCache::MaybePutInCache(void* slot_start,
                                                size_t bucket_index) {
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  INCREMENT_COUNTER(stats_.cache_fill_count);

  if (UNLIKELY(bucket_index >= kBucketCount)) {
    INCREMENT_COUNTER(stats_.cache_fill_misses);
    return false;
  }

  auto& bucket = buckets_[bucket_index];

  PA_DCHECK(bucket.count != 0 || bucket.freelist_head == nullptr);

  PutInBucket(bucket, slot_start);
  INCREMENT_COUNTER(stats_.cache_fill_hits);

  // Batched deallocation, amortizing lock acquisitions.
  if (UNLIKELY(bucket.count >= bucket.limit)) {
    ClearBucket(bucket, bucket.limit / 2);
  }

  if (UNLIKELY(mode_.load(std::memory_order_relaxed) != Mode::kNormal))
    HandleNonNormalMode();

  return true;
}

ALWAYS_INLINE void* ThreadCache::GetFromCache(size_t bucket_index,
                                              size_t* slot_size) {
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  INCREMENT_COUNTER(stats_.alloc_count);
  // Only handle "small" allocations.
  if (UNLIKELY(bucket_index >= kBucketCount)) {
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
