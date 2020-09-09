// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_ref_count.h"

#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_page.h"

namespace base {

namespace internal {

#if ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

// TODO(glazunov): Simplify the function once the non-thread-safe PartitionRoot
// is no longer used.
void PartitionRefCount::Free() {
  auto* page = PartitionPage<ThreadSafe>::FromPointerNoAlignmentCheck(this);
  auto* root = PartitionRoot<ThreadSafe>::FromPage(page);

#ifdef ADDRESS_SANITIZER
  size_t allocated_size = page->GetAllocatedSize();
  // PartitionRefCount is required to be allocated inside a `PartitionRoot` that
  // supports extras.
  PA_DCHECK(root->allow_extras);
  size_t size_with_no_extras = internal::PartitionSizeAdjustSubtract(
      /* allow_extras= */ true, allocated_size);
  ASAN_UNPOISON_MEMORY_REGION(this, size_with_no_extras);
#endif

  if (root->is_thread_safe) {
    root->RawFree(this, page);
    return;
  }

  auto* non_thread_safe_page =
      reinterpret_cast<PartitionPage<NotThreadSafe>*>(page);
  auto* non_thread_safe_root =
      reinterpret_cast<PartitionRoot<NotThreadSafe>*>(root);
  non_thread_safe_root->RawFree(this, non_thread_safe_page);
}

#endif  // ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

}  // namespace internal
}  // namespace base
