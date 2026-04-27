// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_
#define BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace base::subtle {

#if DCHECK_IS_ON()
// Returns addresses of locks acquired by the current thread with
// `subtle::LockTracking::kEnabled`. `uintptr_t` is used because addresses are
// meant to be used as unique identifiers but not to be dereferenced.
BASE_EXPORT span<const uintptr_t> GetTrackedLocksHeldByCurrentThread();
#endif

// Whether to add a lock to the list returned by
// `subtle::GetLocksHeldByCurrentThread()` upon acquisition. This has no effect
// in non-DCHECK builds because tracking is always disabled. This is disabled by
// default to avoid exceeding the fixed-size storage backing
// `GetTrackedLocksHeldByCurrentThread()` and to avoid reentrancy, e.g.:
//
//     thread_local implementation
//     Add lock to the thread_local array of locks held by current thread
//     base::Lock::Acquire from allocator shim
//     ... Allocator shim ...
//     thread_local implementation
//     Access to a thread_local variable
//
// A lock acquired with `subtle::LockTracking::kEnabled` can be used to provide
// a mutual exclusion guarantee for SEQUENCE_CHECKER.
enum class LockTracking {
  kDisabled,
  kEnabled,
};

// YieldProcessor() wraps an architecture specific-instruction that informs the
// processor the thread in a busy wait, which can reduce power consumption and
// improve performance.
static inline void YieldProcessor() {
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_X86)
  __asm__ __volatile__("pause");
#elif (defined(ARCH_CPU_ARMEL) && __ARM_ARCH >= 6) || defined(ARCH_CPU_ARM64)
  __asm__ __volatile__("yield");
#elif defined(ARCH_CPU_MIPSEL)
  // The MIPS32 docs state that the PAUSE instruction is a no-op on older
  // architectures (first added in MIPS32r2). To avoid assembler errors when
  // targeting pre-r2, we must encode the instruction manually.
  __asm__ __volatile__(".word 0x00000140");
#elif defined(ARCH_CPU_MIPS64EL) && __mips_isa_rev >= 2
  // Don't bother doing using .word here since r2 is the lowest supported mips64
  // that Chromium supports.
  __asm__ __volatile__("pause");
#elif defined(ARCH_CPU_PPC64_FAMILY)
  __asm__ __volatile__("or 31,31,31");
#elif defined(ARCH_CPU_RISCV64)
  // Zihintpause extension provides a pause instruction but that extension
  // is not included in the current rv64gc baseline. However, the pause
  // instruction is encoded as a hint. Thus on CPUs without Zihintpause
  // extension, the pause instruction is treated like a nop.
  // Manually encode the instruction to support older toolchains.
  // See also https://sourceware.org/pipermail/libc-alpha/2024-June/157737.html
  __asm__ __volatile__(".insn i 0x0f, 0, x0, x0, 0x010");
#else
#error "Unsupported architecture for YieldProcessor()"
#endif  // ARCH
}

}  // namespace base::subtle

#endif  // BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_
