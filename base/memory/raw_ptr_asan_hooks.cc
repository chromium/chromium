// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_hooks.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <cstring>

#include <sanitizer/asan_interface.h>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/raw_ptr_asan_service.h"

namespace base::internal {

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
    uintptr_t address) {
  NO_CODE_FOLDING();
  auto unused = *reinterpret_cast<char const volatile*>(address);
  asm volatile("" : "+r"(unused));
}

void WrapPtr(uintptr_t address) {
  auto& service = RawPtrAsanService::GetInstance();

  if (service.is_instantiation_check_enabled() && IsFreedHeapPointer(address)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kInstantiation,
        reinterpret_cast<void*>(address));
    service.CrashOnDanglingInstantiation(reinterpret_cast<void*>(address));
  }
}

void ReleaseWrappedPtr(uintptr_t) {}

void SafelyUnwrapForDereference(uintptr_t address) {
  if (RawPtrAsanService::GetInstance().is_dereference_check_enabled() &&
      IsFreedHeapPointer(address)) {
    RawPtrAsanService::SetPendingReport(
        RawPtrAsanService::ReportType::kDereference,
        reinterpret_cast<void*>(address));
    CrashImmediatelyOnUseAfterFree(address);
  }
}

void SafelyUnwrapForExtraction(uintptr_t address) {
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
}

void UnsafelyUnwrapForComparison(uintptr_t) {}

void Advance(uintptr_t, uintptr_t) {}

void Duplicate(uintptr_t) {}

void WrapPtrForDuplication(uintptr_t) {}

void UnsafelyUnwrapForDuplication(uintptr_t) {}

}  // namespace

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

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
