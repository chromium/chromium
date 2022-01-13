// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ALLOCATION_GUARD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ALLOCATION_GUARD_H_

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"

namespace base {
namespace internal {

#if defined(PA_HAS_ALLOCATION_GUARD)

// Disallow allocations in the scope. Does not nest.
class ScopedDisallowAllocations {
 public:
  ScopedDisallowAllocations();
  ~ScopedDisallowAllocations();
};

// Disallow allocations in the scope. Does not nest.
class ScopedAllowAllocations {
 public:
  ScopedAllowAllocations();
  ~ScopedAllowAllocations();

 private:
  bool saved_value_;
};

#else

struct [[maybe_unused]] ScopedDisallowAllocations{};
struct [[maybe_unused]] ScopedAllowAllocations{};

#endif  // defined(PA_HAS_ALLOCATION_GUARD)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ALLOCATION_GUARD_H_
