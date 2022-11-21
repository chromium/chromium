// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_POSIX_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_POSIX_H_

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include <sys/mman.h>

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/posix/eintr_wrapper.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/pkey.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "base/allocator/partition_allocator/partition_alloc_base/mac/foundation_util.h"
#if BUILDFLAG(IS_IOS)
#include "base/allocator/partition_allocator/partition_alloc_base/ios/ios_util.h"
#elif BUILDFLAG(IS_MAC)
#include "base/allocator/partition_allocator/partition_alloc_base/mac/mac_util.h"
#else
#error "Unknown platform"
#endif
#include "base/allocator/partition_allocator/partition_alloc_base/mac/scoped_cftyperef.h"

#include <Availability.h>
#include <Security/Security.h>
#include <mach/mach.h>
#endif
#if BUILDFLAG(IS_ANDROID)
#include <sys/prctl.h>
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/resource.h>
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#if BUILDFLAG(IS_MAC)

// SecTaskGetCodeSignStatus is marked as unavailable on macOS, although it’s
// available on iOS and other Apple operating systems. It is, in fact, present
// on the system since macOS 10.12.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wavailability"
uint32_t SecTaskGetCodeSignStatus(SecTaskRef task) API_AVAILABLE(macos(10.12));
#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_MAC)

namespace partition_alloc::internal {

namespace {

#if BUILDFLAG(IS_ANDROID)
const char* PageTagToName(PageTag tag) {
  // Important: All the names should be string literals. As per prctl.h in
  // //third_party/android_ndk the kernel keeps a pointer to the name instead
  // of copying it.
  //
  // Having the name in .rodata ensures that the pointer remains valid as
  // long as the mapping is alive.
  switch (tag) {
    case PageTag::kBlinkGC:
      return "blink_gc";
    case PageTag::kPartitionAlloc:
      return "partition_alloc";
    case PageTag::kChromium:
      return "chromium";
    case PageTag::kV8:
      return "v8";
    default:
      PA_DCHECK(false);
      return "";
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Tests whether the version of macOS supports the MAP_JIT flag and if the
// current process is signed with the hardened runtime and the allow-jit
// entitlement, returning whether MAP_JIT should be used to allocate regions
// that will contain JIT-compiled executable code.
bool UseMapJit() {
  if (!base::mac::IsAtLeastOS10_14()) {
    // MAP_JIT existed before macOS 10.14, but had somewhat different semantics.
    // Only one MAP_JIT region was permitted per process, but calling code here
    // will very likely require more than one such region. Since MAP_JIT is not
    // strictly necessary to write code to a region and then execute it on these
    // older OSes, don’t use it at all.
    return false;
  }

  // Until determining that the hardened runtime is enabled, early returns will
  // return true, so that MAP_JIT will be used. This is important on arm64,
  // which only allows pages to be simultaneously writable and executable when
  // in a region allocated with MAP_JIT, regardless of code signing options. On
  // arm64, an attempt to set a non-MAP_JIT page as simultaneously writable and
  // executable fails with EPERM. Although this is not enforced on x86_64,
  // MAP_JIT is harmless in that case.

  base::ScopedCFTypeRef<SecTaskRef> task(
      SecTaskCreateFromSelf(kCFAllocatorDefault));
  if (!task) {
    return true;
  }

  uint32_t flags = SecTaskGetCodeSignStatus(task);
  if (!(flags & kSecCodeSignatureRuntime)) {
    // The hardened runtime is not enabled. Note that kSecCodeSignatureRuntime
    // == CS_RUNTIME.
    return true;
  }

  // The hardened runtime is enabled. From this point on, early returns must
  // return false, indicating that MAP_JIT is not to be used. It’s an error
  // (EINVAL) to use MAP_JIT with the hardened runtime unless the JIT
  // entitlement is specified.

  base::ScopedCFTypeRef<CFTypeRef> jit_entitlement(
      SecTaskCopyValueForEntitlement(
          task.get(), CFSTR("com.apple.security.cs.allow-jit"), nullptr));
  if (!jit_entitlement)
    return false;

  return base::mac::CFCast<CFBooleanRef>(jit_entitlement.get()) ==
         kCFBooleanTrue;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

// |mmap| uses a nearby address if the hint address is blocked.
constexpr bool kHintIsAdvisory = true;
std::atomic<int32_t> s_allocPageErrorCode{0};

int GetAccessFlags(PageAccessibilityConfiguration accessibility);

uintptr_t SystemAllocPagesInternal(uintptr_t hint,
                                   size_t length,
                                   PageAccessibilityConfiguration accessibility,
                                   PageTag page_tag,
                                   int file_descriptor_for_shared_alloc) {
#if BUILDFLAG(IS_APPLE)
  // Use a custom tag to make it easier to distinguish Partition Alloc regions
  // in vmmap(1). Tags between 240-255 are supported.
  PA_DCHECK(PageTag::kFirst <= page_tag);
  PA_DCHECK(PageTag::kLast >= page_tag);
  int fd = file_descriptor_for_shared_alloc == -1
               ? VM_MAKE_TAG(static_cast<int>(page_tag))
               : file_descriptor_for_shared_alloc;
#else
  int fd = file_descriptor_for_shared_alloc;
#endif

  int access_flag = GetAccessFlags(accessibility);
  int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;

#if BUILDFLAG(IS_MAC)
  // On macOS 10.14 and higher, executables that are code signed with the
  // "runtime" option cannot execute writable memory by default. They can opt
  // into this capability by specifying the "com.apple.security.cs.allow-jit"
  // code signing entitlement and allocating the region with the MAP_JIT flag.
  static const bool kUseMapJit = UseMapJit();
  if (page_tag == PageTag::kV8 && kUseMapJit) {
    map_flags |= MAP_JIT;
  }
#endif

  void* ret = mmap(reinterpret_cast<void*>(hint), length, access_flag,
                   map_flags, fd, 0);
  if (ret == MAP_FAILED) {
    s_allocPageErrorCode = errno;
    ret = nullptr;
  }

#if BUILDFLAG(IS_ANDROID)
  // On Android, anonymous mappings can have a name attached to them. This is
  // useful for debugging, and double-checking memory attribution.
  if (ret) {
    // No error checking on purpose, testing only.
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, ret, length,
          PageTagToName(page_tag));
  }
#endif

  return reinterpret_cast<uintptr_t>(ret);
}

bool TrySetSystemPagesAccessInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
#if BUILDFLAG(ENABLE_PKEYS)
  return 0 == PkeyMprotectIfEnabled(reinterpret_cast<void*>(address), length,
                                    GetAccessFlags(accessibility),
                                    accessibility.pkey);
#else
  return 0 == PA_HANDLE_EINTR(mprotect(reinterpret_cast<void*>(address), length,
                                       GetAccessFlags(accessibility)));
#endif
}

void SetSystemPagesAccessInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  int access_flags = GetAccessFlags(accessibility);
#if BUILDFLAG(ENABLE_PKEYS)
  int ret =
      PkeyMprotectIfEnabled(reinterpret_cast<void*>(address), length,
                            GetAccessFlags(accessibility), accessibility.pkey);
#else
  int ret = PA_HANDLE_EINTR(mprotect(reinterpret_cast<void*>(address), length,
                                     GetAccessFlags(accessibility)));
#endif

  // On Linux, man mprotect(2) states that ENOMEM is returned when (1) internal
  // kernel data structures cannot be allocated, (2) the address range is
  // invalid, or (3) this would split an existing mapping in a way that would
  // exceed the maximum number of allowed mappings.
  //
  // Neither are very likely, but we still get a lot of crashes here. This is
  // because setrlimit(RLIMIT_DATA)'s limit is checked and enforced here, if the
  // access flags match a "data" mapping, which in our case would be MAP_PRIVATE
  // | MAP_ANONYMOUS, and PROT_WRITE. see the call to may_expand_vm() in
  // mm/mprotect.c in the kernel for details.
  //
  // In this case, we are almost certainly bumping into the sandbox limit, mark
  // the crash as OOM. See SandboxLinux::LimitAddressSpace() for details.
  if (ret == -1 && errno == ENOMEM && (access_flags & PROT_WRITE))
    OOM_CRASH(length);

  PA_PCHECK(0 == ret);
}

void FreePagesInternal(uintptr_t address, size_t length) {
  PA_PCHECK(0 == munmap(reinterpret_cast<void*>(address), length));
}

uintptr_t TrimMappingInternal(uintptr_t base_address,
                              size_t base_length,
                              size_t trim_length,
                              PageAccessibilityConfiguration accessibility,
                              size_t pre_slack,
                              size_t post_slack) {
  uintptr_t ret = base_address;
  // We can resize the allocation run. Release unneeded memory before and after
  // the aligned range.
  if (pre_slack) {
    FreePages(base_address, pre_slack);
    ret = base_address + pre_slack;
  }
  if (post_slack) {
    FreePages(ret + trim_length, post_slack);
  }
  return ret;
}

void DecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  // In POSIX, there is no decommit concept. Discarding is an effective way of
  // implementing the Windows semantics where the OS is allowed to not swap the
  // pages in the region.
  DiscardSystemPages(address, length);

  bool change_permissions =
      accessibility_disposition == PageAccessibilityDisposition::kRequireUpdate;
#if BUILDFLAG(PA_DCHECK_IS_ON)
  // This is not guaranteed, show that we're serious.
  //
  // More specifically, several callers have had issues with assuming that
  // memory is zeroed, this would hopefully make these bugs more visible.  We
  // don't memset() everything, because ranges can be very large, and doing it
  // over the entire range could make Chrome unusable with
  // BUILDFLAG(PA_DCHECK_IS_ON).
  //
  // Only do it when we are about to change the permissions, since we don't know
  // the previous permissions, and cannot restore them.
  if (!DecommittedMemoryIsAlwaysZeroed() && change_permissions) {
    // Memory may not be writable.
    size_t size = std::min(length, 2 * SystemPageSize());
    void* ptr = reinterpret_cast<void*>(address);
    PA_CHECK(mprotect(ptr, size, PROT_WRITE) == 0);
    memset(ptr, 0xcc, size);
  }
#endif

  // Make pages inaccessible, unless the caller requested to keep permissions.
  //
  // Note, there is a small window between these calls when the pages can be
  // incorrectly touched and brought back to memory. Not ideal, but doing those
  // operations in the opposite order resulted in PMF regression on Mac (see
  // crbug.com/1153021).
  if (change_permissions) {
    SetSystemPagesAccess(address, length,
                         PageAccessibilityConfiguration(
                             PageAccessibilityConfiguration::kInaccessible));
  }
}

void DecommitAndZeroSystemPagesInternal(uintptr_t address, size_t length) {
  // https://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html: "If
  // a MAP_FIXED request is successful, then any previous mappings [...] for
  // those whole pages containing any part of the address range [pa,pa+len)
  // shall be removed, as if by an appropriate call to munmap(), before the
  // new mapping is established." As a consequence, the memory will be
  // zero-initialized on next access.
  void* ptr = reinterpret_cast<void*>(address);
  void* ret = mmap(ptr, length, PROT_NONE,
                   MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  PA_CHECK(ptr == ret);
}

void RecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // On POSIX systems, the caller needs to simply read the memory to recommit
  // it. However, if decommit changed the permissions, recommit has to change
  // them back.
  if (accessibility_disposition ==
      PageAccessibilityDisposition::kRequireUpdate) {
    SetSystemPagesAccess(address, length, accessibility);
  }

#if BUILDFLAG(IS_APPLE)
  // On macOS, to update accounting, we need to make another syscall. For more
  // details, see https://crbug.com/823915.
  madvise(reinterpret_cast<void*>(address), length, MADV_FREE_REUSE);
#endif
}

bool TryRecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // On POSIX systems, the caller needs to simply read the memory to recommit
  // it. However, if decommit changed the permissions, recommit has to change
  // them back.
  if (accessibility_disposition ==
      PageAccessibilityDisposition::kRequireUpdate) {
    bool ok = TrySetSystemPagesAccess(address, length, accessibility);
    if (!ok)
      return false;
  }

#if BUILDFLAG(IS_APPLE)
  // On macOS, to update accounting, we need to make another syscall. For more
  // details, see https://crbug.com/823915.
  madvise(reinterpret_cast<void*>(address), length, MADV_FREE_REUSE);
#endif

  return true;
}

void DiscardSystemPagesInternal(uintptr_t address, size_t length) {
#if !BUILDFLAG(IS_NACL)
  void* ptr = reinterpret_cast<void*>(address);
#if BUILDFLAG(IS_APPLE)
  int ret = madvise(ptr, length, MADV_FREE_REUSABLE);
  if (ret) {
    // MADV_FREE_REUSABLE sometimes fails, so fall back to MADV_DONTNEED.
    ret = madvise(ptr, length, MADV_DONTNEED);
  }
  PA_PCHECK(ret == 0);
#else
  // We have experimented with other flags, but with suboptimal results.
  //
  // MADV_FREE (Linux): Makes our memory measurements less predictable;
  // performance benefits unclear.
  //
  // Therefore, we just do the simple thing: MADV_DONTNEED.
  PA_PCHECK(0 == madvise(ptr, length, MADV_DONTNEED));
#endif
#endif  // !BUILDFLAG(IS_NACL)
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_POSIX_H_
