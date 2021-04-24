// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"
#include "base/no_destructor.h"

namespace base {
namespace internal {

ThreadSafePartitionRoot& PCScanMetadataAllocator() {
  static base::NoDestructor<ThreadSafePartitionRoot> allocator{
      PartitionOptions{PartitionOptions::AlignedAlloc::kDisallowed,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::Cookies::kAllowed,
                       PartitionOptions::RefCount::kDisallowed}};
  return *allocator;
}
}  // namespace internal
}  // namespace base
