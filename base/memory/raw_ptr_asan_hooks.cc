// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_hooks.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>

#include <cstring>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/raw_ptr_asan_event.h"
#include "base/memory/raw_ptr_asan_service.h"
#include "partition_alloc/partition_lock.h"

namespace base {

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace {

// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L139
constexpr uint8_t kAsanHeapFreeMagic = 0xfd;

bool IsInAllocatedChunk(const void* allocation_start_ptr, uintptr_t address) {
  uintptr_t allocation_start_address =
      reinterpret_cast<uintptr_t>(allocation_start_ptr);
  return allocation_start_address <= address &&
         address <
             allocation_start_address +
                 __sanitizer_get_allocated_size_fast(allocation_start_ptr);
}

}  // namespace

NO_SANITIZE("address")
void RawPtrAsanService::AcquireInternal(uintptr_t address, bool is_copy) const {
  if (!address) {
    return;
  }

  const void* allocation_start_ptr =
      __sanitizer_get_allocated_begin(reinterpret_cast<void*>(address));
  if (!allocation_start_ptr ||
      !IsInAllocatedChunk(allocation_start_ptr, address)) {
    allocation_start_ptr =
        __sanitizer_get_allocated_begin(reinterpret_cast<void*>(address - 1));
    if (!allocation_start_ptr) {
      // The `address` looks not allocated.
      if (IsFreed(address)) {
        LogEvent(internal::RawPtrAsanEvent::Type::kFreeAssignment, address, 0);
        PA_CHECK(false) << "pointer laundering";
      }
      // The given `address` seems not to be owned by ASAN.
      // For example, AccessedFeature() does feature_ = &feature.
      // The `&feature` is `base::Feature kFeature;` not by allocated from
      // heap.
      return;
    }

    if (!IsInAllocatedChunk(allocation_start_ptr, address - 1)) {
      // This means, the given address is allocated by ASAN allocator,
      // but without malloc(). For example, mmap() is intercepted by
      // ASAN. ASAN mmap() allocates memory from asan heap, but
      // ASAN doesn't invoke malloc hooks, because mmap() is not malloc().
      // In this case, __asan_locate_address() returns "heap", but
      // the address is not in the returned region.
      // This looks like "not managed by PartitionAlloc".
      return;
    }
  }

  if (!IsSupportedAllocation(allocation_start_ptr)) {
    return;  // This means, early allocation.
  }

  QuarantineFlag quarantine_flag;
  {
    auto& map = RawPtrAsanService::GetAllocationMetadataMap(
        reinterpret_cast<uintptr_t>(allocation_start_ptr));
    internal::PartitionAutoLock lock(map.GetLock());
    auto it =
        map.GetMap().find(reinterpret_cast<uintptr_t>(allocation_start_ptr));
    PA_DCHECK(it != map.GetMap().end());
    // Check overflow.
    PA_CHECK(it->second.count < kMaxPtrCount);
    ++it->second.count;
    quarantine_flag = it->second.quarantine_flag;
  }

  if (is_data_race_check_enabled_ && !is_copy &&
      quarantine_flag != QuarantineFlag::NotQuarantined) [[unlikely]] {
    // This should not trigger an immediate crash, since this is protected, but
    // we do want to do data-race checking and report these as bugs, since we're
    // assigning a dangling pointer to a raw_ptr<T>.
    LogEvent(internal::RawPtrAsanEvent::Type::kQuarantineAssignment, address,
             __sanitizer_get_allocated_size_fast(allocation_start_ptr));
  }
}

NO_SANITIZE("address")
uintptr_t RawPtrAsanService::GetAllocationStart(uintptr_t address) const {
  const void* allocation_start_ptr =
      __sanitizer_get_allocated_begin(reinterpret_cast<void*>(address));
  // Be careful. `address` might point to the end of allocation. In this
  // case, __sanitizer_get_allocated_begin() will return nullptr or
  // the begin of the next chunk.
  if (!allocation_start_ptr ||
      !IsInAllocatedChunk(allocation_start_ptr, address)) {
    // We will try `address-1` to obtain an allocated begin.
    allocation_start_ptr =
        __sanitizer_get_allocated_begin(reinterpret_cast<void*>(address - 1));

    if (!allocation_start_ptr ||
        !IsInAllocatedChunk(allocation_start_ptr, address - 1)) {
      return 0;
    }
  }

  if (!IsSupportedAllocation(const_cast<void*>(allocation_start_ptr))) {
    return 0;
  }

  return reinterpret_cast<uintptr_t>(allocation_start_ptr);
}

NO_SANITIZE("address")
void RawPtrAsanService::ReleaseInternal(uintptr_t address) const {
  if (!address) {
    return;
  }

  uintptr_t allocation_start_address = GetAllocationStart(address);
  if (!allocation_start_address) {
    return;
  }

  {
    auto& map =
        RawPtrAsanService::GetAllocationMetadataMap(allocation_start_address);
    internal::PartitionAutoLock lock(map.GetLock());
    auto it = map.GetMap().find(allocation_start_address);
    PA_CHECK(it != map.GetMap().end());
    PA_CHECK(it->second.count > 0u);
    --it->second.count;
    // Still referenced or not quarantined, return.
    if (it->second.count != 0u ||
        it->second.quarantine_flag != QuarantineFlag::Quarantined) {
      return;
    }
  }

  if (is_free_after_quarantined_check_enabled_) {
    LogEvent(internal::RawPtrAsanEvent::Type::kQuarantineExit,
             allocation_start_address,
             __sanitizer_get_allocated_size_fast(
                 reinterpret_cast<void*>(allocation_start_address)));
  }

  // Invoke free() for the address. This will cause `__sanitizer_free_hook()`
  // again. `quarantined_allocation_` and `allocations_` will be updated
  // inside the hook. Must unlock `lock_`, because the free hook wants to lock
  // it.

  // This `free()` may cause `alloc-dealloc-mismatch (operator new vs free)`,
  // However, the allocation was ignored by IgnoreFreeHook and now actually
  // to be freed (if destructor is needed, before invoking IgnoreFreeHook,
  // the code has already invoked destructor?). And at this time, there is
  // no way to find which we should use, free() or delete.
  // So we will suppress alloc-dealloc-mismatch. The configuration was
  // added to //build/sanitizers/asan_suppressions.cc.
  free(reinterpret_cast<void*>(allocation_start_address));
}

NO_SANITIZE("address")
bool RawPtrAsanService::IsQuarantined(uintptr_t address) const {
  if (!address) {
    return false;
  }

  // Use `__asan_region_is_poisoned` instead of `__asan_address_is_poisoned`
  // because the latter may crash on an invalid pointer.
  if (!__asan_region_is_poisoned(reinterpret_cast<void*>(address), 1)) {
    return false;
  }

  uintptr_t allocation_start_address = GetAllocationStart(address);
  if (!allocation_start_address) {
    return false;
  }

  PA_CHECK(__sanitizer_get_ownership(
      reinterpret_cast<void*>(allocation_start_address)));
  {
    auto& map =
        RawPtrAsanService::GetAllocationMetadataMap(allocation_start_address);
    internal::PartitionAutoLock lock(map.GetLock());
    auto it = map.GetMap().find(allocation_start_address);
    // Regarding early allocations, `it == map.GetMap().end()`.
    // `quarantined` will be `false`.
    bool quarantined =
        it != map.GetMap().end() &&
        it->second.quarantine_flag == QuarantineFlag::Quarantined;
    return quarantined;
  }
}

// IsFreed() checks whether the given address points to a freed memory
// region (not including quarantined memory region) or not.
NO_SANITIZE("address")
bool RawPtrAsanService::IsFreed(uintptr_t address) const {
  if (!address) {
    return false;
  }

  void* ptr = reinterpret_cast<void*>(address);
  // Use `__asan_region_is_poisoned` instead of `__asan_address_is_poisoned`
  // because the latter may crash on an invalid pointer.
  if (!__asan_region_is_poisoned(ptr, 1)) {
    return false;
  }

  // Make sure the address is on the heap and is not in a redzone.
  void* chunk_begin_ptr;
  size_t chunk_size;
  // __asan_locate_address() is very slow.
  // If asan_mapping.h is available, we can just use AddrIsInMem() to find
  // whether the address is in asan heap or not.
  const std::string_view allocation_type =
      __asan_locate_address(ptr, nullptr, 0, &chunk_begin_ptr, &chunk_size);

  auto chunk_begin_address = reinterpret_cast<uintptr_t>(chunk_begin_ptr);
  if (allocation_type != "heap" || address < chunk_begin_address ||
      address >= chunk_begin_address +
                     chunk_size) {  // We exclude pointers one past the end
                                    // of an allocations from the analysis
                                    // for now because they're to fragile.
    return false;
  }

  return kAsanHeapFreeMagic == *GetShadow(ptr);
}

#else   // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace {
bool IsFreedHeapPointer(uintptr_t address) {
  // Use `__asan_region_is_poisoned` instead of `__asan_address_is_poisoned`
  // because the latter may crash on an invalid pointer.
  if (!__asan_region_is_poisoned(reinterpret_cast<void*>(address), 1)) {
    return false;
  }

  // Make sure the address is on the heap and is not in a redzone.
  void* region_ptr;
  size_t region_size;
  const char* allocation_type = __asan_locate_address(
      reinterpret_cast<void*>(address), nullptr, 0, &region_ptr, &region_size);

  auto region_base = reinterpret_cast<uintptr_t>(region_ptr);
  if (UNSAFE_TODO(strcmp(allocation_type, "heap")) != 0 ||
      address < region_base ||
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
    uintptr_t address) {
  NO_CODE_FOLDING();
  auto unused = *reinterpret_cast<char const volatile*>(address);
  asm volatile("" : "+r"(unused));
}
}  // namespace
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace {

void WrapPtr(uintptr_t address) {
  auto& service = RawPtrAsanService::GetInstance();

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  if (service.IsEnabled()) {
    service.AcquireInternal(address);
  }
#else
  if (service.is_instantiation_check_enabled() && IsFreedHeapPointer(address)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kInstantiation,
        reinterpret_cast<void*>(address));
    service.CrashOnDanglingInstantiation(reinterpret_cast<void*>(address));
  }
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}

void ReleaseWrappedPtr([[maybe_unused]] uintptr_t address) {
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  auto& service = RawPtrAsanService::GetInstance();
  if (service.IsEnabled()) {
    service.ReleaseInternal(address);
  }
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}

void SafelyUnwrapForDereference([[maybe_unused]] uintptr_t address) {
#if !PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  if (RawPtrAsanService::GetInstance().is_dereference_check_enabled() &&
      IsFreedHeapPointer(address)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kDereference,
        reinterpret_cast<void*>(address));
    CrashImmediatelyOnUseAfterFree(address);
  }
#endif  // !PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}

void SafelyUnwrapForExtraction([[maybe_unused]] uintptr_t address) {
#if !PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  auto& service = RawPtrAsanService::GetInstance();

  if ((service.is_extraction_check_enabled() ||
       service.is_dereference_check_enabled()) &&
      IsFreedHeapPointer(address)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kExtraction,
        reinterpret_cast<void*>(address));
    // If the dereference check is enabled, we still record the extraction event
    // to catch the potential subsequent dangling dereference, but don't report
    // the extraction itself.
    if (service.is_extraction_check_enabled()) {
      service.WarnOnDanglingExtraction(reinterpret_cast<void*>(address));
    }
  }
#endif  // !PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}

void UnsafelyUnwrapForComparison(uintptr_t) {}

void Advance(uintptr_t, uintptr_t) {}

void Duplicate([[maybe_unused]] uintptr_t address) {
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  if (!address) {
    return;
  }

  auto& service = RawPtrAsanService::GetInstance();
  if (service.IsEnabled()) {
    service.AcquireInternal(address, /*is_copy=*/true);
  }
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}

void WrapPtrForDuplication(uintptr_t address) {
  Duplicate(address);
}

void UnsafelyUnwrapForDuplication(uintptr_t) {}

}  // namespace

namespace internal {

const RawPtrHooks* GetRawPtrAsanHooks() {
  static constexpr RawPtrHooks hooks = {
      WrapPtr,
      ReleaseWrappedPtr,
      SafelyUnwrapForDereference,
      SafelyUnwrapForExtraction,
      UnsafelyUnwrapForComparison,
      Advance,
      Duplicate,
      WrapPtrForDuplication,
      UnsafelyUnwrapForDuplication,
  };

  return &hooks;
}

}  // namespace internal
}  // namespace base

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
