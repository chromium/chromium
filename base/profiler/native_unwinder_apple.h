// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_NATIVE_UNWINDER_APPLE_H_
#define BASE_PROFILER_NATIVE_UNWINDER_APPLE_H_

#include <vector>

#include <os/availability.h>

#include "base/base_export.h"
#include "base/profiler/unwinder.h"

namespace base {

// Native unwinder implementation for iOS, ARM64 and X86_64, and macOS 10.14+.
class BASE_EXPORT API_AVAILABLE(ios(12)) NativeUnwinderApple : public Unwinder {
 public:
  NativeUnwinderApple();

  NativeUnwinderApple(const NativeUnwinderApple&) = delete;
  NativeUnwinderApple& operator=(const NativeUnwinderApple&) = delete;

  // Unwinder:
  bool CanUnwindFrom(const Frame& current_frame) const override;
  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) const override;
};

}  // namespace base

#endif  // BASE_PROFILER_NATIVE_UNWINDER_APPLE_H_
