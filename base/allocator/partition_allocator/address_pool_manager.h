// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_

#include <bitset>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/thread_annotations.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "build/build_config.h"

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#endif

namespace partition_alloc {

class AddressSpaceStatsDumper;
struct AddressSpaceStats;
struct PoolStats;

}  // namespace partition_alloc

namespace partition_alloc::internal {

// (64bit version)
// AddressPoolManager takes a reserved virtual address space and manages address
// space allocation.
//
// AddressPoolManager (currently) supports up to 4 pools. Each pool manages a
// contiguous reserved address space. Alloc() takes a pool_handle and returns
// address regions from the specified pool. Free() also takes a pool_handle and
// returns the address region back to the manager.
//
// (32bit version)
// AddressPoolManager wraps AllocPages and FreePages and remembers allocated
// address regions using bitmaps. IsManagedByPartitionAlloc*Pool use the bitmaps
// to judge whether a given address is in a pool that supports BackupRefPtr or
// in a pool that doesn't. All PartitionAlloc allocations must be in either of
// the pools.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) AddressPoolManager {
 public:
  static AddressPoolManager& GetInstance();

  AddressPoolManager(const AddressPoolManager&) = delete;
  AddressPoolManager& operator=(const AddressPoolManager&) = delete;

#if BUILDFLAG(HAS_64_BIT_POINTERS)
  void Add(pool_handle handle, uintptr_t address, size_t length);
  void Remove(pool_handle handle);

  // Populate a |used| bitset of superpages currently in use.
  void GetPoolUsedSuperPages(pool_handle handle,
                             std::bitset<kMaxSuperPagesInPool>& used);

  // Return the base address of a pool.
  uintptr_t GetPoolBaseAddress(pool_handle handle);
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

  // Reserves address space from the pool.
  uintptr_t Reserve(pool_handle handle,
                    uintptr_t requested_address,
                    size_t length);

  // Frees address space back to the pool and decommits underlying system pages.
  void UnreserveAndDecommit(pool_handle handle,
                            uintptr_t address,
                            size_t length);
  void ResetForTesting();

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  void MarkUsed(pool_handle handle, uintptr_t address, size_t size);
  void MarkUnused(pool_handle handle, uintptr_t address, size_t size);

  static bool IsManagedByRegularPool(uintptr_t address) {
    return AddressPoolManagerBitmap::IsManagedByRegularPool(address);
  }

  static bool IsManagedByBRPPool(uintptr_t address) {
    return AddressPoolManagerBitmap::IsManagedByBRPPool(address);
  }
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS)

  void DumpStats(AddressSpaceStatsDumper* dumper);

 private:
  friend class AddressPoolManagerForTesting;
#if BUILDFLAG(ENABLE_PKEYS)
  // If we use a pkey pool, we need to tag its metadata with the pkey. Allow the
  // function to get access to the pool pointer.
  friend void TagGlobalsWithPkey(int pkey);
#endif

  constexpr AddressPoolManager() = default;
  ~AddressPoolManager() = default;

  // Populates `stats` if applicable.
  // Returns whether `stats` was populated. (They might not be, e.g.
  // if PartitionAlloc is wholly unused in this process.)
  bool GetStats(AddressSpaceStats* stats);

#if BUILDFLAG(HAS_64_BIT_POINTERS)
  class Pool {
   public:
    constexpr Pool() = default;
    ~Pool() = default;

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    void Initialize(uintptr_t ptr, size_t length);
    bool IsInitialized();
    void Reset();

    uintptr_t FindChunk(size_t size);
    void FreeChunk(uintptr_t address, size_t size);

    bool TryReserveChunk(uintptr_t address, size_t size);

    void GetUsedSuperPages(std::bitset<kMaxSuperPagesInPool>& used);
    uintptr_t GetBaseAddress();

    void GetStats(PoolStats* stats);

   private:
    Lock lock_;

    // The bitset stores the allocation state of the address pool. 1 bit per
    // super-page: 1 = allocated, 0 = free.
    std::bitset<kMaxSuperPagesInPool> alloc_bitset_ PA_GUARDED_BY(lock_);

    // An index of a bit in the bitset before which we know for sure there all
    // 1s. This is a best-effort hint in the sense that there still may be lots
    // of 1s after this index, but at least we know there is no point in
    // starting the search before it.
    size_t bit_hint_ PA_GUARDED_BY(lock_) = 0;

    size_t total_bits_ = 0;
    uintptr_t address_begin_ = 0;
#if BUILDFLAG(PA_DCHECK_IS_ON)
    uintptr_t address_end_ = 0;
#endif
  };

  PA_ALWAYS_INLINE Pool* GetPool(pool_handle handle) {
    PA_DCHECK(kNullPoolHandle < handle && handle <= kNumPools);
    return &aligned_pools_.pools_[handle - 1];
  }

  // Gets the stats for the pool identified by `handle`, if
  // initialized.
  void GetPoolStats(pool_handle handle, PoolStats* stats);

  // If pkey support is enabled, we need to pkey-tag the pkey pool (which needs
  // to be last). For this, we need to add padding in front of the pools so that
  // pkey one starts on a page boundary.
  struct {
    char pad_[PA_PKEY_ARRAY_PAD_SZ(Pool, kNumPools)] = {};
    Pool pools_[kNumPools];
    char pad_after_[PA_PKEY_FILL_PAGE_SZ(sizeof(Pool))] = {};
  } aligned_pools_ PA_PKEY_ALIGN;

#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

  static PA_CONSTINIT AddressPoolManager singleton_;
};

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
