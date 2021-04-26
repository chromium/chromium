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
#endif  // defined(LIBC_GLIBC)
  return nullptr;
}

#else  // defined(OS_WIN)
#error "Unsupported GetStackTop"
#endif  // defined(OS_WIN)

NOINLINE uintptr_t* GetStackPointer() {
  return reinterpret_cast<uintptr_t*>(__builtin_frame_address(0));
}

}  // namespace internal
}  // namespace base
