// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_PRIORITY_H_
#define CC_TILES_TILE_PRIORITY_H_

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "base/trace_event/traced_value.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/cc_export.h"

namespace cc {

enum WhichTree {
  // Note: these must be 0 and 1 because we index with them in various places,
  // e.g. in Tile::priority_.
  ACTIVE_TREE = 0,
  PENDING_TREE = 1,
  LAST_TREE = 1
};

enum TileResolution {
  LOW_RESOLUTION = 0 ,
  HIGH_RESOLUTION = 1,
  NON_IDEAL_RESOLUTION = 2,
};
std::string TileResolutionToString(TileResolution resolution);

struct CC_EXPORT TilePriority {
  enum PriorityBin { NOW, SOON, EVENTUALLY };

  constexpr TilePriority()
      : resolution(NON_IDEAL_RESOLUTION),
        priority_bin(EVENTUALLY),
        distance_to_visible(std::numeric_limits<float>::infinity()) {}

  constexpr TilePriority(TileResolution resolution,
                         PriorityBin bin,
                         float distance_to_visible)
      : resolution(resolution),
        priority_bin(bin),
        distance_to_visible(distance_to_visible) {}

  void AsValueInto(base::trace_event::TracedValue* dict) const;

  bool IsHigherPriorityThan(const TilePriority& other) const {
    return priority_bin < other.priority_bin ||
           (priority_bin == other.priority_bin &&
            distance_to_visible < other.distance_to_visible);
  }

  TileResolution resolution;
  PriorityBin priority_bin;
  float distance_to_visible;
};

std::string TilePriorityBinToString(TilePriority::PriorityBin bin);

// It is expected the values are ordered from most restrictive to least
// restrictive. See IsTileMemoryLimitPolicyMoreRestictive().
enum TileMemoryLimitPolicy {
  // Nothing. This mode is used when visible is set to false.
  ALLOW_NOTHING = 0,  // Decaf.

  // You might be made visible, but you're not being interacted with.
  ALLOW_ABSOLUTE_MINIMUM = 1,  // Tall.

  // You're being interacted with, but we're low on memory.
  ALLOW_PREPAINT_ONLY = 2,  // Grande.

  // You're the only thing in town. Go crazy.
  ALLOW_ANYTHING = 3  // Venti.
};
std::string TileMemoryLimitPolicyToString(TileMemoryLimitPolicy policy);

// Returns true if `policy1` is more restrictive than `policy2`.
bool IsTileMemoryLimitPolicyMoreRestictive(TileMemoryLimitPolicy policy1,
                                           TileMemoryLimitPolicy policy2);

enum TreePriority {
  SAME_PRIORITY_FOR_BOTH_TREES,
  SMOOTHNESS_TAKES_PRIORITY,
  NEW_CONTENT_TAKES_PRIORITY,
  LAST_TREE_PRIORITY = NEW_CONTENT_TAKES_PRIORITY
  // Be sure to update TreePriorityAsValue when adding new fields.
};
// TODO(nuskos): remove TreePriorityToString once we have a utility function to
// take protozero to strings.
std::string TreePriorityToString(TreePriority prio);
perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MinorStateV2::
    TreePriority
    TreePriorityToProtozeroEnum(TreePriority priority);

class GlobalStateThatImpactsTilePriority {
 public:
  GlobalStateThatImpactsTilePriority()
      : memory_limit_policy(ALLOW_NOTHING),
        soft_memory_limit_in_bytes(0),
        hard_memory_limit_in_bytes(0),
        num_resources_limit(0),
        tree_priority(SAME_PRIORITY_FOR_BOTH_TREES) {}

  TileMemoryLimitPolicy memory_limit_policy;

  size_t soft_memory_limit_in_bytes;
  size_t hard_memory_limit_in_bytes;
  size_t num_resources_limit;

  TreePriority tree_priority;

  bool operator==(const GlobalStateThatImpactsTilePriority& other) const {
    return memory_limit_policy == other.memory_limit_policy &&
           soft_memory_limit_in_bytes == other.soft_memory_limit_in_bytes &&
           hard_memory_limit_in_bytes == other.hard_memory_limit_in_bytes &&
           num_resources_limit == other.num_resources_limit &&
           tree_priority == other.tree_priority;
  }
  bool operator!=(const GlobalStateThatImpactsTilePriority& other) const {
    return !(*this == other);
  }

  void AsValueInto(base::trace_event::TracedValue* dict) const;
};

}  // namespace cc

#endif  // CC_TILES_TILE_PRIORITY_H_
