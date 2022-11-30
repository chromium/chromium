// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_WIN_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_WIN_H_

#include <versionhelpers.h>

#include <cstdint>

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"

namespace partition_alloc::internal {

namespace {

// On Windows, discarded pages are not returned to the system immediately and
// not guaranteed to be zeroed when returned to the application.
using DiscardVirtualMemoryFunction = DWORD(WINAPI*)(PVOID virtualAddress,
                                                    SIZE_T size);
DiscardVirtualMemoryFunction s_discard_virtual_memory =
    reinterpret_cast<DiscardVirtualMemoryFunction>(-1);

}  // namespace

// |VirtualAlloc| will fail if allocation at the hint address is blocked.
constexpr bool kHintIsAdvisory = false;
std::atomic<int32_t> s_allocPageErrorCode{ERROR_SUCCESS};

int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  switch (accessibility.permissions) {
    case PageAccessibilityConfiguration::kRead:
      return PAGE_READONLY;
    case PageAccessibilityConfiguration::kReadWrite:
    case PageAccessibilityConfiguration::kReadWriteTagged:
      return PAGE_READWRITE;
    case PageAccessibilityConfiguration::kReadExecute:
    case PageAccessibilityConfiguration::kReadExecuteProtected:
      return PAGE_EXECUTE_READ;
    case PageAccessibilityConfiguration::kReadWriteExecute:
      return PAGE_EXECUTE_READWRITE;
    default:
      PA_NOTREACHED();
      [[fallthrough]];
    case PageAccessibilityConfiguration::kInaccessible:
      return PAGE_NOACCESS;
  }
}

uintptr_t SystemAllocPagesInternal(
    uintptr_t hint,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageTag page_tag,
    [[maybe_unused]] int file_descriptor_for_shared_alloc) {
  DWORD access_flag = GetAccessFlags(accessibility);
  const DWORD type_flags = (accessibility.permissions !=
                            PageAccessibilityConfiguration::kInaccessible)
                               ? (MEM_RESERVE | MEM_COMMIT)
                               : MEM_RESERVE;
  void* ret = VirtualAlloc(reinterpret_cast<void*>(hint), length, type_flags,
                           access_flag);
  if (ret == nullptr) {
    s_allocPageErrorCode = GetLastError();
  }
  return reinterpret_cast<uintptr_t>(ret);
}

uintptr_t TrimMappingInternal(uintptr_t base_address,
                              size_t base_length,
                              size_t trim_length,
                              PageAccessibilityConfiguration accessibility,
                              size_t pre_slack,
                              size_t post_slack) {
  uintptr_t ret = base_address;
  if (pre_slack || post_slack) {
    // We cannot resize the allocation run. Free it and retry at the aligned
    // address within the freed range.
    ret = base_address + pre_slack;
    FreePages(base_address, base_length);
    ret = SystemAllocPages(ret, trim_length, accessibility, PageTag::kChromium);
  }
  return ret;
}

bool TrySetSystemPagesAccessInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  void* ptr = reinterpret_cast<void*>(address);
  if (accessibility.permissions ==
      PageAccessibilityConfiguration::kInaccessible)
    return VirtualFree(ptr, length, MEM_DECOMMIT) != 0;
  return nullptr !=
         VirtualAlloc(ptr, length, MEM_COMMIT, GetAccessFlags(accessibility));
}

void SetSystemPagesAccessInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  void* ptr = reinterpret_cast<void*>(address);
  if (accessibility.permissions ==
      PageAccessibilityConfiguration::kInaccessible) {
    if (!VirtualFree(ptr, length, MEM_DECOMMIT)) {
      // We check `GetLastError` for `ERROR_SUCCESS` here so that in a crash
      // report we get the error number.
      PA_CHECK(static_cast<uint32_t>(ERROR_SUCCESS) == GetLastError());
    }
  } else {
    if (!VirtualAlloc(ptr, length, MEM_COMMIT, GetAccessFlags(accessibility))) {
      int32_t error = GetLastError();
      if (error == ERROR_COMMITMENT_LIMIT)
        OOM_CRASH(length);
      // We check `GetLastError` for `ERROR_SUCCESS` here so that in a crash
      // report we get the error number.
      PA_CHECK(ERROR_SUCCESS == error);
    }
  }
}

void FreePagesInternal(uintptr_t address, size_t length) {
  PA_CHECK(VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE));
}

void DecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  // Ignore accessibility_disposition, because decommitting is equivalent to
  // making pages inaccessible.
  SetSystemPagesAccess(address, length,
                       PageAccessibilityConfiguration::kInaccessible);
}

void DecommitAndZeroSystemPagesInternal(uintptr_t address, size_t length) {
  // https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree:
  // "If a page is decommitted but not released, its state changes to reserved.
  // Subsequently, you can call VirtualAlloc to commit it, or VirtualFree to
  // release it. Attempts to read from or write to a reserved page results in an
  // access violation exception."
  // https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
  // for MEM_COMMIT: "The function also guarantees that when the caller later
  // initially accesses the memory, the contents will be zero."
  PA_CHECK(VirtualFree(reinterpret_cast<void*>(address), length, MEM_DECOMMIT));
}

void RecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // Ignore accessibility_disposition, because decommitting is equivalent to
  // making pages inaccessible.
  SetSystemPagesAccess(address, length, accessibility);
}

bool TryRecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // Ignore accessibility_disposition, because decommitting is equivalent to
  // making pages inaccessible.
  return TrySetSystemPagesAccess(address, length, accessibility);
}

void DiscardSystemPagesInternal(uintptr_t address, size_t length) {
  if (s_discard_virtual_memory ==
      reinterpret_cast<DiscardVirtualMemoryFunction>(-1)) {
    // DiscardVirtualMemory's minimum supported client is Windows 8.1 Update.
    // So skip GetProcAddress("DiscardVirtualMemory") if windows version is
    // smaller than Windows 8.1.
    if (IsWindows8Point1OrGreater()) {
      s_discard_virtual_memory =
          reinterpret_cast<DiscardVirtualMemoryFunction>(GetProcAddress(
              GetModuleHandle(L"Kernel32.dll"), "DiscardVirtualMemory"));
    } else {
      s_discard_virtual_memory = nullptr;
    }
  }

  void* ptr = reinterpret_cast<void*>(address);
  // Use DiscardVirtualMemory when available because it releases faster than
  // MEM_RESET.
  DWORD ret = 1;
  if (s_discard_virtual_memory) {
    ret = s_discard_virtual_memory(ptr, length);
  }
  // DiscardVirtualMemory is buggy in Win10 SP0, so fall back to MEM_RESET on
  // failure.
  if (ret) {
    PA_CHECK(VirtualAlloc(ptr, length, MEM_RESET, PAGE_READWRITE));
  }
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_WIN_H_
