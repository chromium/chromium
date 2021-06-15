// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <numeric>

#include "base/cxx17_backports.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class CopyFunctions : public StackCopier {
 public:
  using StackCopier::CopyStackContentsAndRewritePointers;
  using StackCopier::RewritePointerIfInOriginalStack;
};

static constexpr size_t kTestStackBufferSize = sizeof(uintptr_t) * 4;

union alignas(StackBuffer::kPlatformStackAlignment) TestStackBuffer {
  uintptr_t as_uintptr[kTestStackBufferSize / sizeof(uintptr_t)];
  uint16_t as_uint16[kTestStackBufferSize / sizeof(uint16_t)];
  uint8_t as_uint8[kTestStackBufferSize / sizeof(uint8_t)];
};

}  // namespace

TEST(StackCopierTest, RewritePointerIfInOriginalStack_InStack) {
  uintptr_t original_stack[4];
  uintptr_t stack_copy[4];
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack_copy[2]),
            CopyFunctions::RewritePointerIfInOriginalStack(
                reinterpret_cast<uint8_t*>(&original_stack[0]),
                &original_stack[0] + size(original_stack),
                reinterpret_cast<uint8_t*>(&stack_copy[0]),
                reinterpret_cast<uintptr_t>(&original_stack[2])));
}

TEST(StackCopierTest, RewritePointerIfInOriginalStack_NotInStack) {
  // We use this variable only for its address, which is outside of
  // original_stack.
  uintptr_t non_stack_location;
  uintptr_t original_stack[4];
  uintptr_t stack_copy[4];

  EXPECT_EQ(reinterpret_cast<uintptr_t>(&non_stack_location),
            CopyFunctions::RewritePointerIfInOriginalStack(
                reinterpret_cast<uint8_t*>(&original_stack[0]),
                &original_stack[0] + size(original_stack),
                reinterpret_cast<uint8_t*>(&stack_copy[0]),
                reinterpret_cast<uintptr_t>(&non_stack_location)));
}

TEST(StackCopierTest, StackCopy) {
  TestStackBuffer original_stack;
  // Fill the stack buffer with increasing uintptr_t values.
  std::iota(&original_stack.as_uintptr[0],
            &original_stack.as_uintptr[0] + size(original_stack.as_uintptr),
            100);
  // Replace the third value with an address within the buffer.
  original_stack.as_uintptr[2] =
      reinterpret_cast<uintptr_t>(&original_stack.as_uintptr[1]);
  TestStackBuffer stack_copy;

  CopyFunctions::CopyStackContentsAndRewritePointers(
      &original_stack.as_uint8[0],
      &original_stack.as_uintptr[0] + size(original_stack.as_uintptr),
      StackBuffer::kPlatformStackAlignment, &stack_copy.as_uintptr[0]);

  EXPECT_EQ(original_stack.as_uintptr[0], stack_copy.as_uintptr[0]);
  EXPECT_EQ(original_stack.as_uintptr[1], stack_copy.as_uintptr[1]);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack_copy.as_uintptr[1]),
            stack_copy.as_uintptr[2]);
  EXPECT_EQ(original_stack.as_uintptr[3], stack_copy.as_uintptr[3]);
}

TEST(StackCopierTest, StackCopy_NonAlignedStackPointerCopy) {
  TestStackBuffer stack_buffer;

  // Fill the stack buffer with increasing uint16_t values.
  std::iota(&stack_buffer.as_uint16[0],
            &stack_buffer.as_uint16[0] + size(stack_buffer.as_uint16), 100);

  // Set the stack bottom to the unaligned location one uint16_t into the
  // buffer.
  uint8_t* unaligned_stack_bottom =
      reinterpret_cast<uint8_t*>(&stack_buffer.as_uint16[1]);

  // Leave extra space within the stack buffer beyond the end of the stack, but
  // preserve the platform alignment.
  const size_t extra_space = StackBuffer::kPlatformStackAlignment;
  uintptr_t* stack_top =
      &stack_buffer.as_uintptr[size(stack_buffer.as_uintptr) -
                               extra_space / sizeof(uintptr_t)];

  // Initialize the copy to all zeros.
  TestStackBuffer stack_copy_buffer = {{0}};

  const uint8_t* stack_copy_bottom =
      CopyFunctions::CopyStackContentsAndRewritePointers(
          unaligned_stack_bottom, stack_top,
          StackBuffer::kPlatformStackAlignment,
          &stack_copy_buffer.as_uintptr[0]);

  // The stack copy bottom address is expected to be at the same offset into the
  // stack copy buffer as the unaligned stack bottom is from the stack buffer.
  // Since the buffers have the same platform stack alignment this also ensures
  // the alignment of the bottom addresses is the same.
  EXPECT_EQ(unaligned_stack_bottom - &stack_buffer.as_uint8[0],
            stack_copy_bottom - &stack_copy_buffer.as_uint8[0]);

  // The first value in the copy should not be overwritten since the stack
  // starts at the second uint16_t.
  EXPECT_EQ(0u, stack_copy_buffer.as_uint16[0]);

  // The next values up to the extra space should have been copied.
  const size_t max_index =
      size(stack_copy_buffer.as_uint16) - extra_space / sizeof(uint16_t);
  for (size_t i = 1; i < max_index; ++i)
    EXPECT_EQ(i + 100, stack_copy_buffer.as_uint16[i]);

  // None of the values in the empty space should have been copied.
  for (size_t i = max_index; i < size(stack_copy_buffer.as_uint16); ++i)
    EXPECT_EQ(0u, stack_copy_buffer.as_uint16[i]);
}

// Checks that an unaligned within-stack pointer value at the start of the stack
// is not rewritten.
TEST(StackCopierTest, StackCopy_NonAlignedStackPointerUnalignedRewriteAtStart) {
  // Initially fill the buffer with 0s.
  TestStackBuffer stack_buffer = {{0}};

  // Set the stack bottom to the unaligned location one uint16_t into the
  // buffer.
  uint8_t* unaligned_stack_bottom =
      reinterpret_cast<uint8_t*>(&stack_buffer.as_uint16[1]);

  // Set the first unaligned pointer-sized value to an address within the stack.
  uintptr_t within_stack_pointer =
      reinterpret_cast<uintptr_t>(&stack_buffer.as_uintptr[2]);
  std::memcpy(unaligned_stack_bottom, &within_stack_pointer,
              sizeof(within_stack_pointer));

  TestStackBuffer stack_copy_buffer = {{0}};

  const uint8_t* stack_copy_bottom =
      CopyFunctions::CopyStackContentsAndRewritePointers(
          unaligned_stack_bottom,
          &stack_buffer.as_uintptr[0] + size(stack_buffer.as_uintptr),
          StackBuffer::kPlatformStackAlignment,
          &stack_copy_buffer.as_uintptr[0]);

  uintptr_t copied_within_stack_pointer;
  std::memcpy(&copied_within_stack_pointer, stack_copy_bottom,
              sizeof(copied_within_stack_pointer));

  // The rewriting should only operate on pointer-aligned values so the
  // unaligned value should be copied verbatim.
  EXPECT_EQ(within_stack_pointer, copied_within_stack_pointer);
}

// Checks that an unaligned within-stack pointer after the start of the stack is
// not rewritten.
TEST(StackCopierTest,
     StackCopy_NonAlignedStackPointerUnalignedRewriteAfterStart) {
  // Initially fill the buffer with 0s.
  TestStackBuffer stack_buffer = {{0}};

  // Set the stack bottom to the unaligned location one uint16_t into the
  // buffer.
  uint8_t* unaligned_stack_bottom =
      reinterpret_cast<uint8_t*>(&stack_buffer.as_uint16[1]);

  // Set the second unaligned pointer-sized value to an address within the
  // stack.
  uintptr_t within_stack_pointer =
      reinterpret_cast<uintptr_t>(&stack_buffer.as_uintptr[2]);
  std::memcpy(unaligned_stack_bottom + sizeof(uintptr_t), &within_stack_pointer,
              sizeof(within_stack_pointer));

  TestStackBuffer stack_copy_buffer = {{0}};

  const uint8_t* stack_copy_bottom =
      CopyFunctions::CopyStackContentsAndRewritePointers(
          unaligned_stack_bottom,
          &stack_buffer.as_uintptr[0] + size(stack_buffer.as_uintptr),
          StackBuffer::kPlatformStackAlignment,
          &stack_copy_buffer.as_uintptr[0]);

  uintptr_t copied_within_stack_pointer;
  std::memcpy(&copied_within_stack_pointer,
              stack_copy_bottom + sizeof(uintptr_t),
              sizeof(copied_within_stack_pointer));

  // The rewriting should only operate on pointer-aligned values so the
  // unaligned value should be copied verbatim.
  EXPECT_EQ(within_stack_pointer, copied_within_stack_pointer);
}

TEST(StackCopierTest, StackCopy_NonAlignedStackPointerAlignedRewrite) {
  // Initially fill the buffer with 0s.
  TestStackBuffer stack_buffer = {{0}};

  // Set the stack bottom to the unaligned location one uint16_t into the
  // buffer.
  uint8_t* unaligned_stack_bottom =
      reinterpret_cast<uint8_t*>(&stack_buffer.as_uint16[1]);

  // Set the second aligned pointer-sized value to an address within the stack.
  stack_buffer.as_uintptr[1] =
      reinterpret_cast<uintptr_t>(&stack_buffer.as_uintptr[2]);

  TestStackBuffer stack_copy_buffer = {{0}};

  CopyFunctions::CopyStackContentsAndRewritePointers(
      unaligned_stack_bottom,
      &stack_buffer.as_uintptr[0] + size(stack_buffer.as_uintptr),
      StackBuffer::kPlatformStackAlignment, &stack_copy_buffer.as_uintptr[0]);

  // The aligned pointer should have been rewritten to point within the stack
  // copy.
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack_copy_buffer.as_uintptr[2]),
            stack_copy_buffer.as_uintptr[1]);
}

}  // namespace base
