// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
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
  if constexpr (AllowDangling)
    partition_alloc::internal::PartitionRefCountPointer(slot_start)
        ->AcquireFromUnprotectedPtr();
  else
    partition_alloc::internal::PartitionRefCountPointer(slot_start)->Acquire();
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
            ->ReleaseFromUnprotectedPtr())
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
  } else {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->Release())
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
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
partition_alloc::PtrPosWithinAlloc
BackupRefPtrImpl<AllowDangling>::IsValidSignedDelta(uintptr_t address,
                                                    ptrdiff_t delta_in_bytes) {
  return partition_alloc::internal::PartitionAllocIsValidPtrDelta(
      address, delta_in_bytes);
}

template <bool AllowDangling>
partition_alloc::PtrPosWithinAlloc
BackupRefPtrImpl<AllowDangling>::IsValidUnsignedDelta(uintptr_t address,
                                                      size_t delta_in_bytes) {
  return partition_alloc::internal::PartitionAllocIsValidPtrDelta(
      address, delta_in_bytes);
}

// Explicitly instantiates the two BackupRefPtr variants in the .cc. This
// ensures the definitions not visible from the .h are available in the binary.
template struct BackupRefPtrImpl</*AllowDangling=*/false>;
template struct BackupRefPtrImpl</*AllowDangling=*/true>;

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

#elif BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <sanitizer/asan_interface.h>
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_asan_service.h"
#include "base/process/process.h"

namespace base::internal {

namespace {
bool IsFreedHeapPointer(void const volatile* ptr) {
  // Use `__asan_region_is_poisoned` instead of `__asan_address_is_poisoned`
  // because the latter may crash on an invalid pointer.
  if (!__asan_region_is_poisoned(const_cast<void*>(ptr), 1))
    return false;

  // Make sure the address is on the heap and is not in a redzone.
  void* region_ptr;
  size_t region_size;
  const char* allocation_type = __asan_locate_address(
      const_cast<void*>(ptr), nullptr, 0, &region_ptr, &region_size);

  auto address = reinterpret_cast<uintptr_t>(ptr);
  auto region_base = reinterpret_cast<uintptr_t>(region_ptr);
  if (strcmp(allocation_type, "heap") != 0 || address < region_base ||
      address >=
          region_base + region_size) {  // We exclude pointers one past the end
                                        // of an allocations from the analysis
                                        // for now because they're to fragile.
    return false;
  }

  // Make sure the allocation has been actually freed rather than
  // user-poisoned.
  int free_thread_id = -1;
  __asan_get_free_stack(region_ptr, nullptr, 0, &free_thread_id);
  return free_thread_id != -1;
}

// Force a non-optimizable memory load operation to trigger an ASan crash.
NOINLINE NOT_TAIL_CALLED void CrashImmediatelyOnUseAfterFree(
    void const volatile* ptr) {
  NO_CODE_FOLDING();
  auto unused = *reinterpret_cast<char const volatile*>(ptr);
  asm volatile("" : "+r"(unused));
}
}  // namespace

NO_SANITIZE("address")
void AsanBackupRefPtrImpl::AsanCheckIfValidDereference(
    void const volatile* ptr) {
  if (RawPtrAsanService::GetInstance().is_dereference_check_enabled() &&
      IsFreedHeapPointer(ptr)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kDereference, ptr);
    CrashImmediatelyOnUseAfterFree(ptr);
  }
}

NO_SANITIZE("address")
void AsanBackupRefPtrImpl::AsanCheckIfValidExtraction(
    void const volatile* ptr) {
  auto& service = RawPtrAsanService::GetInstance();

  if ((service.is_extraction_check_enabled() ||
       service.is_dereference_check_enabled()) &&
      IsFreedHeapPointer(ptr)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kExtraction, ptr);
    // If the dereference check is enabled, we still record the extraction event
    // to catch the potential subsequent dangling dereference, but don't report
    // the extraction itself.
    if (service.is_extraction_check_enabled()) {
      RawPtrAsanService::Log(
          "=================================================================\n"
          "==%d==WARNING: MiraclePtr: dangling-pointer-extraction on address "
          "%p\n"
          "extracted here:",
          Process::Current().Pid(), ptr);
      __sanitizer_print_stack_trace();
      __asan_describe_address(const_cast<void*>(ptr));
      RawPtrAsanService::Log(
          "A regular ASan report will follow if the extracted pointer is "
          "dereferenced later.\n"
          "Otherwise, it is still likely a bug to rely on the address of an "
          "already freed allocation.\n"
          "Refer to "
          "https://chromium.googlesource.com/chromium/src/+/main/base/memory/"
          "raw_ptr.md for details.\n"
          "=================================================================");
    }
  }
}

NO_SANITIZE("address")
void AsanBackupRefPtrImpl::AsanCheckIfValidInstantiation(
    void const volatile* ptr) {
  if (RawPtrAsanService::GetInstance().is_instantiation_check_enabled() &&
      IsFreedHeapPointer(ptr)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kInstantiation, ptr);
    CrashImmediatelyOnUseAfterFree(ptr);
  }
}

}  // namespace base::internal

#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
