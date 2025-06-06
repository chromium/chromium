// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNALS_WIN_H_
#define PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNALS_WIN_H_

#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/oom.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_internal.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_check.h"

namespace partition_alloc::internal {

// |VirtualAlloc| will fail if allocation at the hint address is blocked.
constexpr bool kHintIsAdvisory = false;
std::atomic<int32_t> s_allocPageErrorCode{ERROR_SUCCESS};

bool IsOutOfMemory(DWORD error) {
  // From
  // https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-
  switch (error) {
    // Page file is being extended.
    case ERROR_COMMITMENT_MINIMUM:
    // Page file is too small.
    case ERROR_COMMITMENT_LIMIT:
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    // Not enough memory resources are available to process this command.
    //
    // It is not entirely clear whether this error pertains to out of address
    // space errors, or the kernel being out of memory. Only include it for 64
    // bit architectures, since address space issues are unlikely there.
    case ERROR_NOT_ENOUGH_MEMORY:
#endif
    case ERROR_PAGEFILE_QUOTA:
      // Insufficient quota to complete the requested service.
      return true;
    default:
      return false;
  }
}

void* VirtualAllocWithRetry(void* address,
                            size_t size,
                            DWORD type_flags,
                            DWORD access_flags) {
  // In case of commit failure, this function may repeatedly:
  // 1. Terminate a less important process, if one was provided.
  // 2. Wait 50ms. Local experiments on Win11 show that a process' commit charge
  //    is not immediately relinquished on termination, but it is after 50ms
  //    (this is on 1 machine without CPU contention, we don't know what
  //    conditions can affect this result).
  // 3. Retry the commit.
  //
  // Even if this function cannot terminate a less important process (step 1),
  // the commit may eventually succeed after:
  // - The page file is extended.
  // - Another process terminates (possibly because of OOM).
  //
  // The wait+retry loop (steps 2 and 3, not step 1) has been shown to be
  // effective for Firefox, see crbug.com/1392738 for context. `kDelayMs` is
  // taken from Firefox, and supported by our experiments showing that commit
  // charge is relinquished within 50ms after process termination. `kMaxTries`
  // is based on our observation that Windows perform memory management every 1s
  // (we want to retry for a little bit more than 1s) - Firefox used 10 retries.
  constexpr int kMaxTries = 25;
  constexpr int kDelayMs = 50;

  bool should_retry = GetRetryOnCommitFailure() && (type_flags & MEM_COMMIT) &&
                      (access_flags != PAGE_NOACCESS);
  for (int tries = 0; tries < kMaxTries; tries++) {
    void* ret = VirtualAlloc(address, size, type_flags, access_flags);
    // Only retry for commit failures. If this is an address space problem (e.g.
    // caller asked for an address which is not available), this is unlikely to
    // be resolved by waiting.
    if (ret || !should_retry || !IsOutOfMemory(GetLastError())) {
      return ret;
    }

    TerminateAnotherProcessOnCommitFailure();
    ::Sleep(kDelayMs);
  }
  return nullptr;
}

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
    case PageAccessibilityConfiguration::kReadWriteExecuteProtected:
      return PAGE_EXECUTE_READWRITE;
    case PageAccessibilityConfiguration::kInaccessible:
    case PageAccessibilityConfiguration::kInaccessibleWillJitLater:
      return PAGE_NOACCESS;
  }
  PA_NOTREACHED();
}

uintptr_t SystemAllocPagesInternal(
    uintptr_t hint,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageTag page_tag,
    [[maybe_unused]] int file_descriptor_for_shared_alloc) {
  const DWORD access_flag = GetAccessFlags(accessibility);
  const DWORD type_flags =
      (access_flag == PAGE_NOACCESS) ? MEM_RESERVE : (MEM_RESERVE | MEM_COMMIT);
  void* ret = VirtualAllocWithRetry(reinterpret_cast<void*>(hint), length,
                                    type_flags, access_flag);
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
  if (GetAccessFlags(accessibility) == PAGE_NOACCESS) {
    return VirtualFree(ptr, length, MEM_DECOMMIT) != 0;
  }
  // Call the retry path even though this function can fail, because callers of
  // this are likely to crash the process when this function fails, and we don't
  // want that for transient failures.
  return nullptr != VirtualAllocWithRetry(ptr, length, MEM_COMMIT,
                                          GetAccessFlags(accessibility));
}

void SetSystemPagesAccessInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  void* ptr = reinterpret_cast<void*>(address);
  const DWORD access_flag = GetAccessFlags(accessibility);
  if (access_flag == PAGE_NOACCESS) {
    if (!VirtualFree(ptr, length, MEM_DECOMMIT)) {
      // We check `GetLastError` for `ERROR_SUCCESS` here so that in a crash
      // report we get the error number.
      PA_CHECK(static_cast<uint32_t>(ERROR_SUCCESS) == GetLastError());
    }
  } else {
    if (!VirtualAllocWithRetry(ptr, length, MEM_COMMIT, access_flag)) {
      int32_t error = GetLastError();
      if (error == ERROR_COMMITMENT_LIMIT ||
          error == ERROR_COMMITMENT_MINIMUM) {
        OOM_CRASH(length);
      }
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
                       PageAccessibilityConfiguration(
                           PageAccessibilityConfiguration::kInaccessible));
}

bool DecommitAndZeroSystemPagesInternal(uintptr_t address,
                                        size_t length,
                                        PageTag page_tag) {
  // https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree:
  // "If a page is decommitted but not released, its state changes to reserved.
  // Subsequently, you can call VirtualAlloc to commit it, or VirtualFree to
  // release it. Attempts to read from or write to a reserved page results in an
  // access violation exception."
  // https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
  // for MEM_COMMIT: "The function also guarantees that when the caller later
  // initially accesses the memory, the contents will be zero."
  PA_CHECK(VirtualFree(reinterpret_cast<void*>(address), length, MEM_DECOMMIT));
  return true;
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
  void* ptr = reinterpret_cast<void*>(address);
  // Use DiscardVirtualMemory when available because it releases faster than
  // MEM_RESET.
  DWORD ret = DiscardVirtualMemory(ptr, length);
  // DiscardVirtualMemory is buggy in Win10 SP0, so fall back to MEM_RESET on
  // failure.
  if (ret) {
    PA_CHECK(VirtualAllocWithRetry(ptr, length, MEM_RESET, PAGE_READWRITE));
  }
}

bool SealSystemPagesInternal(uintptr_t address, size_t length) {
  return false;
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNALS_WIN_H_
