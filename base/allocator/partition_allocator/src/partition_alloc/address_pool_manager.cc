// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/address_pool_manager.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>

#include "partition_alloc/address_space_stats.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/thread_isolation/alignment.h"

#if PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include <sys/mman.h>
#endif

namespace partition_alloc::internal {

PA_CONSTINIT AddressPoolManager AddressPoolManager::singleton_;

// static
AddressPoolManager& AddressPoolManager::GetInstance() {
  return singleton_;
}

namespace {
// Allocations are all performed on behalf of PartitionAlloc.
constexpr PageTag kPageTag = PageTag::kPartitionAlloc;

}  // namespace

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace {

// This will crash if the range cannot be decommitted.
void DecommitPages(uintptr_t address, size_t size) {
  // Callers rely on the pages being zero-initialized when recommitting them.
  // |DecommitSystemPages| doesn't guarantee this on all operating systems, in
  // particular on macOS, but |DecommitAndZeroSystemPages| does.
  DecommitAndZeroSystemPages(address, size, kPageTag);
}

}  // namespace

void AddressPoolManager::Add(pool_handle handle, uintptr_t ptr, size_t length) {
  PA_DCHECK(!(ptr & kSuperPageOffsetMask));
  PA_DCHECK(!((ptr + length) & kSuperPageOffsetMask));
  PA_CHECK(handle > 0 && handle <= std::size(pools_));

  Pool* pool = GetPool(handle);
  PA_CHECK(!pool->IsInitialized());
  pool->Initialize(ptr, length);
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
  for (size_t i = 0; i < std::size(pools_); ++i) {
    pools_[i].Reset();
  }
}

void AddressPoolManager::Remove(pool_handle handle) {
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  pool->Reset();
}

uintptr_t AddressPoolManager::Reserve(pool_handle handle,
                                      uintptr_t requested_address,
                                      size_t length) {
  Pool* pool = GetPool(handle);
  if (!requested_address) {
    return pool->FindChunk(length);
  }
  const bool is_available = pool->TryReserveChunk(requested_address, length);
  if (is_available) {
    return requested_address;
  }
  return pool->FindChunk(length);
}

void AddressPoolManager::UnreserveAndDecommit(pool_handle handle,
                                              uintptr_t address,
                                              size_t length) {
  PA_DCHECK(kNullPoolHandle < handle && handle <= kNumPools);
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  DecommitPages(address, length);
  pool->FreeChunk(address, length);
}

void AddressPoolManager::Pool::Initialize(uintptr_t ptr, size_t length) {
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

  ScopedGuard scoped_lock(lock_);
  alloc_bitset_.reset();
  bit_hint_ = 0;
}

bool AddressPoolManager::Pool::IsInitialized() {
  return address_begin_ != 0;
}

void AddressPoolManager::Pool::Reset() {
  address_begin_ = 0;
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

uintptr_t AddressPoolManager::Pool::FindChunk(size_t requested_size) {
  ScopedGuard scoped_lock(lock_);

  PA_DCHECK(!(requested_size & kSuperPageOffsetMask));
  const size_t need_bits = requested_size >> kSuperPageShift;

  // Use first-fit policy to find an available chunk from free chunks. Start
  // from |bit_hint_|, because we know there are no free chunks before.
  size_t beg_bit = bit_hint_;
  size_t curr_bit = bit_hint_;
  while (true) {
    // |end_bit| points 1 past the last bit that needs to be 0. If it goes past
    // |total_bits_|, return |nullptr| to signal no free chunk was found.
    size_t end_bit = beg_bit + need_bits;
    if (end_bit > total_bits_) {
      return 0;
    }

    bool found = true;
    for (; curr_bit < end_bit; ++curr_bit) {
      if (alloc_bitset_.test(curr_bit)) {
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
        PA_DCHECK(!alloc_bitset_.test(i));
        alloc_bitset_.set(i);
      }
      if (bit_hint_ == beg_bit) {
        bit_hint_ = end_bit;
      }
      uintptr_t address = address_begin_ + beg_bit * kSuperPageSize;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      PA_DCHECK(address + requested_size <= address_end_);
#endif
      return address;
    }
  }

  PA_NOTREACHED();
}

bool AddressPoolManager::Pool::TryReserveChunk(uintptr_t address,
                                               size_t requested_size) {
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
    if (alloc_bitset_.test(i)) {
      return false;
    }
  }
  // Otherwise, set the bits.
  for (size_t i = begin_bit; i < end_bit; ++i) {
    alloc_bitset_.set(i);
  }
  return true;
}

void AddressPoolManager::Pool::FreeChunk(uintptr_t address, size_t free_size) {
  ScopedGuard scoped_lock(lock_);

  PA_DCHECK(!(address & kSuperPageOffsetMask));
  PA_DCHECK(!(free_size & kSuperPageOffsetMask));

  PA_DCHECK(address_begin_ <= address);
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(address + free_size <= address_end_);
#endif

  const size_t beg_bit = (address - address_begin_) / kSuperPageSize;
  const size_t end_bit = beg_bit + free_size / kSuperPageSize;
  for (size_t i = beg_bit; i < end_bit; ++i) {
    PA_DCHECK(alloc_bitset_.test(i));
    alloc_bitset_.reset(i);
  }
  bit_hint_ = std::min(bit_hint_, beg_bit);
}

void AddressPoolManager::Pool::GetStats(PoolStats* stats) {
  std::bitset<kMaxSuperPagesInPool> pages;
  size_t i;
  {
    ScopedGuard scoped_lock(lock_);
    pages = alloc_bitset_;
    i = bit_hint_;
  }

  stats->usage = pages.count();

  size_t largest_run = 0;
  size_t current_run = 0;
  for (; i < total_bits_; ++i) {
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
    PA_DCHECK(!bitmap.test(i));
    bitmap.set(i);
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
    PA_DCHECK(bitmap.test(i));
    bitmap.reset(i);
  }
}

uintptr_t AddressPoolManager::Reserve(pool_handle handle,
                                      uintptr_t requested_address,
                                      size_t length) {
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
