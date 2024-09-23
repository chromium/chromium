// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/suspendable_thread_delegate_mac.h"

#include <mach/mach.h>
#include <mach/thread_act.h>
#include <pthread.h>

#include <vector>

#include "base/apple/mach_logging.h"
#include "base/check.h"
#include "base/profiler/profile_builder.h"
#include "build/build_config.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

namespace {

#if defined(ARCH_CPU_X86_64)
constexpr mach_msg_type_number_t kThreadStateCount = x86_THREAD_STATE64_COUNT;
constexpr thread_state_flavor_t kThreadStateFlavor = x86_THREAD_STATE64;
#elif defined(ARCH_CPU_ARM64)
constexpr mach_msg_type_number_t kThreadStateCount = ARM_THREAD_STATE64_COUNT;
constexpr thread_state_flavor_t kThreadStateFlavor = ARM_THREAD_STATE64;
#endif

// Fills |state| with |target_thread|'s context. NO HEAP ALLOCATIONS.
bool GetThreadContextImpl(thread_act_t target_thread, RegisterContext* state) {
  auto count = kThreadStateCount;
  return thread_get_state(target_thread, kThreadStateFlavor,
                          reinterpret_cast<thread_state_t>(state),
                          &count) == KERN_SUCCESS;
}

}  // namespace

// ScopedSuspendThread --------------------------------------------------------

// NO HEAP ALLOCATIONS after thread_suspend.
SuspendableThreadDelegateMac::ScopedSuspendThread::ScopedSuspendThread(
    mach_port_t thread_port)
    : thread_port_(thread_suspend(thread_port) == KERN_SUCCESS
                       ? thread_port
                       : MACH_PORT_NULL) {}

// NO HEAP ALLOCATIONS. The MACH_CHECK is OK because it provides a more noisy
// failure mode than deadlocking.
SuspendableThreadDelegateMac::ScopedSuspendThread::~ScopedSuspendThread() {
  if (!WasSuccessful())
    return;

  kern_return_t kr = thread_resume(thread_port_);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "thread_resume";
}

bool SuspendableThreadDelegateMac::ScopedSuspendThread::WasSuccessful() const {
  return thread_port_ != MACH_PORT_NULL;
}

// SuspendableThreadDelegateMac -----------------------------------------------

SuspendableThreadDelegateMac::SuspendableThreadDelegateMac(
    SamplingProfilerThreadToken thread_token)
    : thread_port_(thread_token.id),
      thread_stack_base_address_(
          reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(
              pthread_from_mach_thread_np(thread_token.id)))) {
  // This class suspends threads, and those threads might be suspended in dyld.
  // Therefore, for all the system functions that might be linked in dynamically
  // that are used while threads are suspended, make calls to them to make sure
  // that they are linked up.
  RegisterContext thread_context;
  GetThreadContextImpl(thread_port_, &thread_context);
}

SuspendableThreadDelegateMac::~SuspendableThreadDelegateMac() = default;

std::unique_ptr<SuspendableThreadDelegate::ScopedSuspendThread>
SuspendableThreadDelegateMac::CreateScopedSuspendThread() {
  return std::make_unique<ScopedSuspendThread>(thread_port_);
}

PlatformThreadId SuspendableThreadDelegateMac::GetThreadId() const {
  return thread_port_;
}

// NO HEAP ALLOCATIONS.
bool SuspendableThreadDelegateMac::GetThreadContext(
    RegisterContext* thread_context) {
  return GetThreadContextImpl(thread_port_, thread_context);
}

// NO HEAP ALLOCATIONS.
uintptr_t SuspendableThreadDelegateMac::GetStackBaseAddress() const {
  return thread_stack_base_address_;
}

// NO HEAP ALLOCATIONS.
bool SuspendableThreadDelegateMac::CanCopyStack(uintptr_t stack_pointer) {
  return true;
}

std::vector<uintptr_t*> SuspendableThreadDelegateMac::GetRegistersToRewrite(
    RegisterContext* thread_context) {
#if defined(ARCH_CPU_X86_64)
  return {
      &AsUintPtr(&thread_context->__rbx), &AsUintPtr(&thread_context->__rbp),
      &AsUintPtr(&thread_context->__rsp), &AsUintPtr(&thread_context->__r12),
      &AsUintPtr(&thread_context->__r13), &AsUintPtr(&thread_context->__r14),
      &AsUintPtr(&thread_context->__r15)};
#elif defined(ARCH_CPU_ARM64)  // defined(ARCH_CPU_X86_64)
  return {
      &AsUintPtr(&thread_context->__fp),
      &AsUintPtr(&thread_context->__sp),
      &AsUintPtr(&thread_context->__x[19]),
      &AsUintPtr(&thread_context->__x[20]),
      &AsUintPtr(&thread_context->__x[21]),
      &AsUintPtr(&thread_context->__x[22]),
      &AsUintPtr(&thread_context->__x[23]),
      &AsUintPtr(&thread_context->__x[24]),
      &AsUintPtr(&thread_context->__x[25]),
      &AsUintPtr(&thread_context->__x[26]),
      &AsUintPtr(&thread_context->__x[27]),
      &AsUintPtr(&thread_context->__x[28]),
  };
#endif                         // defined(ARCH_CPU_ARM64)
}

}  // namespace base
