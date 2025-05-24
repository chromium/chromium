// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/tagging.h"

#include <stdint.h>

#include "partition_alloc/aarch64_support.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
#include <arm_acle.h>
#include <asm/hwcap.h>
#include <sys/auxv.h>
#include <sys/ifunc.h>
#include <sys/prctl.h>
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_GET_TAGGED_ADDR_CTRL 56
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)

#if PA_BUILDFLAG(IS_LINUX)
#include <linux/version.h>

// Linux headers already provide these since v5.10.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#define HAS_PR_MTE_MACROS
#endif
#endif

#ifndef HAS_PR_MTE_MACROS
#define PR_MTE_TCF_SHIFT 1
#define PR_MTE_TCF_NONE (0UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_SYNC (1UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_ASYNC (2UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_MASK (3UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TAG_SHIFT 3
#define PR_MTE_TAG_MASK (0xffffUL << PR_MTE_TAG_SHIFT)
#define HWCAP2_MTE (1 << 18)
#endif
#endif

#if PA_BUILDFLAG(IS_ANDROID)
#include "partition_alloc/partition_alloc_base/files/file_path.h"
#include "partition_alloc/partition_alloc_base/native_library.h"
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
#include <malloc.h>
#endif  // BUILDFLAGS(HAS_MEMORY_TAGGING)
#endif  // PA_BUILDFLAG(IS_ANDROID)

namespace partition_alloc {
void ChangeMemoryTaggingModeForCurrentThreadNoOp(TagViolationReportingMode m) {}

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
using ChangeMemoryTaggingModeForCurrentThreadInternalFn =
    void(TagViolationReportingMode);

void ChangeMemoryTaggingModeForCurrentThreadForMte(
    TagViolationReportingMode m) {
  const auto change_memory_tagging_mode = [](unsigned long prctl_mask) {
    const int status = prctl(PR_SET_TAGGED_ADDR_CTRL, prctl_mask, 0, 0, 0);
    PA_CHECK(status == 0);
  };

  if (m == TagViolationReportingMode::kSynchronous) {
    change_memory_tagging_mode(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC |
                               (0xfffe << PR_MTE_TAG_SHIFT));
  } else if (m == TagViolationReportingMode::kAsynchronous) {
    change_memory_tagging_mode(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC |
                               (0xfffe << PR_MTE_TAG_SHIFT));
  } else {
    change_memory_tagging_mode(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_NONE);
  }
}

extern "C" ChangeMemoryTaggingModeForCurrentThreadInternalFn*
ResolveChangeMemoryTaggingModeForCurrentThread(uint64_t hwcap,
                                               struct __ifunc_arg_t* hw) {
  return internal::IsMteEnabled(hwcap, hw)
             ? &ChangeMemoryTaggingModeForCurrentThreadForMte
             : &ChangeMemoryTaggingModeForCurrentThreadNoOp;
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

void ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode m)
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    __attribute__((ifunc("ResolveChangeMemoryTaggingModeForCurrentThread")));
#else
{
  ChangeMemoryTaggingModeForCurrentThreadNoOp(m);
}
#endif

namespace internal {

#if PA_BUILDFLAG(IS_ANDROID)
bool ChangeMemoryTaggingModeForAllThreadsPerProcess(
    TagViolationReportingMode m) {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  // In order to support Android NDK API level below 26, we need to call
  // mallopt via dynamic linker.
  // int mallopt(int param, int value);
  using MalloptSignature = int (*)(int, int);

  static MalloptSignature mallopt_fnptr = [] {
    base::FilePath module_path;
    base::NativeLibraryLoadError load_error;
    base::FilePath library_path = module_path.Append("libc.so");
    base::NativeLibrary library =
        base::LoadNativeLibrary(library_path, &load_error);
    PA_CHECK(library);
    void* func_ptr =
        base::GetFunctionPointerFromNativeLibrary(library, "mallopt");
    PA_CHECK(func_ptr);
    return reinterpret_cast<MalloptSignature>(func_ptr);
  }();

  int status = 0;
  if (m == TagViolationReportingMode::kSynchronous) {
    status = mallopt_fnptr(M_BIONIC_SET_HEAP_TAGGING_LEVEL,
                           M_HEAP_TAGGING_LEVEL_SYNC);
  } else if (m == TagViolationReportingMode::kAsynchronous) {
    status = mallopt_fnptr(M_BIONIC_SET_HEAP_TAGGING_LEVEL,
                           M_HEAP_TAGGING_LEVEL_ASYNC);
  } else {
    status = mallopt_fnptr(M_BIONIC_SET_HEAP_TAGGING_LEVEL,
                           M_HEAP_TAGGING_LEVEL_NONE);
  }
  return status != 0;
#else
  return false;
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
}
#endif  // PA_BUILDFLAG(IS_ANDROID)

namespace {
[[maybe_unused]] static bool CheckTagRegionParameters(void* ptr, size_t sz) {
  // Check that ptr and size are correct for MTE
  uintptr_t ptr_as_uint = reinterpret_cast<uintptr_t>(ptr);
  bool ret = (ptr_as_uint % kMemTagGranuleSize == 0) &&
             (sz % kMemTagGranuleSize == 0) && sz;
  return ret;
}

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
void* TagRegionRandomlyForMTE(void* ptr, size_t sz, uint64_t mask) {
  // Randomly tag a region (MTE-enabled systems only). The first 16-byte
  // granule is randomly tagged, all other granules in the region are
  // then assigned that initial tag via __arm_mte_set_tag.
  if (!CheckTagRegionParameters(ptr, sz)) {
    return nullptr;
  }
  // __arm_mte_create_random_tag generates a randomly tagged pointer via the
  // hardware's random number generator, but does not apply it to the memory.
  char* nptr = reinterpret_cast<char*>(__arm_mte_create_random_tag(ptr, mask));
  for (size_t i = 0; i < sz; i += kMemTagGranuleSize) {
    // Next, tag the first and all subsequent granules with the randomly tag.
    __arm_mte_set_tag(nptr +
                      i);  // Tag is taken from the top bits of the argument.
  }
  return nptr;
}

void* TagRegionIncrementForMTE(void* ptr, size_t sz) {
  // Increment a region's tag (MTE-enabled systems only), using the tag of the
  // first granule.
  if (!CheckTagRegionParameters(ptr, sz)) {
    return nullptr;
  }
  // Increment ptr's tag.
  char* nptr = reinterpret_cast<char*>(__arm_mte_increment_tag(ptr, 1u));
  for (size_t i = 0; i < sz; i += kMemTagGranuleSize) {
    // Apply the tag to the first granule, and all subsequent granules.
    __arm_mte_set_tag(nptr + i);
  }
  return nptr;
}

void* RemaskVoidPtrForMTE(void* ptr) {
  if (ptr) [[likely]] {
    // Can't look up the tag for a null ptr (segfaults).
    return __arm_mte_get_tag(ptr);
  }
  return nullptr;
}

void* TagRegionIncrementNoOp(void* ptr, size_t sz) {
  // Region parameters are checked even on non-MTE systems to check the
  // intrinsics are used correctly.
  return ptr;
}

void* TagRegionRandomlyNoOp(void* ptr, size_t sz, uint64_t mask) {
  // Verifies a 16-byte aligned tagging granule, size tagging granule (all
  // architectures).
  return ptr;
}

void* RemaskVoidPtrNoOp(void* ptr) {
  return ptr;
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

}  // namespace

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
using RemaskPtrInternalFn = void*(void* ptr);
using TagMemoryRangeIncrementInternalFn = void*(void* ptr, size_t size);
using TagMemoryRangeRandomlyInternalFn = void*(void* ptr,
                                               size_t size,
                                               uint64_t mask);

extern "C" TagMemoryRangeIncrementInternalFn* ResolveTagMemoryRangeIncrement(
    uint64_t hwcap,
    struct __ifunc_arg_t* hw) {
  if (IsMteEnabled(hwcap, hw)) {
    return TagRegionIncrementForMTE;
  }
  return TagRegionIncrementNoOp;
}

extern "C" TagMemoryRangeRandomlyInternalFn* ResolveTagMemoryRandomly(
    uint64_t hwcap,
    struct __ifunc_arg_t* hw) {
  if (IsMteEnabled(hwcap, hw)) {
    return TagRegionRandomlyForMTE;
  }
  return TagRegionRandomlyNoOp;
}

extern "C" RemaskPtrInternalFn* ResolveRemaskPointer(uint64_t hwcap,
                                                     struct __ifunc_arg_t* hw) {
  if (IsMteEnabled(hwcap, hw)) {
    return RemaskVoidPtrForMTE;
  }
  return RemaskVoidPtrNoOp;
}

void* TagMemoryRangeIncrementInternal(void* ptr, size_t size)
    __attribute__((ifunc("ResolveTagMemoryRangeIncrement")));
void* TagMemoryRangeRandomlyInternal(void* ptr, size_t size, uint64_t mask)
    __attribute__((ifunc("ResolveTagMemoryRandomly")));
void* RemaskPointerInternal(void* ptr)
    __attribute__((ifunc("ResolveRemaskPointer")));
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

TagViolationReportingMode GetMemoryTaggingModeForCurrentThreadNoMte() {
  return TagViolationReportingMode::kUndefined;
}

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
using GetMemoryTaggingModeForCurrentThreadInternalFn =
    TagViolationReportingMode();

TagViolationReportingMode GetMemoryTaggingModeForCurrentThreadMte() {
  const int status = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
  PA_CHECK(status >= 0);
  // Check for Asynchronous first because ASYNC on Android sets both
  // PR_MTE_TCF_ASYNC and PR_MTE_TCF_SYNC bits.
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_ASYNC)) {
    return TagViolationReportingMode::kAsynchronous;
  }
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_SYNC)) {
    return TagViolationReportingMode::kSynchronous;
  }
  return TagViolationReportingMode::kDisabled;
}

extern "C" GetMemoryTaggingModeForCurrentThreadInternalFn*
ResolveGetMemoryTaggingModeForCurrentThread(uint64_t hwcap,
                                            struct __ifunc_arg_t* hw) {
  return IsMteEnabled(hwcap, hw) ? &GetMemoryTaggingModeForCurrentThreadMte
                                 : &GetMemoryTaggingModeForCurrentThreadNoMte;
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

TagViolationReportingMode GetMemoryTaggingModeForCurrentThread()
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    __attribute__((ifunc("ResolveGetMemoryTaggingModeForCurrentThread")));
#else
{
  return GetMemoryTaggingModeForCurrentThreadNoMte();
}
#endif

}  // namespace internal

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING) && PA_BUILDFLAG(IS_ANDROID)
bool PermissiveMte::enabled_ = false;

// static
void PermissiveMte::SetEnabled(bool enabled) {
  PermissiveMte::enabled_ = enabled;
}

// static
bool PermissiveMte::HandleCrash(int signo,
                                siginfo_t* siginfo,
                                ucontext_t* context) {
  if (siginfo->si_signo == SIGSEGV &&
      (siginfo->si_code == SEGV_MTESERR || siginfo->si_code == SEGV_MTEAERR) &&
      PermissiveMte::enabled_) {
    // In MTE permissive mode, do not crash the process. Instead, disable MTE
    // and let the failing instruction be retried. The second time should
    // succeed (except if there is another non-MTE fault).
    internal::ChangeMemoryTaggingModeForAllThreadsPerProcess(
        partition_alloc::TagViolationReportingMode::kDisabled);
    return true;
  }
  return false;
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING) && PA_BUILDFLAG(IS_ANDROID)

SuspendTagCheckingScope::SuspendTagCheckingScope() noexcept {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  if (internal::base::CPU::GetInstanceNoAllocation().has_mte()) [[unlikely]] {
    asm volatile(
        R"(
        .arch_extension memtag
        mrs %0, tco
        msr tco, #1
        )"
        : "=r"(previous_tco_));
  }
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
}

SuspendTagCheckingScope::~SuspendTagCheckingScope() {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  if (internal::base::CPU::GetInstanceNoAllocation().has_mte()) [[unlikely]] {
    // Restore previous tco value.
    __asm__ __volatile__(
        R"(
        .arch_extension memtag
        msr tco, %0
        )"
        :
        : "r"(previous_tco_));
  }
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
}

}  // namespace partition_alloc
