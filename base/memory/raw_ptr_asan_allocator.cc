// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_allocator.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#include "base/no_destructor.h"

namespace base::internal {

partition_alloc::PartitionRoot& GetRawPtrAsanInternalAllocator() {
  // PartitionAllocator for internal memory allocation.
  static base::NoDestructor<partition_alloc::PartitionRoot> allocator{
      partition_alloc::PartitionOptions{}};
  return *allocator.get();
}

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
