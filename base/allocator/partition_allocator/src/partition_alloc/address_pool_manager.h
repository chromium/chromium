// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_ADDRESS_POOL_MANAGER_H_
#define PARTITION_ALLOC_ADDRESS_POOL_MANAGER_H_

#include <array>
#include <atomic>
#include <bitset>
#include <limits>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/partition_address_space_internal.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_public_constants.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/thread_isolation/alignment.h"
#include "partition_alloc/thread_isolation/pkey.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/address_pool_manager_bitmap.h"
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
class PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    PA_THREAD_ISOLATED_ALIGN AddressPoolManager {
 public:
  static AddressPoolManager& GetInstance();

  AddressPoolManager(const AddressPoolManager&) = delete;
  AddressPoolManager& operator=(const AddressPoolManager&) = delete;

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  void Add(pool_handle handle,
           uintptr_t address,
           size_t length,
           int pkey = kInvalidPkey);
  void Remove(pool_handle handle);

  // Populate a |used| bitset of superpages currently in use.
  void GetPoolUsedSuperPages(pool_handle handle,
                             std::bitset<kMaxSuperPagesInPool>& used);

  // Return the base address of a pool.
  uintptr_t GetPoolBaseAddress(pool_handle handle);

  using AsyncSafeMemoryPressureNotification = void (*)();
  void SetAsyncSafeMemoryPressureNotification(
      AsyncSafeMemoryPressureNotification notification);
  bool IsAlmostFull();

  static constexpr size_t kDefaultAlmostFullPercent = 90;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  // Expected lifetime of an address space reservation, used to decide where in
  // the pool it is placed when |requested_address| is 0 or unavailable.
  enum class ReservationLifetime {
    // The reservation is expected to never be released (e.g. normal bucket
    // superpages).
    kPersistent,
    // The reservation is expected to be released (e.g. direct-mapped
    // allocations).
    kTransient,
  };

  // Reserves address space from the pool. See |ReservationLifetime| for how
  // |lifetime| affects placement.
  uintptr_t Reserve(
      pool_handle handle,
      uintptr_t requested_address,
      size_t length,
      ReservationLifetime lifetime = ReservationLifetime::kPersistent);

  // Frees address space back to the pool and decommits underlying system pages.
  void UnreserveAndDecommit(pool_handle handle,
                            uintptr_t address,
                            size_t length);
  void ResetForTesting();

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  void MarkUsed(pool_handle handle, uintptr_t address, size_t size);
  void MarkUnused(pool_handle handle, uintptr_t address, size_t size);

  static bool IsManagedByRegularPool(uintptr_t address) {
    return AddressPoolManagerBitmap::IsManagedByRegularPool(address);
  }

  static bool IsManagedByBRPPool(uintptr_t address) {
    return AddressPoolManagerBitmap::IsManagedByBRPPool(address);
  }
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  void DumpStats(AddressSpaceStatsDumper* dumper);

 private:
  friend class AddressPoolManagerForTesting;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // If we use a thread isolated pool, we need to write-protect its metadata.
  // Allow the function to get access to the pool pointer.
  friend void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption);
#endif

  constexpr AddressPoolManager() = default;
  ~AddressPoolManager() = default;

  // Populates `stats` if applicable.
  // Returns whether `stats` was populated. (They might not be, e.g.
  // if PartitionAlloc is wholly unused in this process.)
  bool GetStats(AddressSpaceStats* stats);

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // This function just exists to static_assert the layout of the private fields
  // in Pool. It is never called.
  static void AssertThreadIsolatedLayout();
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  void NotifyMemoryPressureIfNeeded(bool should_notify);

  std::atomic<AsyncSafeMemoryPressureNotification>
      async_safe_memory_pressure_notification_{nullptr};
  std::atomic<size_t> almost_full_pool_count_{0};

  class Pool {
   public:
    PA_ALWAYS_INLINE constexpr Pool();
    ~Pool() = default;

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    void Initialize(uintptr_t ptr, size_t length, int pkey = kInvalidPkey)
        PA_LOCKS_EXCLUDED(lock_);
    bool IsInitialized();
    bool Reset(std::atomic<size_t>& almost_full_pool_count)
        PA_LOCKS_EXCLUDED(lock_);

    void DecommitPages(uintptr_t address, size_t size);

    uintptr_t FindChunk(size_t size,
                        std::atomic<size_t>& almost_full_pool_count,
                        bool& should_notify) PA_LOCKS_EXCLUDED(lock_);
    uintptr_t FindChunkFromEnd(size_t size,
                               std::atomic<size_t>& almost_full_pool_count,
                               bool& should_notify) PA_LOCKS_EXCLUDED(lock_);
    bool FreeChunk(uintptr_t address,
                   size_t size,
                   std::atomic<size_t>& almost_full_pool_count)
        PA_LOCKS_EXCLUDED(lock_);

    bool TryReserveChunk(uintptr_t address,
                         size_t size,
                         std::atomic<size_t>& almost_full_pool_count,
                         bool& should_notify) PA_LOCKS_EXCLUDED(lock_);

    void GetUsedSuperPages(std::bitset<kMaxSuperPagesInPool>& used)
        PA_LOCKS_EXCLUDED(lock_);
    uintptr_t GetBaseAddress();

    void GetStats(PoolStats* stats) PA_LOCKS_EXCLUDED(lock_);

   private:
    bool UpdateAlmostFullStateLocked(
        std::atomic<size_t>& almost_full_pool_count)
        PA_EXCLUSIVE_LOCKS_REQUIRED(lock_);

    // The lock needs to be the first field in this class.
    // We write-protect the pool in the ThreadIsolated case, except that the
    // lock can be used without acquiring write-permission first (via
    // DumpStats()). So instead of protecting the whole variable, we only
    // protect the memory after the lock.
    // See the alignment of ` below.
    Lock lock_;

    // The bitset stores the allocation state of the address pool. 1 bit per
    // super-page: 1 = allocated, 0 = free.
    // Note that we should not use bitset::set, bitset::reset(size_t), or
    // bitset::test. These methods can throw std::out_of_range, which can cause
    // unexpected dependencies and lead to symbol duplication errors during
    // linking.
    std::bitset<kMaxSuperPagesInPool> alloc_bitset_ PA_GUARDED_BY(lock_);

    // An index of a bit in the bitset before which we know for sure there all
    // 1s. This is a best-effort hint in the sense that there still may be lots
    // of 1s after this index, but at least we know there is no point in
    // starting the search before it.
    size_t bit_hint_ PA_GUARDED_BY(lock_) = 0;

    // An index of a bit in the bitset at or after which we know for sure there
    // are all 1s (i.e. all bits in [high_bit_hint_, total_bits_) are 1).
    //
    // Note that |bit_hint_| and |high_bit_hint_| are maintained independently,
    // and it is valid for |bit_hint_| to be greater than |high_bit_hint_|. This
    // happens when the pool is completely full (the two all-1s ranges overlap),
    // and must be handled by all users of these hints. FreeChunk() restores
    // |bit_hint_| <= |high_bit_hint_|.
    size_t high_bit_hint_ PA_GUARDED_BY(lock_) = 0;

    size_t allocated_bits_ PA_GUARDED_BY(lock_) = 0;
    bool is_almost_full_ PA_GUARDED_BY(lock_) = false;

    size_t total_bits_ = 0;
    uintptr_t address_begin_ = 0;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    uintptr_t address_end_ = 0;
#endif
#if PA_BUILDFLAG(ENABLE_PKEYS)
    int pkey_ = kInvalidPkey;
#endif

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    friend class AddressPoolManager;
    friend void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  };

  PA_ALWAYS_INLINE Pool* GetPool(pool_handle handle) {
    PA_DCHECK(kNullPoolHandle < handle && handle <= kNumPools);
    return &pools_[handle - 1];
  }

  // Gets the stats for the pool identified by `handle`, if
  // initialized.
  void GetPoolStats(pool_handle handle, PoolStats* stats);

  // If thread isolation support is enabled, we need to write-protect the
  // isolated pool (which needs to be last). For this, we need to add padding in
  // front of the pools so that the isolated one starts on a page boundary.
  // We also skip the Lock at the beginning of the pool since it needs to be
  // used in contexts where we didn't enable write access to the pool memory.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif
  char pad_[PA_THREAD_ISOLATED_ARRAY_PAD_SZ_WITH_OFFSET(
      Pool,
      kNumPools,
      sizeof(async_safe_memory_pressure_notification_) +
          sizeof(almost_full_pool_count_) + offsetof(Pool, alloc_bitset_))] =
      {};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  std::array<Pool, kNumPools> pools_;

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  constinit static AddressPoolManager singleton_;
};

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
PA_ALWAYS_INLINE constexpr AddressPoolManager::Pool::Pool() = default;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_ADDRESS_POOL_MANAGER_H_
