// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/allocation_guard.h"

#include "partition_alloc/partition_alloc_base/immediate_crash.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_tls.h"

#if PA_CONFIG(HAS_ALLOCATION_GUARD)

namespace partition_alloc {

ScopedDisallowAllocations::ScopedDisallowAllocations() {
  auto* tls = internal::GetTls();
  if (tls) {
    if (tls->disallow_allocations) {
      PA_IMMEDIATE_CRASH();
    }
    tls->disallow_allocations = true;
  }
}

ScopedDisallowAllocations::~ScopedDisallowAllocations() {
  auto* tls = internal::GetTls();
  if (tls) {
    tls->disallow_allocations = false;
  }
}

ScopedAllowAllocations::ScopedAllowAllocations() {
  auto* tls = internal::GetTls();
  if (tls) {
    // Save the previous value, as ScopedAllowAllocations is used in all
    // partitions, not just the malloc() ones(s).
    saved_value_ = tls->disallow_allocations;
    tls->disallow_allocations = false;
  } else {
    saved_value_ = false;
  }
}

ScopedAllowAllocations::~ScopedAllowAllocations() {
  auto* tls = internal::GetTls();
  if (tls) {
    tls->disallow_allocations = saved_value_;
  }
}

}  // namespace partition_alloc

#endif  // PA_CONFIG(HAS_ALLOCATION_GUARD)
