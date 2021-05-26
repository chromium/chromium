// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>

#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/profiler/thread_delegate_posix.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_ANDROID)
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#endif

namespace base {

namespace {

#if defined(OS_ANDROID)
absl::optional<uintptr_t> GetAndroidMainThreadStackBaseAddressImpl() {
  char line[1024];
  base::ScopedFILE fp(base::OpenFile(base::FilePath("/proc/self/maps"), "r"));
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(line);
  if (!fp)
    return absl::nullopt;
  while (fgets(line, sizeof(line), fp.get()) != nullptr) {
    uintptr_t start, end;
    if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR, &start, &end) == 2) {
      if (start <= stack_addr && stack_addr < end)
        return end;
    }
  }
  return absl::nullopt;
}
#endif

uintptr_t GetThreadStackBaseAddressImpl(
    SamplingProfilerThreadToken thread_token) {
  pthread_attr_t attr;
  pthread_getattr_np(thread_token.pthread_id, &attr);
  // See crbug.com/617730 for limitations of this approach on Linux.
  void* address;
  size_t size;
  pthread_attr_getstack(&attr, &address, &size);
  pthread_attr_destroy(&attr);
  const uintptr_t base_address = reinterpret_cast<uintptr_t>(address) + size;
  return base_address;
}

absl::optional<uintptr_t> GetThreadStackBaseAddress(
    SamplingProfilerThreadToken thread_token) {
#if defined(OS_ANDROID)
  // The implementation of pthread_getattr_np() in Bionic reads proc/self/maps
  // to find the main thread base address, and throws SIGABRT when it fails to
  // read or parse the file. So, try to read the maps to get the main thread
  // stack base and cache the result. Other thread base addresses are sourced
  // from pthread state so are cheap to get.
  const bool is_main_thread = thread_token.id == GetCurrentProcId();
  if (is_main_thread) {
    static const absl::optional<uintptr_t> main_thread_base_address =
        GetAndroidMainThreadStackBaseAddressImpl();
    return main_thread_base_address;
  }
#endif
  return GetThreadStackBaseAddressImpl(thread_token);
}

}  // namespace

// static
std::unique_ptr<ThreadDelegatePosix> ThreadDelegatePosix::Create(
    SamplingProfilerThreadToken thread_token) {
  absl::optional<uintptr_t> base_address =
      GetThreadStackBaseAddress(thread_token);
  if (!base_address)
    return nullptr;
  return base::WrapUnique(
      new ThreadDelegatePosix(thread_token.id, *base_address));
}

ThreadDelegatePosix::~ThreadDelegatePosix() = default;

PlatformThreadId ThreadDelegatePosix::GetThreadId() const {
  return thread_id_;
}

uintptr_t ThreadDelegatePosix::GetStackBaseAddress() const {
  return thread_stack_base_address_;
}

std::vector<uintptr_t*> ThreadDelegatePosix::GetRegistersToRewrite(
    RegisterContext* thread_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  return {
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r0),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r1),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r2),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r3),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r4),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r5),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r6),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r7),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r8),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r9),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_r10),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_fp),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_ip),
      reinterpret_cast<uintptr_t*>(&thread_context->arm_sp),
      // arm_lr and arm_pc do not require rewriting because they contain
      // addresses of executable code, not addresses in the stack.
  };
#elif defined(ARCH_CPU_ARM_FAMILY) && \
    defined(ARCH_CPU_64_BITS)   // #if defined(ARCH_CPU_ARM_FAMILY) &&
                                // defined(ARCH_CPU_32_BITS)
  std::vector<uintptr_t*> registers;
  registers.reserve(12);
  // Return the set of callee-save registers per the ARM 64-bit Procedure Call
  // Standard section 5.1.1, plus the stack pointer.
  registers.push_back(reinterpret_cast<uintptr_t*>(&thread_context->sp));
  for (size_t i = 19; i <= 29; ++i)
    registers.push_back(reinterpret_cast<uintptr_t*>(&thread_context->regs[i]));
  return registers;
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_32_BITS)
  return {
      // Return the set of callee-save registers per the i386 System V ABI
      // section 2.2.3, plus the stack pointer.
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_EBX]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_EBP]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_ESI]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_EDI]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_ESP]),
  };
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_64_BITS)
  return {
      // Return the set of callee-save registers per the x86-64 System V ABI
      // section 3.2.1, plus the stack pointer.
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_RBP]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_RBX]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_R12]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_R13]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_R14]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_R15]),
      reinterpret_cast<uintptr_t*>(&thread_context->gregs[REG_RSP]),
  };
#else  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  // Unimplemented for other architectures.
  return {};
#endif
}

ThreadDelegatePosix::ThreadDelegatePosix(PlatformThreadId id,
                                         uintptr_t base_address)
    : thread_id_(id), thread_stack_base_address_(base_address) {}

}  // namespace base
