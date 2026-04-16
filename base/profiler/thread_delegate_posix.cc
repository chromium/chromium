// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_delegate_posix.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>

#include <optional>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#include "base/profiler/stack_base_address_posix.h"
#endif

namespace base {
// static
std::unique_ptr<ThreadDelegatePosix> ThreadDelegatePosix::Create(
    SamplingProfilerThreadToken thread_token) {
  std::optional<uintptr_t> base_address;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base_address = thread_token.stack_base_address;
#else
  base_address =
      GetThreadStackBaseAddress(thread_token.id, thread_token.pthread_id);
#endif
  if (!base_address) {
    return nullptr;
  }
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

std::vector<uintptr_t> ThreadDelegatePosix::GetRegisters(
    RegisterContext* thread_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  return {
      static_cast<uintptr_t>(thread_context->arm_r0),
      static_cast<uintptr_t>(thread_context->arm_r1),
      static_cast<uintptr_t>(thread_context->arm_r2),
      static_cast<uintptr_t>(thread_context->arm_r3),
      static_cast<uintptr_t>(thread_context->arm_r4),
      static_cast<uintptr_t>(thread_context->arm_r5),
      static_cast<uintptr_t>(thread_context->arm_r6),
      static_cast<uintptr_t>(thread_context->arm_r7),
      static_cast<uintptr_t>(thread_context->arm_r8),
      static_cast<uintptr_t>(thread_context->arm_r9),
      static_cast<uintptr_t>(thread_context->arm_r10),
      static_cast<uintptr_t>(thread_context->arm_fp),
      static_cast<uintptr_t>(thread_context->arm_ip),
      static_cast<uintptr_t>(thread_context->arm_sp),
      // arm_lr and arm_pc do not require rewriting because they contain
      // addresses of executable code, not addresses in the stack.
  };
#elif defined(ARCH_CPU_ARM_FAMILY) && \
    defined(ARCH_CPU_64_BITS)  // #if defined(ARCH_CPU_ARM_FAMILY) &&
                               // defined(ARCH_CPU_32_BITS)
  return {
      // Return the set of callee-save registers per the ARM 64-bit Procedure
      // Call
      // Standard section 5.1.1, plus the stack pointer.
      static_cast<uintptr_t>(thread_context->sp),
      static_cast<uintptr_t>(thread_context->regs[19]),
      static_cast<uintptr_t>(thread_context->regs[20]),
      static_cast<uintptr_t>(thread_context->regs[21]),
      static_cast<uintptr_t>(thread_context->regs[22]),
      static_cast<uintptr_t>(thread_context->regs[23]),
      static_cast<uintptr_t>(thread_context->regs[24]),
      static_cast<uintptr_t>(thread_context->regs[25]),
      static_cast<uintptr_t>(thread_context->regs[26]),
      static_cast<uintptr_t>(thread_context->regs[27]),
      static_cast<uintptr_t>(thread_context->regs[28]),
      static_cast<uintptr_t>(thread_context->regs[29]),
  };
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_32_BITS)
  return {
      // Return the set of callee-save registers per the i386 System V ABI
      // section 2.2.3, plus the stack pointer.
      static_cast<uintptr_t>(thread_context->gregs[REG_EBX]),
      static_cast<uintptr_t>(thread_context->gregs[REG_EBP]),
      static_cast<uintptr_t>(thread_context->gregs[REG_ESI]),
      static_cast<uintptr_t>(thread_context->gregs[REG_EDI]),
      static_cast<uintptr_t>(thread_context->gregs[REG_ESP]),
  };
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_64_BITS)
  return {
      // Return the set of callee-save registers per the x86-64 System V ABI
      // section 3.2.1, plus the stack pointer.
      static_cast<uintptr_t>(thread_context->gregs[REG_RBP]),
      static_cast<uintptr_t>(thread_context->gregs[REG_RBX]),
      static_cast<uintptr_t>(thread_context->gregs[REG_R12]),
      static_cast<uintptr_t>(thread_context->gregs[REG_R13]),
      static_cast<uintptr_t>(thread_context->gregs[REG_R14]),
      static_cast<uintptr_t>(thread_context->gregs[REG_R15]),
      static_cast<uintptr_t>(thread_context->gregs[REG_RSP]),
  };
#else  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  // Unimplemented for other architectures.
  return {};
#endif
}

void ThreadDelegatePosix::SetRegisters(
    RegisterContext* thread_context,
    const std::vector<uintptr_t>& registers) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  CHECK_EQ(registers.size(), 14u);
  thread_context->arm_r0 = registers[0];
  thread_context->arm_r1 = registers[1];
  thread_context->arm_r2 = registers[2];
  thread_context->arm_r3 = registers[3];
  thread_context->arm_r4 = registers[4];
  thread_context->arm_r5 = registers[5];
  thread_context->arm_r6 = registers[6];
  thread_context->arm_r7 = registers[7];
  thread_context->arm_r8 = registers[8];
  thread_context->arm_r9 = registers[9];
  thread_context->arm_r10 = registers[10];
  thread_context->arm_fp = registers[11];
  thread_context->arm_ip = registers[12];
  thread_context->arm_sp = registers[13];
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
  CHECK_EQ(registers.size(), 12u);
  thread_context->sp = registers[0];
  thread_context->regs[19] = registers[1];
  thread_context->regs[20] = registers[2];
  thread_context->regs[21] = registers[3];
  thread_context->regs[22] = registers[4];
  thread_context->regs[23] = registers[5];
  thread_context->regs[24] = registers[6];
  thread_context->regs[25] = registers[7];
  thread_context->regs[26] = registers[8];
  thread_context->regs[27] = registers[9];
  thread_context->regs[28] = registers[10];
  thread_context->regs[29] = registers[11];
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_32_BITS)
  CHECK_EQ(registers.size(), 5u);
  thread_context->gregs[REG_EBX] = static_cast<intptr_t>(registers[0]);
  thread_context->gregs[REG_EBP] = static_cast<intptr_t>(registers[1]);
  thread_context->gregs[REG_ESI] = static_cast<intptr_t>(registers[2]);
  thread_context->gregs[REG_EDI] = static_cast<intptr_t>(registers[3]);
  thread_context->gregs[REG_ESP] = static_cast<intptr_t>(registers[4]);
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_64_BITS)
  CHECK_EQ(registers.size(), 7u);
  thread_context->gregs[REG_RBP] = static_cast<intptr_t>(registers[0]);
  thread_context->gregs[REG_RBX] = static_cast<intptr_t>(registers[1]);
  thread_context->gregs[REG_R12] = static_cast<intptr_t>(registers[2]);
  thread_context->gregs[REG_R13] = static_cast<intptr_t>(registers[3]);
  thread_context->gregs[REG_R14] = static_cast<intptr_t>(registers[4]);
  thread_context->gregs[REG_R15] = static_cast<intptr_t>(registers[5]);
  thread_context->gregs[REG_RSP] = static_cast<intptr_t>(registers[6]);
#endif
}

ThreadDelegatePosix::ThreadDelegatePosix(PlatformThreadId id,
                                         uintptr_t base_address)
    : thread_id_(id), thread_stack_base_address_(base_address) {}

}  // namespace base
