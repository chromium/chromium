// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/address_pool_manager.h"

#include <atomic>
#include <cstdint>
#include <limits>

#include "partition_alloc/address_space_stats.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/algorithm.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_public_constants.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/thread_isolation/alignment.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include <sys/mman.h>
#endif

namespace partition_alloc::internal {

constinit AddressPoolManager AddressPoolManager::singleton_;

// static
AddressPoolManager& AddressPoolManager::GetInstance() {
  return singleton_;
}

namespace {
// Allocations are all performed on behalf of PartitionAlloc.
constexpr PageTag kPageTag = PageTag::kPartitionAlloc;

}  // namespace

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

void AddressPoolManager::SetAsyncSafeMemoryPressureNotification(
    AsyncSafeMemoryPressureNotification notification) {
  async_safe_memory_pressure_notification_.store(notification,
                                                 std::memory_order_release);
}

void AddressPoolManager::NotifyMemoryPressureIfNeeded(bool should_notify) {
  if (!should_notify) [[likely]] {
    return;
  }
  if (auto* notification = async_safe_memory_pressure_notification_.load(
          std::memory_order_acquire)) {
    notification();
  }
}

void AddressPoolManager::Add(pool_handle handle,
                             uintptr_t ptr,
                             size_t length,
                             int pkey) {
  PA_DCHECK(!(ptr & kSuperPageOffsetMask));
  PA_DCHECK(!((ptr + length) & kSuperPageOffsetMask));
  PA_CHECK(handle > 0 && handle <= std::size(pools_));

  Pool* pool = GetPool(handle);
  PA_CHECK(!pool->IsInitialized());
  pool->Initialize(ptr, length, pkey);
}

void AddressPoolManager::GetPoolUsedSuperPages(
    pool_handle handle,
    std::bitset<kMaxSuperPagesInPool>& used) {
  Pool* pool = GetPool(handle);
  if (!pool) {
    return;
  }

  pool->GetUsedSuperPages(used);
}

uintptr_t AddressPoolManager::GetPoolBaseAddress(pool_handle handle) {
  Pool* pool = GetPool(handle);
  if (!pool) {
    return 0;
  }

  return pool->GetBaseAddress();
}

void AddressPoolManager::ResetForTesting() {
  for (auto& pool : pools_) {
    pool.Reset(almost_full_pool_count_);
  }
  PA_DCHECK(almost_full_pool_count_.load(std::memory_order_relaxed) == 0);
  async_safe_memory_pressure_notification_.store(nullptr,
                                                 std::memory_order_release);
}

void AddressPoolManager::Remove(pool_handle handle) {
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  NotifyMemoryPressureIfNeeded(pool->Reset(almost_full_pool_count_));
}

uintptr_t AddressPoolManager::Reserve(pool_handle handle,
                                      uintptr_t requested_address,
                                      size_t length,
                                      ReservationLifetime lifetime) {
  Pool* pool = GetPool(handle);
  bool should_notify = false;
  uintptr_t address = 0;
  // Reservations can be persistent or not. If a reservation is persistent, we
  // try to pack it from the bottom of the address space, otherwise from the
  // top. This is intended to minimize fragmentation over time:
  // - Persistent: Since these are never released, they effectively end up being
  //   a single, continuous region with no holes.
  // - Non-persistent: The main goal is to keep holes that are as large as
  //   possible, to prevent allocation failures due to GigaCage
  //   fragmentation. Allocating from the top at least moves these away from the
  //   persistent allocations. The current policy (first-fit) is not necessarily
  //   the best one, but at least it prevents the interleaving of persistent and
  //   non-persistent allocations from creating permanent fragmentation.
  if (requested_address &&
      pool->TryReserveChunk(requested_address, length, almost_full_pool_count_,
                            should_notify)) {
    address = requested_address;
  } else if (lifetime == ReservationLifetime::kTransient) {
    address =
        pool->FindChunkFromEnd(length, almost_full_pool_count_, should_notify);
  } else {
    address = pool->FindChunk(length, almost_full_pool_count_, should_notify);
  }
  NotifyMemoryPressureIfNeeded(should_notify);
  return address;
}

void AddressPoolManager::UnreserveAndDecommit(pool_handle handle,
                                              uintptr_t address,
                                              size_t length) {
  PA_DCHECK(kNullPoolHandle < handle && handle <= kNumPools);
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  pool->DecommitPages(address, length);
  const bool should_notify =
      pool->FreeChunk(address, length, almost_full_pool_count_);
  NotifyMemoryPressureIfNeeded(should_notify);
}

void AddressPoolManager::Pool::Initialize(uintptr_t ptr,
                                          size_t length,
                                          int pkey) {
  PA_CHECK(ptr != 0);
  PA_CHECK(!(ptr & kSuperPageOffsetMask));
  PA_CHECK(!(length & kSuperPageOffsetMask));
  address_begin_ = ptr;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  address_end_ = ptr + length;
  PA_DCHECK(address_begin_ < address_end_);
#endif

  total_bits_ = length / kSuperPageSize;
  PA_CHECK(total_bits_ <= kMaxSuperPagesInPool);

#if PA_BUILDFLAG(ENABLE_PKEYS)
  if (pkey != kInvalidPkey) {
    pkey_ = pkey;
    SetSystemPagesAccess(ptr, length,
                         PageAccessibilityConfiguration(
                             PageAccessibilityConfiguration::kInaccessible,
                             ThreadIsolationOption(pkey_)));
  }
#else
  PA_CHECK(pkey == kInvalidPkey);
#endif

  ScopedGuard scoped_lock(lock_);
  alloc_bitset_.reset();
  bit_hint_ = 0;
  high_bit_hint_ = total_bits_;
  allocated_bits_ = 0;
  is_almost_full_ = false;
}

void AddressPoolManager::Pool::DecommitPages(uintptr_t address, size_t size) {
  // Callers rely on the pages being zero-initialized when recommitting them.
  // |DecommitSystemPages| doesn't guarantee this on all operating systems, in
  // particular on macOS, but |DecommitAndZeroSystemPages| does.
  PA_CHECK(DecommitAndZeroSystemPages(address, size, kPageTag));
#if PA_BUILDFLAG(ENABLE_PKEYS)
  if (pkey_ != kInvalidPkey) {
    // DecommitAndZeroSystemPages replaces the mapping with fresh anonymous
    // pages via mmap(MAP_FIXED, PROT_NONE), which resets the memory protection
    // key to the default key (0). Re-tag the pages with the pool's pkey.
    SetSystemPagesAccess(address, size,
                         PageAccessibilityConfiguration(
                             PageAccessibilityConfiguration::kInaccessible,
                             ThreadIsolationOption(pkey_)));
  }
#endif
}

bool AddressPoolManager::Pool::IsInitialized() {
  return address_begin_ != 0;
}

bool AddressPoolManager::Pool::Reset(
    std::atomic<size_t>& almost_full_pool_count) {
  ScopedGuard scoped_lock(lock_);
  address_begin_ = 0;
  total_bits_ = 0;
#if PA_BUILDFLAG(ENABLE_PKEYS)
  pkey_ = kInvalidPkey;
#endif
  alloc_bitset_.reset();
  bit_hint_ = 0;
  high_bit_hint_ = 0;
  allocated_bits_ = 0;
  if (!is_almost_full_) {
    return false;
  }
  is_almost_full_ = false;
  const size_t prev_count =
      almost_full_pool_count.fetch_sub(1, std::memory_order_relaxed);
  PA_DCHECK(prev_count > 0);
  return prev_count == 1;
}

bool AddressPoolManager::Pool::UpdateAlmostFullStateLocked(
    std::atomic<size_t>& almost_full_pool_count) {
  if (total_bits_ == 0) {
    return false;
  }
  if (!is_almost_full_) {
    if (allocated_bits_ * 100 >= total_bits_ * kDefaultAlmostFullPercent) {
      is_almost_full_ = true;
      return almost_full_pool_count.fetch_add(1, std::memory_order_relaxed) ==
             0;
    }
  } else {
    if (allocated_bits_ * 100 <=
        total_bits_ * (kDefaultAlmostFullPercent - 5)) {
      is_almost_full_ = false;
      const size_t prev_count =
          almost_full_pool_count.fetch_sub(1, std::memory_order_relaxed);
      PA_DCHECK(prev_count > 0);
      return prev_count == 1;
    }
  }
  return false;
}

void AddressPoolManager::Pool::GetUsedSuperPages(
    std::bitset<kMaxSuperPagesInPool>& used) {
  ScopedGuard scoped_lock(lock_);

  PA_DCHECK(IsInitialized());
  used = alloc_bitset_;
}

uintptr_t AddressPoolManager::Pool::GetBaseAddress() {
  PA_DCHECK(IsInitialized());
  return address_begin_;
}

uintptr_t AddressPoolManager::Pool::FindChunk(
    size_t requested_size,
    std::atomic<size_t>& almost_full_pool_count,
    bool& should_notify) {
  ScopedGuard scoped_lock(lock_);

  PA_DCHECK(!(requested_size & kSuperPageOffsetMask));
  const size_t need_bits = requested_size >> kSuperPageShift;

  // Use first-fit policy to find an available chunk from free chunks. Start
  // from |bit_hint_|, because we know there are no free chunks before.
  size_t beg_bit = bit_hint_;
  size_t curr_bit = bit_hint_;
  while (true) {
    // The candidate chunk is [beg_bit, beg_bit + need_bits). All bits at or
    // after |high_bit_hint_| are known to be 1, so if the chunk doesn't fit
    // below it, return 0 to signal no free chunk was found. Note that
    // |beg_bit| may be past |high_bit_hint_| when the pool is full.
    if (beg_bit > high_bit_hint_ || high_bit_hint_ - beg_bit < need_bits) {
      should_notify = false;
      return 0;
    }
    size_t end_bit = beg_bit + need_bits;

    bool found = true;
    for (; curr_bit < end_bit; ++curr_bit) {
      if (alloc_bitset_[curr_bit]) {
        // The bit was set, so this chunk isn't entirely free. Set |found=false|
        // to ensure the outer loop continues. However, continue the inner loop
        // to set |beg_bit| just past the last set bit in the investigated
        // chunk. |curr_bit| is advanced all the way to |end_bit| to prevent the
        // next outer loop pass from checking the same bits.
        beg_bit = curr_bit + 1;
        found = false;
        if (bit_hint_ == curr_bit) {
          ++bit_hint_;
        }
      }
    }

    // An entire [beg_bit;end_bit) region of 0s was found. Fill them with 1s (to
    // mark as allocated) and return the allocated address.
    if (found) {
      for (size_t i = beg_bit; i < end_bit; ++i) {
        PA_DCHECK(!alloc_bitset_[i]);
        alloc_bitset_[i] = true;
      }
      allocated_bits_ += need_bits;
      if (bit_hint_ == beg_bit) {
        bit_hint_ = end_bit;
      }
      if (high_bit_hint_ == end_bit) {
        high_bit_hint_ = beg_bit;
      }
      should_notify = UpdateAlmostFullStateLocked(almost_full_pool_count);
      uintptr_t address = address_begin_ + beg_bit * kSuperPageSize;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      PA_DCHECK(address + requested_size <= address_end_);
#endif
      return address;
    }
  }

  PA_NOTREACHED();
}

uintptr_t AddressPoolManager::Pool::FindChunkFromEnd(
    size_t requested_size,
    std::atomic<size_t>& almost_full_pool_count,
    bool& should_notify) {
  ScopedGuard scoped_lock(lock_);

  PA_DCHECK(!(requested_size & kSuperPageOffsetMask));
  const size_t need_bits = requested_size >> kSuperPageShift;

  // Use first-fit policy from the high end to find an available chunk from free
  // chunks. Start from |high_bit_hint_|, because we know there are no free
  // chunks at or after it.
  size_t end_bit = high_bit_hint_;
  size_t curr_bit = high_bit_hint_;
  while (true) {
    if (end_bit < bit_hint_ || end_bit - bit_hint_ < need_bits) {
      should_notify = false;
      return 0;
    }
    size_t beg_bit = end_bit - need_bits;

    bool found = true;
    while (curr_bit > beg_bit) {
      --curr_bit;
      if (alloc_bitset_[curr_bit]) {
        // The bit was set, so this chunk isn't entirely free. Set |found=false|
        // to ensure the outer loop continues. However, continue the inner loop
        // to set |end_bit| to the lowest set bit in the investigated chunk.
        // |curr_bit| is decremented all the way to |beg_bit| to prevent the
        // next outer loop pass from checking the same bits.
        end_bit = curr_bit;
        found = false;
        if (high_bit_hint_ == curr_bit + 1) {
          --high_bit_hint_;
        }
      }
    }

    // An entire [beg_bit;end_bit) region of 0s was found. Fill them with 1s (to
    // mark as allocated) and return the allocated address.
    if (found) {
      for (size_t i = beg_bit; i < end_bit; ++i) {
        PA_DCHECK(!alloc_bitset_[i]);
        alloc_bitset_[i] = true;
      }
      allocated_bits_ += need_bits;
      if (high_bit_hint_ == end_bit) {
        high_bit_hint_ = beg_bit;
      }
      if (bit_hint_ == beg_bit) {
        bit_hint_ = end_bit;
      }
      should_notify = UpdateAlmostFullStateLocked(almost_full_pool_count);
      uintptr_t address = address_begin_ + beg_bit * kSuperPageSize;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      PA_DCHECK(address + requested_size <= address_end_);
#endif
      return address;
    }
  }

  PA_NOTREACHED();
}

bool AddressPoolManager::Pool::TryReserveChunk(
    uintptr_t address,
    size_t requested_size,
    std::atomic<size_t>& almost_full_pool_count,
    bool& should_notify) {
  ScopedGuard scoped_lock(lock_);
  PA_DCHECK(!(address & kSuperPageOffsetMask));
  PA_DCHECK(!(requested_size & kSuperPageOffsetMask));
  const size_t begin_bit = (address - address_begin_) / kSuperPageSize;
  const size_t need_bits = requested_size / kSuperPageSize;
  const size_t end_bit = begin_bit + need_bits;
  // Check that requested address is not too high.
  if (end_bit > total_bits_) {
    return false;
  }
  // Check if any bit of the requested region is set already.
  for (size_t i = begin_bit; i < end_bit; ++i) {
    if (alloc_bitset_[i]) {
      return false;
    }
  }
  // Otherwise, set the bits.
  for (size_t i = begin_bit; i < end_bit; ++i) {
    alloc_bitset_[i] = true;
  }
  allocated_bits_ += need_bits;
  should_notify = UpdateAlmostFullStateLocked(almost_full_pool_count);
  return true;
}

bool AddressPoolManager::Pool::FreeChunk(
    uintptr_t address,
    size_t free_size,
    std::atomic<size_t>& almost_full_pool_count) {
  ScopedGuard scoped_lock(lock_);

  PA_DCHECK(!(address & kSuperPageOffsetMask));
  PA_DCHECK(!(free_size & kSuperPageOffsetMask));

  PA_DCHECK(address_begin_ <= address);
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(address + free_size <= address_end_);
#endif

  const size_t beg_bit = (address - address_begin_) / kSuperPageSize;
  const size_t need_bits = free_size / kSuperPageSize;
  const size_t end_bit = beg_bit + need_bits;
  for (size_t i = beg_bit; i < end_bit; ++i) {
    PA_DCHECK(alloc_bitset_[i]);
    alloc_bitset_[i] = false;
  }
  PA_DCHECK(allocated_bits_ >= need_bits);
  allocated_bits_ -= need_bits;
  bit_hint_ = std::min(bit_hint_, beg_bit);
  high_bit_hint_ = std::max(high_bit_hint_, end_bit);
  return UpdateAlmostFullStateLocked(almost_full_pool_count);
}

void AddressPoolManager::Pool::GetStats(PoolStats* stats) {
  std::bitset<kMaxSuperPagesInPool> pages;
  size_t i;
  size_t end;
  {
    ScopedGuard scoped_lock(lock_);
    pages = alloc_bitset_;
    i = bit_hint_;
    end = high_bit_hint_;
    PA_DCHECK(pages.count() == allocated_bits_);
    stats->usage = allocated_bits_;
  }

  size_t largest_run = 0;
  size_t current_run = 0;
  for (; i < end; ++i) {
    if (!pages[i]) {
      current_run += 1;
      continue;
    } else if (current_run > largest_run) {
      largest_run = current_run;
    }
    current_run = 0;
  }

  // Fell out of the loop with last bit being zero. Check once more.
  if (current_run > largest_run) {
    largest_run = current_run;
  }
  stats->largest_available_reservation = largest_run;
}

void AddressPoolManager::GetPoolStats(const pool_handle handle,
                                      PoolStats* stats) {
  Pool* pool = GetPool(handle);
  if (!pool->IsInitialized()) {
    return;
  }
  pool->GetStats(stats);
}

bool AddressPoolManager::GetStats(AddressSpaceStats* stats) {
  // Get 64-bit pool stats.
  GetPoolStats(kRegularPoolHandle, &stats->regular_pool_stats);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  GetPoolStats(kBRPPoolHandle, &stats->brp_pool_stats);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (IsConfigurablePoolAvailable()) {
    GetPoolStats(kConfigurablePoolHandle, &stats->configurable_pool_stats);
  }
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  GetPoolStats(kThreadIsolatedPoolHandle, &stats->thread_isolated_pool_stats);
#endif
  return true;
}

bool AddressPoolManager::IsAlmostFull() {
  return almost_full_pool_count_.load(std::memory_order_relaxed) > 0;
}

#else  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

static_assert(
    kSuperPageSize % AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap ==
        0,
    "kSuperPageSize must be a multiple of kBytesPer1BitOfBRPPoolBitmap.");
static_assert(
    kSuperPageSize / AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap > 0,
    "kSuperPageSize must be larger than kBytesPer1BitOfBRPPoolBitmap.");
static_assert(AddressPoolManagerBitmap::kGuardBitsOfBRPPoolBitmap >=
                  AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap,
              "kGuardBitsOfBRPPoolBitmap must be larger than or equal to "
              "kGuardOffsetOfBRPPoolBitmap.");

template <size_t bitsize>
void SetBitmap(std::bitset<bitsize>& bitmap,
               size_t start_bit,
               size_t bit_length) {
  const size_t end_bit = start_bit + bit_length;
  PA_DCHECK(start_bit <= bitsize);
  PA_DCHECK(end_bit <= bitsize);

  for (size_t i = start_bit; i < end_bit; ++i) {
    PA_DCHECK(!bitmap[i]);
    bitmap[i] = true;
  }
}

template <size_t bitsize>
void ResetBitmap(std::bitset<bitsize>& bitmap,
                 size_t start_bit,
                 size_t bit_length) {
  const size_t end_bit = start_bit + bit_length;
  PA_DCHECK(start_bit <= bitsize);
  PA_DCHECK(end_bit <= bitsize);

  for (size_t i = start_bit; i < end_bit; ++i) {
    PA_DCHECK(bitmap[i]);
    bitmap[i] = false;
  }
}

uintptr_t AddressPoolManager::Reserve(pool_handle handle,
                                      uintptr_t requested_address,
                                      size_t length,
                                      ReservationLifetime lifetime) {
  PA_DCHECK(!(length & DirectMapAllocationGranularityOffsetMask()));
  uintptr_t address =
      AllocPages(requested_address, length, kSuperPageSize,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 kPageTag);
  return address;
}

void AddressPoolManager::UnreserveAndDecommit(pool_handle handle,
                                              uintptr_t address,
                                              size_t length) {
  PA_DCHECK(!(address & kSuperPageOffsetMask));
  PA_DCHECK(!(length & DirectMapAllocationGranularityOffsetMask()));
  FreePages(address, length);
}

void AddressPoolManager::MarkUsed(pool_handle handle,
                                  uintptr_t address,
                                  size_t length) {
  ScopedGuard scoped_lock(AddressPoolManagerBitmap::GetLock());
  // When ENABLE_BACKUP_REF_PTR_SUPPORT is off, BRP pool isn't used.
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (handle == kBRPPoolHandle) {
    PA_DCHECK(
        (length % AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap) == 0);

    // Make IsManagedByBRPPoolPool() return false when an address inside the
    // first or the last PartitionPageSize()-bytes block is given:
    //
    //          ------+---+---------------+---+----
    // memory   ..... | B | managed by PA | B | ...
    // regions  ------+---+---------------+---+----
    //
    // B: PartitionPageSize()-bytes block. This is used internally by the
    // allocator and is not available for callers.
    //
    // This is required to avoid crash caused by the following code:
    //   {
    //     // Assume this allocation happens outside of PartitionAlloc.
    //     raw_ptr<T> ptr = new T[20];
    //     for (size_t i = 0; i < 20; i ++) { ptr++; }
    //     // |ptr| may point to an address inside 'B'.
    //   }
    //
    // Suppose that |ptr| points to an address inside B after the loop. If
    // IsManagedByBRPPoolPool(ptr) were to return true, ~raw_ptr<T>() would
    // crash, since the memory is not allocated by PartitionAlloc.
    SetBitmap(AddressPoolManagerBitmap::brp_pool_bits_,
              (address >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) +
                  AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap,
              (length >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) -
                  AddressPoolManagerBitmap::kGuardBitsOfBRPPoolBitmap);
  } else
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  {
    PA_DCHECK(handle == kRegularPoolHandle);
    PA_DCHECK(
        (length % AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap) ==
        0);
    SetBitmap(AddressPoolManagerBitmap::regular_pool_bits_,
              address >> AddressPoolManagerBitmap::kBitShiftOfRegularPoolBitmap,
              length >> AddressPoolManagerBitmap::kBitShiftOfRegularPoolBitmap);
  }
}

void AddressPoolManager::MarkUnused(pool_handle handle,
                                    uintptr_t address,
                                    size_t length) {
  // Address regions allocated for normal buckets are never released, so this
  // function can only be called for direct map. However, do not DCHECK on
  // IsManagedByDirectMap(address), because many tests test this function using
  // small allocations.

  ScopedGuard scoped_lock(AddressPoolManagerBitmap::GetLock());
  // When ENABLE_BACKUP_REF_PTR_SUPPORT is off, BRP pool isn't used.
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (handle == kBRPPoolHandle) {
    PA_DCHECK(
        (length % AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap) == 0);

    // Make IsManagedByBRPPoolPool() return false when an address inside the
    // first or the last PartitionPageSize()-bytes block is given.
    // (See MarkUsed comment)
    ResetBitmap(
        AddressPoolManagerBitmap::brp_pool_bits_,
        (address >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) +
            AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap,
        (length >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) -
            AddressPoolManagerBitmap::kGuardBitsOfBRPPoolBitmap);
  } else
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  {
    PA_DCHECK(handle == kRegularPoolHandle);
    PA_DCHECK(
        (length % AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap) ==
        0);
    ResetBitmap(
        AddressPoolManagerBitmap::regular_pool_bits_,
        address >> AddressPoolManagerBitmap::kBitShiftOfRegularPoolBitmap,
        length >> AddressPoolManagerBitmap::kBitShiftOfRegularPoolBitmap);
  }
}

void AddressPoolManager::ResetForTesting() {
  ScopedGuard guard(AddressPoolManagerBitmap::GetLock());
  AddressPoolManagerBitmap::regular_pool_bits_.reset();
  AddressPoolManagerBitmap::brp_pool_bits_.reset();
}

namespace {

// Counts super pages in use represented by `bitmap`.
template <size_t bitsize>
size_t CountUsedSuperPages(const std::bitset<bitsize>& bitmap,
                           const size_t bits_per_super_page) {
  size_t count = 0;
  size_t bit_index = 0;

  // Stride over super pages.
  for (size_t super_page_index = 0; bit_index < bitsize; ++super_page_index) {
    // Stride over the bits comprising the super page.
    for (bit_index = super_page_index * bits_per_super_page;
         bit_index < (super_page_index + 1) * bits_per_super_page &&
         bit_index < bitsize;
         ++bit_index) {
      if (bitmap[bit_index]) {
        count += 1;
        // Move on to the next super page.
        break;
      }
    }
  }
  return count;
}

}  // namespace

bool AddressPoolManager::GetStats(AddressSpaceStats* stats) {
  std::bitset<AddressPoolManagerBitmap::kRegularPoolBits> regular_pool_bits;
  std::bitset<AddressPoolManagerBitmap::kBRPPoolBits> brp_pool_bits;
  {
    ScopedGuard scoped_lock(AddressPoolManagerBitmap::GetLock());
    regular_pool_bits = AddressPoolManagerBitmap::regular_pool_bits_;
    brp_pool_bits = AddressPoolManagerBitmap::brp_pool_bits_;
  }  // scoped_lock

  // Pool usage is read out from the address pool bitmaps.
  // The output stats are sized in super pages, so we interpret
  // the bitmaps into super page usage.
  static_assert(
      kSuperPageSize %
              AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap ==
          0,
      "information loss when calculating metrics");
  constexpr size_t kRegularPoolBitsPerSuperPage =
      kSuperPageSize /
      AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap;

  // Get 32-bit pool usage.
  stats->regular_pool_stats.usage =
      CountUsedSuperPages(regular_pool_bits, kRegularPoolBitsPerSuperPage);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  static_assert(
      kSuperPageSize % AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap ==
          0,
      "information loss when calculating metrics");
  constexpr size_t kBRPPoolBitsPerSuperPage =
      kSuperPageSize / AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap;
  stats->brp_pool_stats.usage =
      CountUsedSuperPages(brp_pool_bits, kBRPPoolBitsPerSuperPage);

  // Get blocklist size.
  for (const auto& blocked :
       AddressPoolManagerBitmap::brp_forbidden_super_page_map_) {
    if (blocked.load(std::memory_order_relaxed)) {
      stats->blocklist_size += 1;
    }
  }

  // Count failures in finding non-blocklisted addresses.
  stats->blocklist_hit_count =
      AddressPoolManagerBitmap::blocklist_hit_count_.load(
          std::memory_order_relaxed);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  return true;
}

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

void AddressPoolManager::DumpStats(AddressSpaceStatsDumper* dumper) {
  AddressSpaceStats stats{};
  if (GetStats(&stats)) {
    dumper->DumpStats(&stats);
  }
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
// This function just exists to static_assert the layout of the private fields
// in Pool. It is never called.
void AddressPoolManager::AssertThreadIsolatedLayout() {
  constexpr size_t last_pool_offset =
      offsetof(AddressPoolManager, pools_) + sizeof(Pool) * (kNumPools - 1);
  constexpr size_t alloc_bitset_offset =
      last_pool_offset + offsetof(Pool, alloc_bitset_);
  static_assert(alloc_bitset_offset % PA_THREAD_ISOLATED_ALIGN_SZ == 0);
  static_assert(sizeof(AddressPoolManager) % PA_THREAD_ISOLATED_ALIGN_SZ == 0);
}
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

}  // namespace partition_alloc::internal
