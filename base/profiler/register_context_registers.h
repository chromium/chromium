// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides functions that provide access to key registers in the
// native register context.

#ifndef BASE_PROFILER_REGISTER_CONTEXT_REGISTERS_H_
#define BASE_PROFILER_REGISTER_CONTEXT_REGISTERS_H_

#include <ptrauth.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/ucontext.h>
#elif BUILDFLAG(IS_APPLE)
#include <mach/machine/thread_status.h>
#else
#include "base/profiler/register_context.h"
#endif

namespace base {

// Helper function to account for the fact that platform-specific register state
// types may be of the same size as uintptr_t, but not of the same type or
// signedness -- e.g. unsigned int vs. unsigned long on 32-bit Windows, unsigned
// long vs. unsigned long long on Mac, long long vs. unsigned long long on
// Linux.
template <typename T>
uintptr_t& AsUintPtr(T* value) {
  static_assert(sizeof(T) == sizeof(uintptr_t),
                "register state type must be of equivalent size to uintptr_t");
  return *reinterpret_cast<uintptr_t*>(value);
}

#if BUILDFLAG(IS_WIN)

inline uintptr_t RegisterContextStackPointer(::CONTEXT* context) {
#if defined(ARCH_CPU_X86_64)
  return context->Rsp;
#elif defined(ARCH_CPU_ARM64)
  return context->Sp;
#elif defined(ARCH_CPU_X86)
  return context->Esp;
#else
#error "Unknown architecture"
#endif
}

inline void SetRegisterContextStackPointer(::CONTEXT* context, uintptr_t val) {
#if defined(ARCH_CPU_X86_64)
  context->Rsp = val;
#elif defined(ARCH_CPU_ARM64)
  context->Sp = val;
#elif defined(ARCH_CPU_X86)
  context->Esp = val;
#else
#error "Unknown architecture"
#endif
}

inline uintptr_t RegisterContextFramePointer(::CONTEXT* context) {
#if defined(ARCH_CPU_X86_64)
  return context->Rbp;
#elif defined(ARCH_CPU_ARM64)
  return context->Fp;
#elif defined(ARCH_CPU_X86)
  return context->Ebp;
#else
#error "Unknown architecture"
#endif
}

inline void SetRegisterContextFramePointer(::CONTEXT* context, uintptr_t val) {
#if defined(ARCH_CPU_X86_64)
  context->Rbp = val;
#elif defined(ARCH_CPU_ARM64)
  context->Fp = val;
#elif defined(ARCH_CPU_X86)
  context->Ebp = val;
#else
#error "Unknown architecture"
#endif
}

inline uintptr_t RegisterContextInstructionPointer(::CONTEXT* context) {
#if defined(ARCH_CPU_X86_64)
  return context->Rip;
#elif defined(ARCH_CPU_ARM64)
  return context->Pc;
#elif defined(ARCH_CPU_X86)
  return context->Eip;
#else
#error "Unknown architecture"
#endif
}

inline void SetRegisterContextInstructionPointer(::CONTEXT* context,
                                                 uintptr_t val) {
#if defined(ARCH_CPU_X86_64)
  context->Rip = val;
#elif defined(ARCH_CPU_ARM64)
  context->Pc = val;
#elif defined(ARCH_CPU_X86)
  context->Eip = val;
#else
#error "Unknown architecture"
#endif
}

#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

inline uintptr_t RegisterContextStackPointer(mcontext_t* context) {
#if defined(ARCH_CPU_ARMEL)
  return context->arm_sp;
#elif defined(ARCH_CPU_ARM64)
  return context->sp;
#elif defined(ARCH_CPU_X86)
  return AsUintPtr(&context->gregs[REG_ESP]);
#elif defined(ARCH_CPU_X86_64)
  return AsUintPtr(&context->gregs[REG_RSP]);
#else
  // The implementations here and below are placeholders for other POSIX
  // platforms that just return the first three register slots in the context.
  return *reinterpret_cast<uintptr_t*>(context);
#endif
}

inline void SetRegisterContextStackPointer(mcontext_t* context, uintptr_t val) {
#if defined(ARCH_CPU_ARMEL)
  context->arm_sp = val;
#elif defined(ARCH_CPU_ARM64)
  context->sp = val;
#elif defined(ARCH_CPU_X86)
  AsUintPtr(&context->gregs[REG_ESP]) = val;
#elif defined(ARCH_CPU_X86_64)
  AsUintPtr(&context->gregs[REG_RSP]) = val;
#else
  // The implementations here and below are placeholders for other POSIX
  // platforms that just the first three register slots in the context.
  *reinterpret_cast<uintptr_t*>(context) = val;
#endif
}

inline uintptr_t RegisterContextFramePointer(mcontext_t* context) {
#if defined(ARCH_CPU_ARMEL)
  return context->arm_fp;
#elif defined(ARCH_CPU_ARM64)
  // r29 is the FP register on 64-bit ARM per the Procedure Call Standard,
  // section 5.1.1.
  return context->regs[29];
#elif defined(ARCH_CPU_X86)
  return AsUintPtr(&context->gregs[REG_EBP]);
#elif defined(ARCH_CPU_X86_64)
  return AsUintPtr(&context->gregs[REG_RBP]);
#else
  return *(UNSAFE_TODO(reinterpret_cast<uintptr_t*>(context) + 1));
#endif
}

inline void SetRegisterContextFramePointer(mcontext_t* context, uintptr_t val) {
#if defined(ARCH_CPU_ARMEL)
  context->arm_fp = val;
#elif defined(ARCH_CPU_ARM64)
  // r29 is the FP register on 64-bit ARM per the Procedure Call Standard,
  // section 5.1.1.
  context->regs[29] = val;
#elif defined(ARCH_CPU_X86)
  AsUintPtr(&context->gregs[REG_EBP]) = val;
#elif defined(ARCH_CPU_X86_64)
  AsUintPtr(&context->gregs[REG_RBP]) = val;
#else
  *(UNSAFE_TODO(reinterpret_cast<uintptr_t*>(context) + 1)) = val;
#endif
}

inline uintptr_t RegisterContextInstructionPointer(mcontext_t* context) {
#if defined(ARCH_CPU_ARMEL)
  return context->arm_pc;
#elif defined(ARCH_CPU_ARM64)
  return context->pc;
#elif defined(ARCH_CPU_X86)
  return AsUintPtr(&context->gregs[REG_EIP]);
#elif defined(ARCH_CPU_X86_64)
  return AsUintPtr(&context->gregs[REG_RIP]);
#else
  return *(UNSAFE_TODO(reinterpret_cast<uintptr_t*>(context) + 2));
#endif
}

inline void SetRegisterContextInstructionPointer(mcontext_t* context,
                                                 uintptr_t val) {
#if defined(ARCH_CPU_ARMEL)
  context->arm_pc = val;
#elif defined(ARCH_CPU_ARM64)
  context->pc = val;
#elif defined(ARCH_CPU_X86)
  AsUintPtr(&context->gregs[REG_EIP]) = val;
#elif defined(ARCH_CPU_X86_64)
  AsUintPtr(&context->gregs[REG_RIP]) = val;
#else
  *(UNSAFE_TODO(reinterpret_cast<uintptr_t*>(context) + 2)) = val;
#endif
}

#elif BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_X86_64)

inline uintptr_t RegisterContextStackPointer(x86_thread_state64_t* context) {
  return context->__rsp;
}

inline void SetRegisterContextStackPointer(x86_thread_state64_t* context,
                                           uintptr_t val) {
  context->__rsp = val;
}

inline uintptr_t RegisterContextFramePointer(x86_thread_state64_t* context) {
  return context->__rbp;
}

inline void SetRegisterContextFramePointer(x86_thread_state64_t* context,
                                           uintptr_t val) {
  context->__rbp = val;
}

inline uintptr_t RegisterContextInstructionPointer(
    x86_thread_state64_t* context) {
  return context->__rip;
}

inline void SetRegisterContextInstructionPointer(x86_thread_state64_t* context,
                                                 uintptr_t val) {
  context->__rip = val;
}

#elif BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)

#if BUILDFLAG(ARCH_CPU_PTRAUTH)
inline auto& Arm64Sp(arm_thread_state64_t* ctx) {
  return ctx->__opaque_sp;
}
inline auto& Arm64Fp(arm_thread_state64_t* ctx) {
  return ctx->__opaque_fp;
}
inline auto& Arm64Pc(arm_thread_state64_t* ctx) {
  return ctx->__opaque_pc;
}
#else   // !BUILDFLAG(ARCH_CPU_PTRAUTH)
inline auto& Arm64Sp(arm_thread_state64_t* ctx) {
  return ctx->__sp;
}
inline auto& Arm64Fp(arm_thread_state64_t* ctx) {
  return ctx->__fp;
}
inline auto& Arm64Pc(arm_thread_state64_t* ctx) {
  return ctx->__pc;
}
#endif  // !BUILDFLAG(ARCH_CPU_PTRAUTH)

inline uintptr_t RegisterContextStackPointer(arm_thread_state64_t* context) {
  void* val = ptrauth_strip(reinterpret_cast<void*>(Arm64Sp(context)),
                            ptrauth_key_asda);
  return AsUintPtr(&val);
}

inline void SetRegisterContextStackPointer(arm_thread_state64_t* context,
                                           uintptr_t val) {
  AsUintPtr(&Arm64Sp(context)) = val;
}

inline uintptr_t RegisterContextFramePointer(arm_thread_state64_t* context) {
  void* val = ptrauth_strip(reinterpret_cast<void*>(Arm64Fp(context)),
                            ptrauth_key_asda);
  return AsUintPtr(&val);
}

inline void SetRegisterContextFramePointer(arm_thread_state64_t* context,
                                           uintptr_t val) {
  AsUintPtr(&Arm64Fp(context)) = val;
}

inline uintptr_t RegisterContextInstructionPointer(
    arm_thread_state64_t* context) {
  void* val = ptrauth_strip(reinterpret_cast<void*>(Arm64Pc(context)),
                            ptrauth_key_asia);
  return AsUintPtr(&val);
}

inline void SetRegisterContextInstructionPointer(arm_thread_state64_t* context,
                                                 uintptr_t val) {
  AsUintPtr(&Arm64Pc(context)) = val;
}

#else  // BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)

// Placeholders for other cases.
inline uintptr_t RegisterContextStackPointer(RegisterContext* context) {
  return context->stack_pointer;
}

inline void SetRegisterContextStackPointer(RegisterContext* context,
                                           uintptr_t val) {
  context->stack_pointer = val;
}

inline uintptr_t RegisterContextFramePointer(RegisterContext* context) {
  return context->frame_pointer;
}

inline void SetRegisterContextFramePointer(RegisterContext* context,
                                           uintptr_t val) {
  context->frame_pointer = val;
}

inline uintptr_t RegisterContextInstructionPointer(RegisterContext* context) {
  return context->instruction_pointer;
}

inline void SetRegisterContextInstructionPointer(RegisterContext* context,
                                                 uintptr_t val) {
  context->instruction_pointer = val;
}

#endif

}  // namespace base

#endif  // BASE_PROFILER_REGISTER_CONTEXT_REGISTERS_H_
