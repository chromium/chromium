// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_buffer.h"

#include "base/memory/aligned_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(StackBufferTest, BufferAllocated) {
  const unsigned int kBufferSize = 32 * 1024;
  StackBuffer stack_buffer(kBufferSize);
  EXPECT_EQ(stack_buffer.size(), kBufferSize);
  // Without volatile, the compiler could simply optimize away the entire for
  // loop below.
  volatile uintptr_t* buffer = stack_buffer.buffer();
  ASSERT_NE(nullptr, buffer);
  EXPECT_TRUE(IsAligned(const_cast<uintptr_t*>(buffer),
                        StackBuffer::kPlatformStackAlignment));

  // Memory pointed to by buffer should be writable.
  for (unsigned int i = 0; i < (kBufferSize / sizeof(buffer[0])); i++) {
    buffer[i] = i;
    EXPECT_EQ(buffer[i], i);
  }
}

}  // namespace base
