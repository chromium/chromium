// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/native_unwinder_ios.h"

#include <pthread/stack_np.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/native_unwinder.h"
#include "build/build_config.h"

namespace base {

NativeUnwinderIOS::NativeUnwinderIOS() {}

bool NativeUnwinderIOS::CanUnwindFrom(const Frame& current_frame) const {
  return current_frame.module && current_frame.module->IsNative();
}

UnwindResult NativeUnwinderIOS::TryUnwind(RegisterContext* thread_context,
                                          uintptr_t stack_top,
                                          std::vector<Frame>* stack) const {
  // We expect the frame corresponding to the |thread_context| register state to
  // exist within |stack|.
  DCHECK_GT(stack->size(), 0u);

#if defined(ARCH_CPU_ARM64)
  const uintptr_t align_mask = 0x1;
  const uintptr_t stack_bottom = thread_context->__sp;
  uintptr_t next_frame = thread_context->__fp;
#elif defined(ARCH_CPU_X86_64)
  const uintptr_t align_mask = 0xf;
  const uintptr_t stack_bottom = thread_context->__rsp;
  uintptr_t next_frame = thread_context->__rbp;
#endif

  const auto is_fp_valid = [&](uintptr_t fp) {
    return fp >= stack_bottom && fp < stack_top && (fp & align_mask) == 0;
  };

  if (!is_fp_valid(next_frame)) {
    return UnwindResult::ABORTED;
  }

  for (;;) {
    uintptr_t retaddr;
    uintptr_t frame = next_frame;
    next_frame = pthread_stack_frame_decode_np(frame, &retaddr);

    if (!is_fp_valid(frame) || next_frame <= frame) {
      return UnwindResult::COMPLETED;
    }

    stack->emplace_back(retaddr, module_cache()->GetModuleForAddress(retaddr));
  }

  NOTREACHED();
  return UnwindResult::COMPLETED;
}

std::unique_ptr<Unwinder> CreateNativeUnwinder(ModuleCache* module_cache) {
  return std::make_unique<NativeUnwinderIOS>();
}

}  // namespace base
