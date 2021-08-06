// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager.h"

#if defined(OS_APPLE)
#include <sys/mman.h>
#endif

#include <algorithm>
#include <limits>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/cxx17_backports.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"

namespace base {
namespace internal {

namespace {

base::LazyInstance<AddressPoolManager>::Leaky g_address_pool_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
AddressPoolManager* AddressPoolManager::GetInstance() {
  return g_address_pool_manager.Pointer();
}

#if defined(PA_HAS_64_BITS_POINTERS)

namespace {

// This will crash if the range cannot be decommitted.
void DecommitPages(void* address, size_t size) {
#if defined(OS_APPLE)
  // MAP_FIXED replaces an existing mapping with a new one, when the address is
  // already part of a mapping. Since newly-created mappings are guaranteed to
  // be zero-filled, this has the desired effect. It is only required on macOS,
  // as on other operating systems, |DecommitSystemPages()| provides the same
  // behavior.
  void* ptr = mmap(address, size, PROT_NONE,
                   MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  PA_CHECK(ptr == address);
#else
  static_assert(DecommittedMemoryIsAlwaysZeroed(), "");
  DecommitSystemPages(address, size, PageUpdatePermissions);
#endif
}

}  // namespace

pool_handle AddressPoolManager::Add(uintptr_t ptr, size_t length) {
  PA_DCHECK(!(ptr & kSuperPageOffsetMask));
  PA_DCHECK(!((ptr + length) & kSuperPageOffsetMask));

  for (pool_handle i = 0; i < base::size(pools_); ++i) {
    if (!pools_[i].IsInitialized()) {
      pools_[i].Initialize(ptr, length);
      return i + 1;
    }
  }
  NOTREACHED();
  return 0;
}

void AddressPoolManager::GetPoolUsedSuperPages(
    pool_handle handle,
    std::bitset<kMaxSuperPages>& used) {
  Pool* pool = GetPool(handle);
  if (!pool)
    return;

  pool->GetUsedSuperPages(used);
}

uintptr_t AddressPoolManager::GetPoolBaseAddress(pool_handle handle) {
  Pool* pool = GetPool(handle);
  if (!pool)
    return 0;

  return pool->GetBaseAddress();
}

void AddressPoolManager::ResetForTesting() {
  for (pool_handle i = 0; i < base::size(pools_); ++i)
    pools_[i].Reset();
}

void AddressPoolManager::Remove(pool_handle handle) {
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  pool->Reset();
}

char* AddressPoolManager::Reserve(pool_handle handle,
                                  void* requested_address,
                                  size_t length) {
  Pool* pool = GetPool(handle);
  if (!requested_address)
    return reinterpret_cast<char*>(pool->FindChunk(length));
  const bool is_available = pool->TryReserveChunk(
      reinterpret_cast<uintptr_t>(requested_address), length);
  if (is_available)
    return static_cast<char*>(requested_address);
  return reinterpret_cast<char*>(pool->FindChunk(length));
}

void AddressPoolManager::UnreserveAndDecommit(pool_handle handle,
                                              void* ptr,
                                              size_t length) {
  PA_DCHECK(0 < handle && handle <= kNumPools);
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  DecommitPages(ptr, length);
  pool->FreeChunk(reinterpret_cast<uintptr_t>(ptr), length);
}

void AddressPoolManager::Pool::Initialize(uintptr_t ptr, size_t length) {
  PA_CHECK(ptr != 0);
  PA_CHECK(!(ptr & kSuperPageOffsetMask));
  PA_CHECK(!(length & kSuperPageOffsetMask));
  address_begin_ = ptr;
#if DCHECK_IS_ON()
  address_end_ = ptr + length;
  PA_DCHECK(address_begin_ < address_end_);
#endif

  total_bits_ = length / kSuperPageSize;
  PA_CHECK(total_bits_ <= kMaxSuperPages);

  base::AutoLock scoped_lock(lock_);
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
    std::bitset<kMaxSuperPages>& used) {
  base::AutoLock scoped_lock(lock_);

  PA_DCHECK(IsInitialized());
  used = alloc_bitset_;
}

uintptr_t AddressPoolManager::Pool::GetBaseAddress() {
  PA_DCHECK(IsInitialized());
  return address_begin_;
}

uintptr_t AddressPoolManager::Pool::FindChunk(size_t requested_size) {
  base::AutoLock scoped_lock(lock_);

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
    if (end_bit > total_bits_)
      return 0;

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
        if (bit_hint_ == curr_bit)
          ++bit_hint_;
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
#if DCHECK_IS_ON()
      PA_DCHECK(address + requested_size <= address_end_);
#endif
      return address;
    }
  }

  NOTREACHED();
  return 0;
}

bool AddressPoolManager::Pool::TryReserveChunk(uintptr_t address,
                                               size_t requested_size) {
  base::AutoLock scoped_lock(lock_);
  PA_DCHECK(!(address & kSuperPageOffsetMask));
  PA_DCHECK(!(requested_size & kSuperPageOffsetMask));
  const size_t begin_bit = (address - address_begin_) / kSuperPageSize;
  const size_t need_bits = requested_size / kSuperPageSize;
  const size_t end_bit = begin_bit + need_bits;
  // Check that requested address is not too high.
  if (end_bit > total_bits_)
    return false;
  // Check if any bit of the requested region is set already.
  for (size_t i = begin_bit; i < end_bit; ++i) {
    if (alloc_bitset_.test(i))
      return false;
  }
  // Otherwise, set the bits.
  for (size_t i = begin_bit; i < end_bit; ++i) {
    alloc_bitset_.set(i);
  }
  return true;
}

void AddressPoolManager::Pool::FreeChunk(uintptr_t address, size_t free_size) {
  base::AutoLock scoped_lock(lock_);

  PA_DCHECK(!(address & kSuperPageOffsetMask));
  PA_DCHECK(!(free_size & kSuperPageOffsetMask));

  PA_DCHECK(address_begin_ <= address);
#if DCHECK_IS_ON()
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

AddressPoolManager::Pool::Pool() = default;
AddressPoolManager::Pool::~Pool() = default;

#else  // defined(PA_HAS_64_BITS_POINTERS)

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

char* AddressPoolManager::Reserve(pool_handle handle,
                                  void* requested_address,
                                  size_t length) {
  PA_DCHECK(!(length & DirectMapAllocationGranularityOffsetMask()));
  char* ptr = reinterpret_cast<char*>(
      AllocPages(requested_address, length, kSuperPageSize, PageInaccessible,
                 PageTag::kPartitionAlloc));
  if (UNLIKELY(!ptr))
    return nullptr;
  return ptr;
}

void AddressPoolManager::UnreserveAndDecommit(pool_handle handle,
                                              void* ptr,
                                              size_t length) {
  uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(ptr);
  PA_DCHECK(!(ptr_as_uintptr & kSuperPageOffsetMask));
  PA_DCHECK(!(length & DirectMapAllocationGranularityOffsetMask()));
  FreePages(ptr, length);
}

void AddressPoolManager::MarkUsed(pool_handle handle,
                                  const void* address,
                                  size_t length) {
  uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(address);
  AutoLock guard(AddressPoolManagerBitmap::GetLock());
  if (handle == kNonBRPPoolHandle) {
    PA_DCHECK((length %
               AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap) == 0);
    SetBitmap(
        AddressPoolManagerBitmap::non_brp_pool_bits_,
        ptr_as_uintptr >> AddressPoolManagerBitmap::kBitShiftOfNonBRPPoolBitmap,
        length >> AddressPoolManagerBitmap::kBitShiftOfNonBRPPoolBitmap);
  } else {
    PA_DCHECK(handle == kBRPPoolHandle);
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
    SetBitmap(
        AddressPoolManagerBitmap::brp_pool_bits_,
        (ptr_as_uintptr >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) +
            AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap,
        (length >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) -
            AddressPoolManagerBitmap::kGuardBitsOfBRPPoolBitmap);
  }
}

void AddressPoolManager::MarkUnused(pool_handle handle,
                                    const void* address,
                                    size_t length) {
  uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(address);
  AutoLock guard(AddressPoolManagerBitmap::GetLock());
  // Address regions allocated for normal buckets are never freed, so frequency
  // of codepaths taken depends solely on which pool direct map allocations go
  // to. In the USE_BACKUP_REF_PTR case, they usually go to BRP pool (except for
  // aligned partition). Otherwise, they always go to non-BRP pool.
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (UNLIKELY(handle == kNonBRPPoolHandle)) {
#else
  if (LIKELY(handle == kNonBRPPoolHandle)) {
#endif
    PA_DCHECK((length %
               AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap) == 0);
    ResetBitmap(
        AddressPoolManagerBitmap::non_brp_pool_bits_,
        ptr_as_uintptr >> AddressPoolManagerBitmap::kBitShiftOfNonBRPPoolBitmap,
        length >> AddressPoolManagerBitmap::kBitShiftOfNonBRPPoolBitmap);
  } else {
    PA_DCHECK(handle == kBRPPoolHandle);
    PA_DCHECK(
        (length % AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap) == 0);

    // Make IsManagedByBRPPoolPool() return false when an address inside the
    // first or the last PartitionPageSize()-bytes block is given.
    // (See MarkUsed comment)
    ResetBitmap(
        AddressPoolManagerBitmap::brp_pool_bits_,
        (ptr_as_uintptr >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) +
            AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap,
        (length >> AddressPoolManagerBitmap::kBitShiftOfBRPPoolBitmap) -
            AddressPoolManagerBitmap::kGuardBitsOfBRPPoolBitmap);
  }
}

void AddressPoolManager::ResetForTesting() {
  AutoLock guard(AddressPoolManagerBitmap::GetLock());
  AddressPoolManagerBitmap::non_brp_pool_bits_.reset();
  AddressPoolManagerBitmap::brp_pool_bits_.reset();
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

AddressPoolManager::AddressPoolManager() = default;
AddressPoolManager::~AddressPoolManager() = default;

}  // namespace internal
}  // namespace base
