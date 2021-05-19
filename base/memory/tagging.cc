// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/tagging.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(__ARM_FEATURE_MEMORY_TAGGING) && defined(ARCH_CPU_ARM64) && \
    (defined(OS_LINUX) || defined(OS_ANDROID))
#define HAS_MEMORY_TAGGING 1
#include <sys/auxv.h>
#include <sys/prctl.h>
#define HWCAP2_MTE (1 << 19)
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_GET_TAGGED_ADDR_CTRL 56
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)
#define PR_MTE_TCF_SHIFT 1
#define PR_MTE_TCF_NONE (0UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_SYNC (1UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_ASYNC (2UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_MASK (3UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TAG_SHIFT 3
#define PR_MTE_TAG_MASK (0xffffUL << PR_MTE_TAG_SHIFT)
#endif

#if defined(OS_ANDROID)
#include "base/native_library.h"
#define M_BIONIC_SET_HEAP_TAGGING_LEVEL (-204)

/**
 * Constants for use with the M_BIONIC_SET_HEAP_TAGGING_LEVEL mallopt() option.
 * These come from Android's platform bionic/libc/include/malloc.h
 */
enum HeapTaggingLevel {
  /**
   * Disable heap tagging and memory tag checks (if supported).
   * Heap tagging may not be re-enabled after being disabled.
   */
  M_HEAP_TAGGING_LEVEL_NONE = 0,
  /**
   * Address-only tagging. Heap pointers have a non-zero tag in the
   * most significant ("top") byte which is checked in free(). Memory
   * accesses ignore the tag using arm64's Top Byte Ignore (TBI) feature.
   */
  M_HEAP_TAGGING_LEVEL_TBI = 1,
  /**
   * Enable heap tagging and asynchronous memory tag checks (if supported).
   * Disable stack trace collection.
   */
  M_HEAP_TAGGING_LEVEL_ASYNC = 2,
  /**
   * Enable heap tagging and synchronous memory tag checks (if supported).
   * Enable stack trace collection.
   */
  M_HEAP_TAGGING_LEVEL_SYNC = 3,
};
#endif  // defined(OS_ANDROID)

namespace base {
namespace memory {

#if defined(OS_ANDROID)
void ChangeMemoryTaggingModeForAllThreadsPerProcess(
    TagViolationReportingMode m) {
#if defined(HAS_MEMORY_TAGGING)
  // In order to support Android NDK API level below 26, we need to call
  // mallopt via dynamic linker.
  // int mallopt(int param, int value);
  using MalloptSignature = int (*)(int, int);

  static MalloptSignature mallopt_fnptr = []() {
    FilePath module_path;
    NativeLibraryLoadError load_error;
    FilePath library_path = module_path.Append("libc.so");
    NativeLibrary library = LoadNativeLibrary(library_path, &load_error);
    if (!library) {
      LOG(FATAL) << "ChangeMemoryTaggingModeForAllThreadsPerProcess: dlsym "
                    "mallopt failure"
                 << load_error.ToString();
    }
    void* func_ptr = GetFunctionPointerFromNativeLibrary(library, "mallopt");
    if (func_ptr == nullptr) {
      LOG(FATAL) << "ChangeMemoryTaggingModeForAllThreadsPerProcess: dlsym "
                    "mallopt failure";
    }
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
  if (!status) {
    LOG(FATAL)
        << "ChangeMemoryTaggingModeForAllThreadsPerProcess: mallopt failure";
  }
#endif  // defined(HAS_MEMORY_TAGGING)
}
#else
#if defined(HAS_MEMORY_TAGGING)
namespace {
void ChangeMemoryTaggingModeInternal(unsigned prctl_mask) {
  base::CPU cpu;
  if (cpu.has_mte()) {
    int status = prctl(PR_SET_TAGGED_ADDR_CTRL, prctl_mask, 0, 0, 0);
    if (status != 0) {
      LOG(FATAL) << "ChangeMemoryTaggingModeInternal: prctl failure";
    }
  }
}
}  // namespace
#endif  // defined(HAS_MEMORY_TAGGING)

void ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode m) {
#if defined(HAS_MEMORY_TAGGING)
  if (m == TagViolationReportingMode::kSynchronous) {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC |
                                    (0xfffe << PR_MTE_TAG_SHIFT));
  } else if (m == TagViolationReportingMode::kAsynchronous) {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC |
                                    (0xfffe << PR_MTE_TAG_SHIFT));
  } else {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_NONE);
  }
#endif  // defined(HAS_MEMORY_TAGGING)
}
#endif  // defined(OS_ANDROID)

TagViolationReportingMode GetMemoryTaggingModeForCurrentThread() {
#if defined(HAS_MEMORY_TAGGING)
  base::CPU cpu;
  if (!cpu.has_mte()) {
    return TagViolationReportingMode::kUndefined;
  }
  int status = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
  if (status < 0) {
    LOG(FATAL) << "GetMemoryTaggingModeForCurrentThread: prctl failure";
  }
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_SYNC)) {
    return TagViolationReportingMode::kSynchronous;
  }
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_ASYNC)) {
    return TagViolationReportingMode::kAsynchronous;
  }
#endif  // defined(HAS_MEMORY_TAGGING)
  return TagViolationReportingMode::kUndefined;
}

}  // namespace memory
}  // namespace base
