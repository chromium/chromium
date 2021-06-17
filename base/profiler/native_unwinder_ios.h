// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_NATIVE_UNWINDER_IOS_H_
#define BASE_PROFILER_NATIVE_UNWINDER_IOS_H_

#include "base/profiler/unwinder.h"

namespace base {

// Native unwinder implementation for iOS, ARM64 and X86_64.
class NativeUnwinderIOS : public Unwinder {
 public:
  NativeUnwinderIOS();

  NativeUnwinderIOS(const NativeUnwinderIOS&) = delete;
  NativeUnwinderIOS& operator=(const NativeUnwinderIOS&) = delete;

  // Unwinder:
  bool CanUnwindFrom(const Frame& current_frame) const override;
  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) const override;
};

}  // namespace base

#endif  // BASE_PROFILER_NATIVE_UNWINDER_IOS_H_
