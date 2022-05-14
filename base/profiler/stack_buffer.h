// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_BUFFER_H_
#define BASE_PROFILER_STACK_BUFFER_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/base_export.h"
#include "base/memory/aligned_memory.h"

namespace base {

// This class contains a buffer for stack copies that can be shared across
// multiple instances of StackSampler.
class BASE_EXPORT StackBuffer {
 public:
  // The expected alignment of the stack on the current platform. Windows and
  // System V AMD64 ABIs on x86, x64, and ARM require the stack to be aligned
  // to twice the pointer size. Excepted from this requirement is code setting
  // up the stack during function calls (between pushing the return address
  // and the end of the function prologue). The profiler will sometimes
  // encounter this exceptional case for leaf frames.
  static constexpr size_t kPlatformStackAlignment = 2 * sizeof(uintptr_t);

  explicit StackBuffer(size_t buffer_size);

  StackBuffer(const StackBuffer&) = delete;
  StackBuffer& operator=(const StackBuffer&) = delete;

  ~StackBuffer();

  // Returns a kPlatformStackAlignment-aligned pointer to the stack buffer.
  uintptr_t* buffer() const {
    // Aligned during allocation.
    return buffer_.get();
  }

  // Size in bytes.
  size_t size() const { return size_; }

 private:
  // The size in bytes of the requested buffer allocation.
  const size_t size_;

  // The buffer to store the stack. Already aligned.
  const std::unique_ptr<uintptr_t, AlignedFreeDeleter> buffer_;
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_BUFFER_H_
