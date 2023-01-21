// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include <cstdint>

#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_base/check.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"

namespace base::internal {

template <bool AllowDangling>
void BackupRefPtrImpl<AllowDangling>::AcquireInternal(uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  if constexpr (AllowDangling) {
    partition_alloc::internal::PartitionRefCountPointer(slot_start)
        ->AcquireFromUnprotectedPtr();
  } else {
    partition_alloc::internal::PartitionRefCountPointer(slot_start)->Acquire();
  }
}

template <bool AllowDangling>
void BackupRefPtrImpl<AllowDangling>::ReleaseInternal(uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  if constexpr (AllowDangling) {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->ReleaseFromUnprotectedPtr()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  } else {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->Release()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  }
}

template <bool AllowDangling>
void BackupRefPtrImpl<AllowDangling>::ReportIfDanglingInternal(
    uintptr_t address) {
  if (partition_alloc::internal::IsUnretainedDanglingRawPtrCheckEnabled()) {
    if (IsSupportedAndNotNull(address)) {
      uintptr_t slot_start =
          partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
      partition_alloc::internal::PartitionRefCountPointer(slot_start)
          ->ReportIfDangling();
    }
  }
}

template <bool AllowDangling>
bool BackupRefPtrImpl<AllowDangling>::IsPointeeAlive(uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  return partition_alloc::internal::PartitionRefCountPointer(slot_start)
      ->IsAlive();
}

template <bool AllowDangling>
template <typename Z>
partition_alloc::PtrPosWithinAlloc
BackupRefPtrImpl<AllowDangling>::IsValidDelta(
    uintptr_t address,
    partition_alloc::internal::PtrDelta<Z> delta) {
  return partition_alloc::internal::PartitionAllocIsValidPtrDelta(address,
                                                                  delta);
}

// Explicitly instantiates the two BackupRefPtr variants in the .cc. This
// ensures the definitions not visible from the .h are available in the binary.
template struct BackupRefPtrImpl</*AllowDangling=*/false>;
template struct BackupRefPtrImpl</*AllowDangling=*/true>;

template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc BackupRefPtrImpl<false>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<size_t>);
template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc BackupRefPtrImpl<false>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<ptrdiff_t>);
template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc BackupRefPtrImpl<true>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<size_t>);
template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc BackupRefPtrImpl<true>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<ptrdiff_t>);

#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address) {
  if (partition_alloc::internal::IsManagedByDirectMap(address)) {
    uintptr_t reservation_start =
        partition_alloc::internal::GetDirectMapReservationStart(address);
    PA_BASE_CHECK(address - reservation_start >=
                  partition_alloc::PartitionPageSize());
  } else {
    PA_BASE_CHECK(partition_alloc::internal::IsManagedByNormalBuckets(address));
    PA_BASE_CHECK(address % partition_alloc::kSuperPageSize >=
                  partition_alloc::PartitionPageSize());
  }
}
#endif  // BUILDFLAG(PA_DCHECK_IS_ON) ||
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace base::internal

#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#if BUILDFLAG(USE_ASAN_UNOWNED_PTR)

#include <sanitizer/asan_interface.h>
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"

namespace base::internal {

PA_NO_SANITIZE("address")
bool AsanUnownedPtrImpl::EndOfAliveAllocation(const volatile void* ptr) {
  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
  return __asan_region_is_poisoned(reinterpret_cast<void*>(address), 1) &&
         !__asan_region_is_poisoned(reinterpret_cast<void*>(address - 1), 1);
}

bool AsanUnownedPtrImpl::LikelySmuggledScalar(const volatile void* ptr) {
  intptr_t address = reinterpret_cast<intptr_t>(ptr);
  return address < 0x4000;  // Negative or small positive.
}

}  // namespace base::internal

#endif  // BUILDFLAG(USE_ASAN_UNOWNED_PTR)

#if BUILDFLAG(USE_HOOKABLE_RAW_PTR)
#include <atomic>

namespace base::internal {

namespace {

void DefaultWrapPtrHook(uintptr_t address) {}
void DefaultReleaseWrappedPtrHook(uintptr_t address) {}
void DefaultUnwrapForDereferenceHook(uintptr_t address) {}
void DefaultUnwrapForExtractionHook(uintptr_t address) {}
void DefaultUnwrapForComparisonHook(uintptr_t address) {}
void DefaultAdvanceHook(uintptr_t old_address, uintptr_t new_address) {}
void DefaultDuplicateHook(uintptr_t address) {}

constexpr RawPtrHooks default_hooks = {
    DefaultWrapPtrHook,
    DefaultReleaseWrappedPtrHook,
    DefaultUnwrapForDereferenceHook,
    DefaultUnwrapForExtractionHook,
    DefaultUnwrapForComparisonHook,
    DefaultAdvanceHook,
    DefaultDuplicateHook,
};

}  // namespace

std::atomic<const RawPtrHooks*> g_hooks{&default_hooks};

const RawPtrHooks* GetRawPtrHooks() {
  return g_hooks.load(std::memory_order_relaxed);
}

void InstallRawPtrHooks(const RawPtrHooks* hooks) {
  g_hooks.store(hooks, std::memory_order_relaxed);
}

void ResetRawPtrHooks() {
  InstallRawPtrHooks(&default_hooks);
}

}  // namespace base::internal

#endif  // BUILDFLAG(USE_HOOKABLE_RAW_PTR)
