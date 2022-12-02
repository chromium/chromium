// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_bound_arg_tracker.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>

#include "base/memory/raw_ptr_asan_service.h"
#include "base/no_destructor.h"

namespace base {
// static
uintptr_t RawPtrAsanBoundArgTracker::GetProtectedArgPtr(uintptr_t ptr) {
  ProtectedArgsVector* protected_args = CurrentProtectedArgs().Get();
  if (!protected_args) {
    return 0;
  }

  for (auto protected_arg_ptr : *protected_args) {
    uintptr_t allocation_base = 0;
    size_t allocation_size = 0;
    __asan_locate_address(reinterpret_cast<void*>(protected_arg_ptr), nullptr,
                          0, reinterpret_cast<void**>(&allocation_base),
                          &allocation_size);
    if (allocation_base <= ptr && ptr < allocation_base + allocation_size) {
      return allocation_base;
    }
  }

  return 0;
}

RawPtrAsanBoundArgTracker::RawPtrAsanBoundArgTracker()
    : enabled_(RawPtrAsanService::GetInstance().IsEnabled()) {
  if (enabled_) {
    prev_protected_args_ = CurrentProtectedArgs().Get();
    CurrentProtectedArgs().Set(&protected_args_);
  }
}

RawPtrAsanBoundArgTracker::~RawPtrAsanBoundArgTracker() {
  if (enabled_) {
    CurrentProtectedArgs().Set(prev_protected_args_);
  }
}

void RawPtrAsanBoundArgTracker::Add(uintptr_t ptr) {
  if (ptr) {
    protected_args_->push_back(ptr);
  }
}

// static
ThreadLocalPointer<RawPtrAsanBoundArgTracker::ProtectedArgsVector>&
RawPtrAsanBoundArgTracker::CurrentProtectedArgs() {
  // We use thread-local storage instead of sequence-local storage for
  // consistency with PendingReport in RawPtrAsanService.
  static NoDestructor<
      ThreadLocalPointer<RawPtrAsanBoundArgTracker::ProtectedArgsVector>>
      protected_args;
  return *protected_args;
}
}  // namespace base

#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
