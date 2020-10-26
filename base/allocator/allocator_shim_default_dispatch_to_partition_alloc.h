// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
#define BASE_ALLOCATOR_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_

#include "base/allocator/partition_allocator/partition_alloc.h"

namespace base {
namespace internal {

class PartitionAllocMalloc {
 public:
  static ThreadSafePartitionRoot* Allocator();
  static ThreadSafePartitionRoot* AlignedAllocator();
};

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
