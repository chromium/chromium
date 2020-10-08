// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_

#include <bitset>

#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

namespace base {

namespace internal {

// (64bit version)
// AddressPoolManager takes a reserved virtual address space and manages address
// space allocation.
//
// AddressPoolManager (currently) supports up to 2 pools. Each pool manages a
// contiguous reserved address space. Alloc() takes a pool_handle and returns
// address regions from the specified pool. Free() also takes a pool_handle and
// returns the address region back to the manager.
//
// (32bit version)
// AddressPoolManager wraps AllocPages and FreePages and remembers allocated
// address regions using bitmaps. IsManagedByPartitionAllocDirectMap and
// IsManagedByPartitionAllocNormalBuckets use the bitmaps to judge whether a
// given address is managed by the direct map or normal buckets.
class BASE_EXPORT AddressPoolManager {
 public:
  static AddressPoolManager* GetInstance();

#if defined(PA_HAS_64_BITS_POINTERS)
  pool_handle Add(uintptr_t address, size_t length);
  void Remove(pool_handle handle);
#endif
  char* Alloc(pool_handle handle, void* requested_address, size_t length);
  void Free(pool_handle handle, void* ptr, size_t length);
  void ResetForTesting();

#if !defined(PA_HAS_64_BITS_POINTERS)
  static bool IsManagedByDirectMapPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    return TS_UNCHECKED_READ(directmap_bits_)
        .test(address_as_uintptr / PageAllocationGranularity());
  }

  static bool IsManagedByNormalBucketPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    return TS_UNCHECKED_READ(normal_bucket_bits_)
        .test(address_as_uintptr >> kSuperPageShift);
  }
#endif

 private:
  AddressPoolManager();
  ~AddressPoolManager();

#if defined(PA_HAS_64_BITS_POINTERS)
  class Pool {
   public:
    Pool();
    ~Pool();

    void Initialize(uintptr_t ptr, size_t length);
    bool IsInitialized();
    void Reset();

    uintptr_t FindChunk(size_t size);
    void FreeChunk(uintptr_t address, size_t size);

   private:
    // The bitset stores the allocation state of the address pool. 1 bit per
    // super-page: 1 = allocated, 0 = free.
    static constexpr size_t kGiB = 1024 * 1024 * 1024;
    static constexpr size_t kMaxSupportedSize = 16 * kGiB;
    static constexpr size_t kMaxBits = kMaxSupportedSize / kSuperPageSize;
    base::Lock lock_;
    std::bitset<kMaxBits> alloc_bitset_ GUARDED_BY(lock_);
    // An index of a bit in the bitset before which we know for sure there all
    // 1s. This is a best-effort hint in the sense that there still may be lots
    // of 1s after this index, but at least we know there is no point in
    // starting the search before it.
    size_t bit_hint_ GUARDED_BY(lock_);

    size_t total_bits_ = 0;
    uintptr_t address_begin_ = 0;
#if DCHECK_IS_ON()
    uintptr_t address_end_ = 0;
#endif
  };

  ALWAYS_INLINE Pool* GetPool(pool_handle handle);

  static constexpr size_t kNumPools = 2;
  Pool pools_[kNumPools];

#else   // defined(PA_HAS_64_BITS_POINTERS)

  static constexpr size_t kGiB = 1024 * 1024 * 1024;
  static constexpr uint64_t kAddressSpaceSize = 4ULL * kGiB;
  static constexpr size_t kNormalBucketBits =
      kAddressSpaceSize / kSuperPageSize;
  static constexpr size_t kDirectMapBits =
      kAddressSpaceSize / PageAllocationGranularity();

  void MarkUsed(pool_handle handle, const char* address, size_t size);
  void MarkUnused(pool_handle handle, uintptr_t address, size_t size);

  static Lock& GetLock();

  static std::bitset<kDirectMapBits> directmap_bits_ GUARDED_BY(GetLock());
  static std::bitset<kNormalBucketBits> normal_bucket_bits_
      GUARDED_BY(GetLock());

  static constexpr pool_handle kDirectMapHandle = 1;
  static constexpr pool_handle kNormalBucketHandle = 2;
  friend internal::pool_handle GetDirectMapPool();
  friend internal::pool_handle GetNormalBucketPool();
#endif  // defined(PA_HAS_64_BITS_POINTERS)

  friend struct base::LazyInstanceTraitsBase<AddressPoolManager>;
  DISALLOW_COPY_AND_ASSIGN(AddressPoolManager);
};

#if !defined(PA_HAS_64_BITS_POINTERS)
ALWAYS_INLINE internal::pool_handle GetDirectMapPool() {
  return AddressPoolManager::kDirectMapHandle;
}

ALWAYS_INLINE internal::pool_handle GetNormalBucketPool() {
  return AddressPoolManager::kNormalBucketHandle;
}
#endif

}  // namespace internal

#if !defined(PA_HAS_64_BITS_POINTERS)
ALWAYS_INLINE bool IsManagedByPartitionAllocDirectMap(const void* address) {
  return internal::AddressPoolManager::IsManagedByDirectMapPool(address);
}

ALWAYS_INLINE bool IsManagedByPartitionAllocNormalBuckets(const void* address) {
  return internal::AddressPoolManager::IsManagedByNormalBucketPool(address);
}
#endif

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
