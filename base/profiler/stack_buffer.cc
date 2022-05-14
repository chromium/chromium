// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_buffer.h"

#include "base/bits.h"

namespace base {

constexpr size_t StackBuffer::kPlatformStackAlignment;

StackBuffer::StackBuffer(size_t buffer_size)
    : size_(buffer_size),
      buffer_(static_cast<uintptr_t*>(
          AlignedAlloc(size_, kPlatformStackAlignment))) {
  static_assert(bits::IsPowerOfTwo(kPlatformStackAlignment));
}

StackBuffer::~StackBuffer() = default;

}  // namespace base
