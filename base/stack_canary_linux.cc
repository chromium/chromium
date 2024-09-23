// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/stack_canary_linux.h"

#include <dlfcn.h>
#include <stdint.h>
#include <sys/mman.h>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/rand_util.h"
#include "build/build_config.h"

namespace base {

#if defined(LIBC_GLIBC)

#if defined(ARCH_CPU_ARM_FAMILY)
// On ARM, Glibc uses a global variable (exported) called __stack_chk_guard.
extern "C" {
extern uintptr_t __stack_chk_guard;
}
#endif  // defined(ARCH_CPU_ARM_FAMILY)

#if !defined(NDEBUG)
// In debug builds, if we detect stack smashing in old stack frames after
// changing the canary, it's nice to let someone know that it's because the
// canary changed and they should prevent their function from using stack
// canaries.
static bool g_emit_debug_message = false;

extern "C" {
typedef __attribute__((noreturn)) void(GLibcStackChkFailFunction)();

// This overrides glibc's version of __stack_chk_fail(), which is called when
// the canary doesn't match.
__attribute__((visibility("default"), noinline, noreturn)) void
__stack_chk_fail() {
  if (g_emit_debug_message) {
    RAW_LOG(
        FATAL,
        "Stack smashing detected. The canary was changed during runtime "
        "(see crbug.com/1206626). You may need to mark your function with "
        "the no_stack_protector attribute, or just exit() before stack "
        "smashing occurs. You can also disable this canary-changing feature "
        "by adding --change-stack-guard-on-fork=disable to the command line.");
  }

  // Call the real __stack_chk_fail().
  // Note that dlsym may not be safe to perform since this is called during
  // corruption, but this code purposely only runs in debug builds and in the
  // normal case might provide better debug information.
  GLibcStackChkFailFunction* glibc_stack_chk_fail =
      reinterpret_cast<GLibcStackChkFailFunction*>(
          dlsym(RTLD_NEXT, "__stack_chk_fail"));
  (*glibc_stack_chk_fail)();
}
}
#endif  // !defined(NDEBUG)

NO_STACK_PROTECTOR void ResetStackCanaryIfPossible() {
  uintptr_t canary;
  base::RandBytes(base::byte_span_from_ref(canary));
  // First byte should be the null byte for string functions.
  canary &= ~static_cast<uintptr_t>(0xff);

  // The x86/x64 offsets should work for musl too.
#if defined(ARCH_CPU_X86_64)
  asm volatile("movq %q0,%%fs:%P1" : : "er"(canary), "i"(0x28));
#elif defined(ARCH_CPU_X86)
  asm volatile("movl %0,%%gs:%P1" : : "ir"(canary), "i"(0x14));
#elif defined(ARCH_CPU_ARM_FAMILY)
  // ARM's stack canary is held on a relro page. So, we'll need to make the page
  // writable, change the stack canary, and then make the page ro again.
  // We want to be single-threaded when changing page permissions, since it's
  // reasonable for other threads to assume that page permissions for global
  // variables don't change.
  size_t page_size = base::GetPageSize();
  uintptr_t __stack_chk_guard_page = base::bits::AlignDown(
      reinterpret_cast<uintptr_t>(&__stack_chk_guard), page_size);
  PCHECK(0 == mprotect(reinterpret_cast<void*>(__stack_chk_guard_page),
                       page_size, PROT_READ | PROT_WRITE));
  __stack_chk_guard = canary;
  PCHECK(0 == mprotect(reinterpret_cast<void*>(__stack_chk_guard_page),
                       page_size, PROT_READ));
#endif
}

void SetStackSmashingEmitsDebugMessage() {
#if !defined(NDEBUG)
  g_emit_debug_message = true;
#endif  // !defined(NDEBUG)
}

#else  // defined(LIBC_GLIBC)

// We don't know how to reset the canary if not compiling for glibc.
void ResetStackCanaryIfPossible() {}

void SetStackSmashingEmitsDebugMessage() {}

#endif  // defined(LIBC_GLIBC)
}  // namespace base
