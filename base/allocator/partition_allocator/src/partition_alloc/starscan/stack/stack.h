// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_STACK_STACK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_STACK_STACK_H_

#include <cstdint>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc::internal {

// Returns the current stack pointer.
// TODO(bikineev,1202644): Remove this once base/stack_util.h lands.
PA_NOINLINE PA_COMPONENT_EXPORT(PARTITION_ALLOC) uintptr_t* GetStackPointer();
// Returns the top of the stack using system API.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void* GetStackTop();

// Interface for stack visitation.
class StackVisitor {
 public:
  virtual void VisitStack(uintptr_t* stack_ptr, uintptr_t* stack_top) = 0;
};

// Abstraction over the stack. Supports handling of:
// - native stack;
// - SafeStack: https://releases.llvm.org/10.0.0/tools/clang/docs/SafeStack.html
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) Stack final {
 public:
  // Sets start of the stack.
  explicit Stack(void* stack_top);

  // Word-aligned iteration of the stack. Flushes callee saved registers and
  // passes the range of the stack on to |visitor|.
  void IteratePointers(StackVisitor* visitor) const;

  // Returns the top of the stack.
  void* stack_top() const { return stack_top_; }

 private:
  void* stack_top_;
};

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_STACK_STACK_H_
