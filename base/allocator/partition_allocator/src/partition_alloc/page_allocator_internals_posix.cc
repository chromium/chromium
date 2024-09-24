// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdint.h>
#include <sys/mman.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/notreached.h"

#if PA_BUILDFLAG(IS_APPLE)
#include "partition_alloc/partition_alloc_base/apple/foundation_util.h"
#if PA_BUILDFLAG(IS_IOS)
#include "partition_alloc/partition_alloc_base/ios/ios_util.h"
#elif PA_BUILDFLAG(IS_MAC)
#include "partition_alloc/partition_alloc_base/mac/mac_util.h"
#else
#error "Unknown platform"
#endif
#include <Availability.h>
#include <Security/Security.h>
#include <mach/mach.h>

#include "partition_alloc/partition_alloc_base/apple/scoped_cftyperef.h"
#endif

#if PA_BUILDFLAG(IS_MAC)

// SecTaskGetCodeSignStatus is marked as unavailable on macOS, although it’s
// available on iOS and other Apple operating systems. It is, in fact, present
// on the system since macOS 10.12.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wavailability"
uint32_t SecTaskGetCodeSignStatus(SecTaskRef task) API_AVAILABLE(macos(10.12));
#pragma clang diagnostic pop

#endif  // PA_BUILDFLAG(IS_MAC)

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING) || \
    (defined(__ARM_FEATURE_BTI_DEFAULT) && (__ARM_FEATURE_BTI_DEFAULT == 1))
struct __ifunc_arg_t;

#include "partition_alloc/aarch64_support.h"

#define NEEDS_HANDLING_OF_HW_CAPABILITIES
#endif

// PA_PROT_BTI requests a page that supports BTI landing pads.
#define PA_PROT_BTI 0x10

// PA_PROT_MTE requests a page that's suitable for memory tagging.
#define PA_PROT_MTE 0x20

namespace partition_alloc::internal {
namespace {

int GetAccessFlags(PageAccessibilityConfiguration accessibility,
                   bool mte_enabled,
                   bool bti_enabled) {
  switch (accessibility.permissions) {
    case PageAccessibilityConfiguration::kRead:
      return PROT_READ;
    case PageAccessibilityConfiguration::kReadWriteTagged:
      return PROT_READ | PROT_WRITE | (mte_enabled ? PA_PROT_MTE : 0);
    case PageAccessibilityConfiguration::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageAccessibilityConfiguration::kReadExecuteProtected:
      return PROT_READ | PROT_EXEC | (bti_enabled ? PA_PROT_BTI : 0);
    case PageAccessibilityConfiguration::kReadExecute:
      return PROT_READ | PROT_EXEC;
    case PageAccessibilityConfiguration::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    case PageAccessibilityConfiguration::kReadWriteExecuteProtected:
      return PROT_READ | PROT_WRITE | PROT_EXEC |
             (bti_enabled ? PA_PROT_BTI : 0);
    case PageAccessibilityConfiguration::kInaccessible:
    case PageAccessibilityConfiguration::kInaccessibleWillJitLater:
      return PROT_NONE;
  }
}

template <bool MteEnabled, bool BtiEnabled>
int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  return GetAccessFlags(accessibility, MteEnabled, BtiEnabled);
}

}  // namespace

#if defined(NEEDS_HANDLING_OF_HW_CAPABILITIES)
using GetAccessFlagsInternalFn = int(PageAccessibilityConfiguration);

extern "C" GetAccessFlagsInternalFn* ResolveGetAccessFlags(
    uint64_t hwcap,
    struct __ifunc_arg_t* hw) {
  if (IsMteEnabled(hwcap, hw)) {
    if (IsBtiEnabled(hwcap, hw)) {
      return GetAccessFlags<true, true>;
    } else {
      return GetAccessFlags<true, false>;
    }
  } else {
    if (IsBtiEnabled(hwcap, hw)) {
      return GetAccessFlags<false, true>;
    } else {
      return GetAccessFlags<false, false>;
    }
  }
}
#endif

// Resolve the implementation for GetAccessFlags using an iFunc.
int GetAccessFlags(PageAccessibilityConfiguration accessibility)
#if defined(NEEDS_HANDLING_OF_HW_CAPABILITIES)
    __attribute__((ifunc("ResolveGetAccessFlags")));
#else
{
  return GetAccessFlags<false, false>(accessibility);
}
#endif

#if defined(LINUX_NAME_REGION)

void NameRegion(void* start, size_t length, PageTag page_tag) {
  // Important: All the names should be string literals. As per prctl.h in
  // //third_party/android_toolchain/ndk the kernel keeps a pointer to the name
  // instead of copying it.
  //
  // Having the name in .rodata ensures that the pointer remains valid as
  // long as the mapping is alive.
  const char* name = nullptr;
  switch (page_tag) {
    case PageTag::kSimulation:
      name = "simulation";
      break;
    case PageTag::kBlinkGC:
      name = "blink_gc";
      break;
    case PageTag::kPartitionAlloc:
      name = "partition_alloc";
      break;
    case PageTag::kChromium:
      name = "chromium";
      break;
    case PageTag::kV8:
      name = "v8";
      break;
    default:
      PA_NOTREACHED();
  }

  // No error checking on purpose, testing only.
  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, start, length, name);
}

#endif  // defined(LINUX_NAME_REGION)

#if PA_BUILDFLAG(IS_MAC)
// Tests whether the version of macOS supports the MAP_JIT flag and if the
// current process is signed with the hardened runtime and the allow-jit
// entitlement, returning whether MAP_JIT should be used to allocate regions
// that will contain JIT-compiled executable code.
bool UseMapJit() {
  // Until determining that the hardened runtime is enabled, early returns will
  // return true, so that MAP_JIT will be used. This is important on arm64,
  // which only allows pages to be simultaneously writable and executable when
  // in a region allocated with MAP_JIT, regardless of code signing options. On
  // arm64, an attempt to set a non-MAP_JIT page as simultaneously writable and
  // executable fails with EPERM. Although this is not enforced on x86_64,
  // MAP_JIT is harmless in that case.

  base::apple::ScopedCFTypeRef<SecTaskRef> task(
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

  base::apple::ScopedCFTypeRef<CFTypeRef> jit_entitlement(
      SecTaskCopyValueForEntitlement(
          task.get(), CFSTR("com.apple.security.cs.allow-jit"), nullptr));
  if (!jit_entitlement) {
    return false;
  }

  return base::apple::CFCast<CFBooleanRef>(jit_entitlement.get()) ==
         kCFBooleanTrue;
}
#elif PA_BUILDFLAG(IS_IOS)
bool UseMapJit() {
// Always enable MAP_JIT in simulator as it is supported unconditionally.
#if TARGET_IPHONE_SIMULATOR
  return true;
#else
  // TODO(crbug.com/40255826): Fill this out when the API it is
  // available.
  return false;
#endif  // TARGET_IPHONE_SIMULATOR
}
#endif  // PA_BUILDFLAG(IS_IOS)

}  // namespace partition_alloc::internal
