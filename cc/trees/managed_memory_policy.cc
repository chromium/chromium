// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/managed_memory_policy.h"

#include <stddef.h>

#include "base/notreached.h"

namespace cc {

const size_t ManagedMemoryPolicy::kDefaultNumResourcesLimit = 10 * 1000 * 1000;

using gpu::MemoryAllocation;

ManagedMemoryPolicy::ManagedMemoryPolicy(size_t bytes_limit_when_visible)
    : bytes_limit_when_visible(bytes_limit_when_visible),
      priority_cutoff_when_visible(MemoryAllocation::CUTOFF_ALLOW_EVERYTHING),
      num_resources_limit(kDefaultNumResourcesLimit) {}

ManagedMemoryPolicy::ManagedMemoryPolicy(
    const gpu::MemoryAllocation& allocation)
    : bytes_limit_when_visible(allocation.bytes_limit_when_visible),
      priority_cutoff_when_visible(allocation.priority_cutoff_when_visible),
      num_resources_limit(kDefaultNumResourcesLimit) {}

ManagedMemoryPolicy::ManagedMemoryPolicy(
    size_t bytes_limit_when_visible,
    MemoryAllocation::PriorityCutoff priority_cutoff_when_visible,
    size_t num_resources_limit)
    : bytes_limit_when_visible(bytes_limit_when_visible),
      priority_cutoff_when_visible(priority_cutoff_when_visible),
      num_resources_limit(num_resources_limit) {}

bool ManagedMemoryPolicy::operator==(const ManagedMemoryPolicy& other) const {
  return bytes_limit_when_visible == other.bytes_limit_when_visible &&
         priority_cutoff_when_visible == other.priority_cutoff_when_visible &&
         num_resources_limit == other.num_resources_limit;
}

bool ManagedMemoryPolicy::operator!=(const ManagedMemoryPolicy& other) const {
  return !(*this == other);
}

// static
TileMemoryLimitPolicy
ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
    gpu::MemoryAllocation::PriorityCutoff priority_cutoff) {
  switch (priority_cutoff) {
    case MemoryAllocation::CUTOFF_ALLOW_NOTHING:
      return ALLOW_NOTHING;
    case MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY:
      return ALLOW_ABSOLUTE_MINIMUM;
    case MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE:
      return ALLOW_PREPAINT_ONLY;
    case MemoryAllocation::CUTOFF_ALLOW_EVERYTHING:
      return ALLOW_ANYTHING;
  }
  NOTREACHED();
  return ALLOW_NOTHING;
}

}  // namespace cc
