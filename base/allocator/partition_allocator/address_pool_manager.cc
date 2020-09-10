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
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/bits.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/stl_util.h"

namespace base {
namespace internal {

#if defined(PA_HAS_64_BITS_POINTERS)

namespace {

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
  SetSystemPagesAccess(address, size, PageInaccessible);
  DecommitSystemPages(address, size);
#endif
}

bool WARN_UNUSED_RESULT CommitPages(void* address, size_t size) {
#if defined(OS_APPLE)
  SetSystemPagesAccess(address, size, PageReadWrite);
#else
  if (!RecommitSystemPages(address, size, PageReadWrite))
    return false;
  SetSystemPagesAccess(address, size, PageReadWrite);
#endif

  return true;
}

base::LazyInstance<AddressPoolManager>::Leaky g_address_pool_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

constexpr size_t AddressPoolManager::Pool::kMaxBits;

// static
AddressPoolManager* AddressPoolManager::GetInstance() {
  return g_address_pool_manager.Pointer();
}

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

void AddressPoolManager::ResetForTesting() {
  for (pool_handle i = 0; i < base::size(pools_); ++i)
    pools_[i].Reset();
}

void AddressPoolManager::Remove(pool_handle handle) {
  Pool* pool = GetPool(handle);
  PA_DCHECK(pool->IsInitialized());
  pool->Reset();
}

char* AddressPoolManager::Alloc(pool_handle handle, size_t length) {
  Pool* pool = GetPool(handle);
  char* ptr = reinterpret_cast<char*>(pool->FindChunk(length));

  if (UNLIKELY(!ptr) || !CommitPages(ptr, length))
    return nullptr;
  return ptr;
}

void AddressPoolManager::Free(pool_handle handle, void* ptr, size_t length) {
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
  PA_CHECK(total_bits_ <= kMaxBits);

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

uintptr_t AddressPoolManager::Pool::FindChunk(size_t requested_size) {
  base::AutoLock scoped_lock(lock_);

  const size_t required_size = bits::Align(requested_size, kSuperPageSize);
  const size_t need_bits = required_size >> kSuperPageShift;

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
      PA_DCHECK(address + required_size <= address_end_);
#endif
      return address;
    }
  }

  NOTREACHED();
  return 0;
}

void AddressPoolManager::Pool::FreeChunk(uintptr_t address, size_t free_size) {
  base::AutoLock scoped_lock(lock_);

  PA_DCHECK(!(address & kSuperPageOffsetMask));

  const size_t size = bits::Align(free_size, kSuperPageSize);
  DCHECK_LE(address_begin_, address);
#if DCHECK_IS_ON()
  PA_DCHECK(address + size <= address_end_);
#endif

  const size_t beg_bit = (address - address_begin_) / kSuperPageSize;
  const size_t end_bit = beg_bit + size / kSuperPageSize;
  for (size_t i = beg_bit; i < end_bit; ++i) {
    PA_DCHECK(alloc_bitset_.test(i));
    alloc_bitset_.reset(i);
  }
  bit_hint_ = std::min(bit_hint_, beg_bit);
}

AddressPoolManager::Pool::Pool() = default;
AddressPoolManager::Pool::~Pool() = default;

AddressPoolManager::AddressPoolManager() = default;
AddressPoolManager::~AddressPoolManager() = default;

ALWAYS_INLINE AddressPoolManager::Pool* AddressPoolManager::GetPool(
    pool_handle handle) {
  PA_DCHECK(0 < handle && handle <= kNumPools);
  return &pools_[handle - 1];
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal
}  // namespace base
