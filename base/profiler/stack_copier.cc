// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/stack_copier.h"

#include <vector>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/profiler/stack_buffer.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/tagging.h"
#endif

namespace base {

StackCopier::~StackCopier() = default;

std::unique_ptr<StackBuffer> StackCopier::CloneStack(
    const StackBuffer& stack_buffer,
    uintptr_t* stack_top,
    RegisterContext* thread_context) {
  const uintptr_t original_top = *stack_top;
  const uintptr_t original_bottom =
      reinterpret_cast<uintptr_t>(stack_buffer.buffer());
  size_t stack_size = original_top - original_bottom;
  auto cloned_stack_buffer = std::make_unique<StackBuffer>(stack_size);
  const uint8_t* stack_copy_bottom = CopyStackContentsAndRewritePointers(
      reinterpret_cast<const uint8_t*>(stack_buffer.buffer()),
      reinterpret_cast<const uintptr_t*>(original_top),
      StackBuffer::kPlatformStackAlignment, cloned_stack_buffer->buffer());

  // `stack_buffer` is double pointer aligned by default so we should always
  // get the same result.
  CHECK(stack_copy_bottom ==
        reinterpret_cast<uint8_t*>(cloned_stack_buffer->buffer()));
  *stack_top =
      reinterpret_cast<const uintptr_t>(stack_copy_bottom) + stack_size;

  for (uintptr_t* reg : GetRegistersToRewrite(thread_context)) {
    *reg = RewritePointerIfInOriginalStack(
        reinterpret_cast<const uint8_t*>(original_bottom),
        reinterpret_cast<const uintptr_t*>(original_top), stack_copy_bottom,
        *reg);
  }
  return cloned_stack_buffer;
}

// static
uintptr_t StackCopier::RewritePointerIfInOriginalStack(
    const uint8_t* original_stack_bottom,
    const uintptr_t* original_stack_top,
    const uint8_t* stack_copy_bottom,
    uintptr_t pointer) {
  auto original_stack_bottom_uint =
      reinterpret_cast<uintptr_t>(original_stack_bottom);
  auto original_stack_top_uint =
      reinterpret_cast<uintptr_t>(original_stack_top);
  auto stack_copy_bottom_uint = reinterpret_cast<uintptr_t>(stack_copy_bottom);

  if (pointer < original_stack_bottom_uint ||
      pointer >= original_stack_top_uint)
    return pointer;

  return stack_copy_bottom_uint + (pointer - original_stack_bottom_uint);
}

// static
NO_SANITIZE("address")
const uint8_t* StackCopier::CopyStackContentsAndRewritePointers(
    const uint8_t* original_stack_bottom,
    const uintptr_t* original_stack_top,
    size_t platform_stack_alignment,
    uintptr_t* stack_buffer_bottom) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
  // Disable MTE during this function because this function indiscriminately
  // reads stack frames, some of which belong to system libraries, not Chrome
  // itself. With stack tagging, some bytes on the stack have MTE tags different
  // from the stack pointer tag.
  partition_alloc::SuspendTagCheckingScope suspend_tag_checking_scope;
#endif

  const uint8_t* byte_src = original_stack_bottom;
  // The first address in the stack with pointer alignment. Pointer-aligned
  // values from this point to the end of the stack are possibly rewritten using
  // RewritePointerIfInOriginalStack(). Bytes before this cannot be a pointer
  // because they occupy less space than a pointer would.
  const uint8_t* first_aligned_address =
      bits::AlignUp(byte_src, sizeof(uintptr_t));

  // The stack copy bottom, which is offset from |stack_buffer_bottom| by the
  // same alignment as in the original stack. This guarantees identical
  // alignment between values in the original stack and the copy. This uses the
  // platform stack alignment rather than pointer alignment so that the stack
  // copy is aligned to platform expectations.
  uint8_t* stack_copy_bottom =
      reinterpret_cast<uint8_t*>(stack_buffer_bottom) +
      (byte_src - bits::AlignDown(byte_src, platform_stack_alignment));
  uint8_t* byte_dst = stack_copy_bottom;

  // Copy bytes verbatim up to the first aligned address.
  for (; byte_src < first_aligned_address; ++byte_src, ++byte_dst)
    *byte_dst = *byte_src;

  // Copy the remaining stack by pointer-sized values, rewriting anything that
  // looks like a pointer into the stack.
  const uintptr_t* src = reinterpret_cast<const uintptr_t*>(byte_src);
  uintptr_t* dst = reinterpret_cast<uintptr_t*>(byte_dst);
  for (; src < original_stack_top; ++src, ++dst) {
    *dst = RewritePointerIfInOriginalStack(
        original_stack_bottom, original_stack_top, stack_copy_bottom, *src);
  }

  return stack_copy_bottom;
}

}  // namespace base
