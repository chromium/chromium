// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MANAGED_MEMORY_POLICY_H_
#define CC_TREES_MANAGED_MEMORY_POLICY_H_

#include <stddef.h>

#include "cc/cc_export.h"
#include "cc/tiles/tile_priority.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"

namespace cc {

struct CC_EXPORT ManagedMemoryPolicy {
  static const size_t kDefaultNumResourcesLimit;

  explicit ManagedMemoryPolicy(size_t bytes_limit_when_visible);
  explicit ManagedMemoryPolicy(const gpu::MemoryAllocation& allocation);
  ManagedMemoryPolicy(
      size_t bytes_limit_when_visible,
      gpu::MemoryAllocation::PriorityCutoff priority_cutoff_when_visible,
      size_t num_resources_limit);
  bool operator==(const ManagedMemoryPolicy&) const;
  bool operator!=(const ManagedMemoryPolicy&) const;

  size_t bytes_limit_when_visible;
  gpu::MemoryAllocation::PriorityCutoff priority_cutoff_when_visible;
  size_t num_resources_limit;

  static TileMemoryLimitPolicy PriorityCutoffToTileMemoryLimitPolicy(
      gpu::MemoryAllocation::PriorityCutoff priority_cutoff);
};

}  // namespace cc

#endif  // CC_TREES_MANAGED_MEMORY_POLICY_H_
