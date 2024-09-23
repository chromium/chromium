// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/stack_buffer.h"

#include "base/memory/aligned_memory.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/bits.h"
#include "base/memory/page_size.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)
TEST(StackBufferTest, MarkBufferContentsAsUnneeded) {
  const unsigned int kBufferSize = 32 * GetPageSize();
  StackBuffer stack_buffer(kBufferSize);
  volatile uintptr_t* buffer = stack_buffer.buffer();
  ASSERT_NE(nullptr, buffer);

  // Force the kernel to allocate backing store for the buffer.
  for (unsigned int i = 0; i < (kBufferSize / sizeof(uintptr_t)); i++) {
    buffer[i] = i;
    EXPECT_EQ(buffer[i], i);
  }

  // Tell kernel to discard (most of) the memory.
  constexpr size_t kUndiscardedElements = 100;
  stack_buffer.MarkUpperBufferContentsAsUnneeded(kUndiscardedElements *
                                                 sizeof(buffer[0]));

  // The first 100 elements shouldn't have been discarded.
  for (size_t i = 0; i < kUndiscardedElements; i++) {
    EXPECT_EQ(buffer[i], i);
  }

  // Pages past the discard point should be zero-filled now.
  const size_t kExpectedDiscardStartPoint =
      bits::AlignUp(kUndiscardedElements * sizeof(buffer[0]), GetPageSize()) /
      sizeof(buffer[0]);
  for (size_t i = kExpectedDiscardStartPoint;
       i < kBufferSize / sizeof(buffer[0]); i++) {
    EXPECT_EQ(buffer[i], 0U);
  }

  // Writing to the memory (both discarded and undiscarded parts) shouldn't
  // cause segmentation faults and should remember the value we write.
  for (unsigned int i = 0; i < (kBufferSize / sizeof(buffer[0])); i++) {
    buffer[i] = i + 7;
    EXPECT_EQ(buffer[i], i + 7);
  }
}
#endif  // #if BUILDFLAG(IS_CHROMEOS)

}  // namespace base
