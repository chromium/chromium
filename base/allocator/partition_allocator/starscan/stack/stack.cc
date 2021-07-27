// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/stack/stack.h"

#include <cstdint>
#include <limits>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

#if defined(LIBC_GLIBC)
extern "C" void* __libc_stack_end;
#endif

namespace base {
namespace internal {

#if defined(OS_WIN)

void* GetStackTop() {
#if defined(ARCH_CPU_X86_64)
  return reinterpret_cast<void*>(
      reinterpret_cast<NT_TIB64*>(NtCurrentTeb())->StackBase);
#elif defined(ARCH_CPU_32_BITS)
  return reinterpret_cast<void*>(
      reinterpret_cast<NT_TIB*>(NtCurrentTeb())->StackBase);
#elif defined(ARCH_CPU_ARM64)
  // Windows 8 and later, see
  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits
  ULONG_PTR lowLimit, highLimit;
  ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
  return reinterpret_cast<void*>(highLimit);
#else
#error "Unsupported GetStackStart"
#endif
}

#elif defined(OS_MAC)

void* GetStackTop() {
  return pthread_get_stackaddr_np(pthread_self());
}

#elif defined(OS_POSIX) || defined(OS_FUCHSIA)

void* GetStackTop() {
  pthread_attr_t attr;
  int error = pthread_getattr_np(pthread_self(), &attr);
  if (!error) {
    void* base;
    size_t size;
    error = pthread_attr_getstack(&attr, &base, &size);
    PA_CHECK(!error);
    pthread_attr_destroy(&attr);
    return reinterpret_cast<uint8_t*>(base) + size;
  }

#if defined(LIBC_GLIBC)
  // pthread_getattr_np can fail for the main thread. In this case
  // just like NaCl we rely on the __libc_stack_end to give us
  // the start of the stack.
  // See https://code.google.com/p/nativeclient/issues/detail?id=3431.
  return __libc_stack_end;
#else
  return nullptr;
#endif  // defined(LIBC_GLIBC)
}

#else  // defined(OS_WIN)
#error "Unsupported GetStackTop"
#endif  // defined(OS_WIN)

using IterateStackCallback = void (*)(const Stack*, StackVisitor*, uintptr_t*);
extern "C" void PAPushAllRegistersAndIterateStack(const Stack*,
                                                  StackVisitor*,
                                                  IterateStackCallback);

Stack::Stack(void* stack_top) : stack_top_(stack_top) {
  PA_DCHECK(stack_top);
}

NOINLINE uintptr_t* GetStackPointer() {
  return reinterpret_cast<uintptr_t*>(__builtin_frame_address(0));
}

namespace {

ALLOW_UNUSED_TYPE
void IterateSafeStackIfNecessary(StackVisitor* visitor) {
#if defined(__has_feature)
#if __has_feature(safe_stack)
  // Source:
  // https://github.com/llvm/llvm-project/blob/main/compiler-rt/lib/safestack/safestack.cpp
  constexpr size_t kSafeStackAlignmentBytes = 16;
  void* stack_ptr = __builtin___get_unsafe_stack_ptr();
  void* stack_top = __builtin___get_unsafe_stack_top();
  PA_CHECK(stack_top > stack_ptr);
  PA_CHECK(0u == (reinterpret_cast<uintptr_t>(stack_ptr) &
                  (kSafeStackAlignmentBytes - 1)));
  PA_CHECK(0u == (reinterpret_cast<uintptr_t>(stack_top) &
                  (kSafeStackAlignmentBytes - 1)));
  visitor->VisitStack(reinterpret_cast<uintptr_t*>(stack_ptr),
                      reinterpret_cast<uintptr_t*>(stack_top));
#endif  // __has_feature(safe_stack)
#endif  // defined(__has_feature)
}

// Called by the trampoline that pushes registers on the stack. This method
// should never be inlined to ensure that a possible redzone cannot contain
// any data that needs to be scanned.
// No ASAN support as method accesses redzones while walking the stack.
NOINLINE NO_SANITIZE("address") ALLOW_UNUSED_TYPE
    void IteratePointersImpl(const Stack* stack,
                             StackVisitor* visitor,
                             uintptr_t* stack_ptr) {
  PA_DCHECK(stack);
  PA_DCHECK(visitor);
  PA_CHECK(nullptr != stack->stack_top());
  // All supported platforms should have their stack aligned to at least
  // sizeof(void*).
  constexpr size_t kMinStackAlignment = sizeof(void*);
  PA_CHECK(0u ==
           (reinterpret_cast<uintptr_t>(stack_ptr) & (kMinStackAlignment - 1)));
  visitor->VisitStack(stack_ptr,
                      reinterpret_cast<uintptr_t*>(stack->stack_top()));
}

}  // namespace

void Stack::IteratePointers(StackVisitor* visitor) const {
#if defined(PA_PCSCAN_STACK_SUPPORTED)
  PAPushAllRegistersAndIterateStack(this, visitor, &IteratePointersImpl);
  // No need to deal with callee-saved registers as they will be kept alive by
  // the regular conservative stack iteration.
  IterateSafeStackIfNecessary(visitor);
#endif
}

}  // namespace internal
}  // namespace base
