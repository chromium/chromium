// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_STACK_STACK_H_
#define PARTITION_ALLOC_STACK_STACK_H_

#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>

#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/containers/flat_map.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc::internal {

namespace base {
template <typename T>
class NoDestructor;
}

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
// - SafeStack:
// https://releases.llvm.org/10.0.0/tools/clang/docs/SafeStack.html
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

// A class to keep stack top pointers through thread creation/destruction.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) StackTopRegistry {
 public:
  static StackTopRegistry& Get();
  void NotifyThreadCreated(void* stack_top = GetStackPointer());
  void NotifyThreadDestroyed();
  void* GetCurrentThreadStackTop() const;

 private:
  // std::unordered_map is not available inside PartitionAlloc, because
  // libc++.dll depends on PartitionAlloc when PartitionAlloc-Everywhere
  // is enabled on Windows component build, and std::unordered_map needs
  // a symbol defined inside libc++, i.e. __next_prime. Instead, we should
  // use base::flat_map. Regarding std::vector, std::set, and so on, these
  // classes are header-only. They are still available inside
  // PartitionAlloc with InternalAllocator.
  using StackTops = base::flat_map<
      base::PlatformThreadId,
      void*,
      std::less<>,
      std::vector<std::pair<base::PlatformThreadId, void*>,
                  internal::InternalAllocator<
                      std::pair<base::PlatformThreadId, void*>>>>;

  friend class base::NoDestructor<StackTopRegistry>;

  StackTopRegistry();
  ~StackTopRegistry();

  // TLS emulation of stack tops. Since this is guaranteed to go through
  // non-quarantinable partition, using it from safepoints is safe.
  mutable Lock lock_;
  StackTops stack_tops_ PA_GUARDED_BY(lock_);
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_STACK_STACK_H_
