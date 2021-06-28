// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_

#include <bitset>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

namespace base {

template <typename Type>
struct LazyInstanceTraitsBase;

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
// address regions using bitmaps. IsManagedByPartitionAllocBRPPool and
// IsManagedByPartitionAllocNonBRPPool use the bitmaps to judge whether a given
// address is in a pool that supports BackupRefPtr or in a pool that doesn't.
// All PartitionAlloc allocations must be in either of the pools.
class BASE_EXPORT AddressPoolManager {
 public:
  static AddressPoolManager* GetInstance();

#if defined(PA_HAS_64_BITS_POINTERS)
  pool_handle Add(uintptr_t address, size_t length);
  void Remove(pool_handle handle);

  // Populate a |used| bitset of superpages currently in use.
  void GetPoolUsedSuperPages(pool_handle handle,
                             std::bitset<kMaxSuperPages>& used);

  // Return the base address of a pool.
  uintptr_t GetPoolBaseAddress(pool_handle handle);
#endif

  // Reserves address space from GigaCage.
  char* Reserve(pool_handle handle, void* requested_address, size_t length);

  // Frees address space back to GigaCage and decommits underlying system pages.
  void UnreserveAndDecommit(pool_handle handle, void* ptr, size_t length);
  void ResetForTesting();

#if !defined(PA_HAS_64_BITS_POINTERS)
  void MarkUsed(pool_handle handle, const void* address, size_t size);
  void MarkUnused(pool_handle handle, const void* address, size_t size);

  static bool IsManagedByNonBRPPool(const void* address) {
    return AddressPoolManagerBitmap::IsManagedByNonBRPPool(address);
  }

  static bool IsManagedByBRPPool(const void* address) {
    return AddressPoolManagerBitmap::IsManagedByBRPPool(address);
  }
#endif  // !defined(PA_HAS_64_BITS_POINTERS)

 private:
  friend class AddressPoolManagerForTesting;

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

    bool TryReserveChunk(uintptr_t address, size_t size);

    void GetUsedSuperPages(std::bitset<kMaxSuperPages>& used);
    uintptr_t GetBaseAddress();

   private:
    base::Lock lock_;

    // The bitset stores the allocation state of the address pool. 1 bit per
    // super-page: 1 = allocated, 0 = free.
    std::bitset<kMaxSuperPages> alloc_bitset_ GUARDED_BY(lock_);

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

  ALWAYS_INLINE Pool* GetPool(pool_handle handle) {
    PA_DCHECK(0 < handle && handle <= kNumPools);
    return &pools_[handle - 1];
  }

  static constexpr size_t kNumPools = 2;
  Pool pools_[kNumPools];

#else  // defined(PA_HAS_64_BITS_POINTERS)

  // BRP stands for BackupRefPtr. GigaCage is split into pools, one which
  // supports BackupRefPtr and one that doesn't.
  static constexpr pool_handle kNonBRPPoolHandle = 1;
  static constexpr pool_handle kBRPPoolHandle = 2;
  friend pool_handle GetNonBRPPool();
  friend pool_handle GetBRPPool();

#endif  // defined(PA_HAS_64_BITS_POINTERS)

  friend struct base::LazyInstanceTraitsBase<AddressPoolManager>;
  DISALLOW_COPY_AND_ASSIGN(AddressPoolManager);
};

#if !defined(PA_HAS_64_BITS_POINTERS)
ALWAYS_INLINE pool_handle GetNonBRPPool() {
  return AddressPoolManager::kNonBRPPoolHandle;
}

ALWAYS_INLINE pool_handle GetBRPPool() {
  return AddressPoolManager::kBRPPoolHandle;
}

#endif  // !defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
