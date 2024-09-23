// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_bound_arg_tracker.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>

#include "base/memory/raw_ptr_asan_service.h"

namespace base {

namespace {

// We use thread-local storage instead of sequence-local storage for consistency
// with PendingReport in RawPtrAsanService.
constinit thread_local RawPtrAsanBoundArgTracker::ProtectedArgsVector*
    protected_args = nullptr;

}  // namespace

// static
uintptr_t RawPtrAsanBoundArgTracker::GetProtectedArgPtr(uintptr_t ptr) {
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
    prev_protected_args_ = protected_args;
    protected_args = &protected_args_;
  }
}

RawPtrAsanBoundArgTracker::~RawPtrAsanBoundArgTracker() {
  if (enabled_) {
    protected_args = prev_protected_args_;
  }
}

void RawPtrAsanBoundArgTracker::Add(uintptr_t ptr) {
  if (ptr) {
    protected_args_.push_back(ptr);
  }
}

}  // namespace base

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
