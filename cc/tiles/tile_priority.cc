// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_priority.h"

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace cc {

std::string WhichTreeToString(WhichTree tree) {
  switch (tree) {
  case ACTIVE_TREE:
    return "ACTIVE_TREE";
  case PENDING_TREE:
    return "PENDING_TREE";
  default:
      DCHECK(false) << "Unrecognized WhichTree value " << tree;
      return "<unknown WhichTree value>";
  }
}

std::string TileResolutionToString(TileResolution resolution) {
  switch (resolution) {
  case LOW_RESOLUTION:
    return "LOW_RESOLUTION";
  case HIGH_RESOLUTION:
    return "HIGH_RESOLUTION";
  case NON_IDEAL_RESOLUTION:
    return "NON_IDEAL_RESOLUTION";
  }
  DCHECK(false) << "Unrecognized TileResolution value " << resolution;
  return "<unknown TileResolution value>";
}

std::string TilePriorityBinToString(TilePriority::PriorityBin bin) {
  switch (bin) {
    case TilePriority::NOW:
      return "NOW";
    case TilePriority::SOON:
      return "SOON";
    case TilePriority::EVENTUALLY:
      return "EVENTUALLY";
  }
  DCHECK(false) << "Unrecognized TilePriority::PriorityBin value " << bin;
  return "<unknown TilePriority::PriorityBin value>";
}

void TilePriority::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetString("resolution", TileResolutionToString(resolution));
  state->SetString("priority_bin", TilePriorityBinToString(priority_bin));
  state->SetDouble("distance_to_visible",
                   MathUtil::AsDoubleSafely(distance_to_visible));
}

std::string TileMemoryLimitPolicyToString(TileMemoryLimitPolicy policy) {
  switch (policy) {
  case ALLOW_NOTHING:
    return "ALLOW_NOTHING";
  case ALLOW_ABSOLUTE_MINIMUM:
    return "ALLOW_ABSOLUTE_MINIMUM";
  case ALLOW_PREPAINT_ONLY:
    return "ALLOW_PREPAINT_ONLY";
  case ALLOW_ANYTHING:
    return "ALLOW_ANYTHING";
  default:
      DCHECK(false) << "Unrecognized policy value";
      return "<unknown>";
  }
}

std::string TreePriorityToString(TreePriority prio) {
  switch (prio) {
  case SAME_PRIORITY_FOR_BOTH_TREES:
    return "SAME_PRIORITY_FOR_BOTH_TREES";
  case SMOOTHNESS_TAKES_PRIORITY:
    return "SMOOTHNESS_TAKES_PRIORITY";
  case NEW_CONTENT_TAKES_PRIORITY:
    return "NEW_CONTENT_TAKES_PRIORITY";
  default:
    DCHECK(false) << "Unrecognized priority value " << prio;
    return "<unknown>";
  }
}

void GlobalStateThatImpactsTilePriority::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetString("memory_limit_policy",
                   TileMemoryLimitPolicyToString(memory_limit_policy));
  state->SetInteger("soft_memory_limit_in_bytes",
                    base::saturated_cast<int>(soft_memory_limit_in_bytes));
  state->SetInteger("hard_memory_limit_in_bytes",
                    base::saturated_cast<int>(hard_memory_limit_in_bytes));
  state->SetInteger("num_resources_limit",
                    base::saturated_cast<int>(num_resources_limit));
  state->SetString("tree_priority", TreePriorityToString(tree_priority));
}

}  // namespace cc
