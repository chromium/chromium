// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/raster_tile_priority_queue_all.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "cc/base/features.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"

namespace cc {

namespace {

class RasterOrderComparator {
 public:
  explicit RasterOrderComparator(TreePriority tree_priority)
      : tree_priority_(tree_priority) {}

  // Note that in this function, we have to return true if and only if
  // a is strictly lower priority than b.
  bool operator()(
      const std::unique_ptr<TilingSetRasterQueueAll>& a_queue,
      const std::unique_ptr<TilingSetRasterQueueAll>& b_queue) const {
    const TilePriority& a_priority = a_queue->Top().priority();
    const TilePriority& b_priority = b_queue->Top().priority();
    bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

    // If the priority bin is the same but one of the tiles is from a
    // non-drawing layer, then the drawing layer has a higher priority.
    if (b_priority.priority_bin == a_priority.priority_bin &&
        b_queue->is_drawing_layer() != a_queue->is_drawing_layer()) {
      return b_queue->is_drawing_layer();
    }

    // If the bin is the same but the resolution is not, then the order will be
    // determined by whether we prioritize low res or not.
    // TODO(vmpstr): Remove this when TilePriority is no longer a member of Tile
    // class but instead produced by the iterators.
    if (b_priority.priority_bin == a_priority.priority_bin &&
        b_priority.resolution != a_priority.resolution) {
      // Non ideal resolution should be sorted lower than other resolutions.
      if (a_priority.resolution == NON_IDEAL_RESOLUTION)
        return true;

      if (b_priority.resolution == NON_IDEAL_RESOLUTION)
        return false;

      if (prioritize_low_res)
        return b_priority.resolution == LOW_RESOLUTION;
      return b_priority.resolution == HIGH_RESOLUTION;
    }

    return b_priority.IsHigherPriorityThan(a_priority);
  }

 private:
  TreePriority tree_priority_;
};

void CreateTilingSetRasterQueues(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>& layers,
    TreePriority tree_priority,
    std::vector<std::unique_ptr<TilingSetRasterQueueAll>>* queues) {
  DCHECK(queues->empty());

  const bool cc_slimming_enabled = features::IsCCSlimmingEnabled();
  for (PictureLayerImpl* layer : layers) {
    if (!layer->HasValidTilePriorities())
      continue;

    PictureLayerTilingSet* tiling_set = layer->picture_layer_tiling_set();
    if (cc_slimming_enabled && tiling_set->all_tiles_done()) {
      continue;
    }
    bool prioritize_low_res = tree_priority == SMOOTHNESS_TAKES_PRIORITY;
    std::unique_ptr<TilingSetRasterQueueAll> tiling_set_queue =
        TilingSetRasterQueueAll::Create(
            tiling_set, prioritize_low_res,
            layer->contributes_to_drawn_render_surface());
    // Queues will only contain non empty tiling sets.
    if (tiling_set_queue && !tiling_set_queue->IsEmpty()) {
      queues->push_back(std::move(tiling_set_queue));
    }
  }
  std::make_heap(queues->begin(), queues->end(),
                 RasterOrderComparator(tree_priority));
}

}  // namespace

RasterTilePriorityQueueAll::RasterTilePriorityQueueAll() = default;
RasterTilePriorityQueueAll::~RasterTilePriorityQueueAll() = default;

void RasterTilePriorityQueueAll::Build(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        active_layers,
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        pending_layers,
    TreePriority tree_priority) {
  tree_priority_ = tree_priority;

  CreateTilingSetRasterQueues(active_layers, tree_priority_, &active_queues_);
  CreateTilingSetRasterQueues(pending_layers, tree_priority_, &pending_queues_);
}

bool RasterTilePriorityQueueAll::IsEmpty() const {
  return active_queues_.empty() && pending_queues_.empty();
}

const PrioritizedTile& RasterTilePriorityQueueAll::Top() const {
  DCHECK(!IsEmpty());
  const auto& next_queues = GetNextQueues();
  return next_queues.front()->Top();
}

void RasterTilePriorityQueueAll::Pop() {
  DCHECK(!IsEmpty());

  auto& next_queues = GetNextQueues();
  std::pop_heap(next_queues.begin(), next_queues.end(),
                RasterOrderComparator(tree_priority_));
  TilingSetRasterQueueAll* queue = next_queues.back().get();
  queue->Pop();

  // Remove empty queues.
  if (queue->IsEmpty()) {
    next_queues.pop_back();
  } else {
    std::push_heap(next_queues.begin(), next_queues.end(),
                   RasterOrderComparator(tree_priority_));
  }
}

std::vector<std::unique_ptr<TilingSetRasterQueueAll>>&
RasterTilePriorityQueueAll::GetNextQueues() {
  const auto* const_this = static_cast<const RasterTilePriorityQueueAll*>(this);
  const auto& const_queues = const_this->GetNextQueues();
  return const_cast<std::vector<std::unique_ptr<TilingSetRasterQueueAll>>&>(
      const_queues);
}

const std::vector<std::unique_ptr<TilingSetRasterQueueAll>>&
RasterTilePriorityQueueAll::GetNextQueues() const {
  DCHECK(!IsEmpty());

  // If we only have one queue with tiles, return it.
  if (active_queues_.empty())
    return pending_queues_;
  if (pending_queues_.empty())
    return active_queues_;

  const PrioritizedTile& active_tile = active_queues_.front()->Top();
  const PrioritizedTile& pending_tile = pending_queues_.front()->Top();

  const TilePriority& active_priority = active_tile.priority();
  const TilePriority& pending_priority = pending_tile.priority();

  // Priority rule:
  // - SMOOTHNESS_TAKES_PRIORITY: Active NOW before pending NOW; same as all
  // mode for other bins.
  // - NEW_CONTENT_TAKES_PRIORITY: Pending NOW before active NOW; same as all
  // mode for other bins.
  // - SAME_PRIORITY_FOR_BOTH_TREES (All): Calling IsHigherPriorityThan().
  // Notes: This priority rule should not break
  // TileManager::TilePriorityViolatesMemoryPolicy().

  // Prioritize the highest priority_bin NOW out of either one of active or
  // pending for smoothness and new content modes.
  if (pending_priority.priority_bin == TilePriority::NOW &&
      active_priority.priority_bin == TilePriority::NOW) {
    if (tree_priority_ == SMOOTHNESS_TAKES_PRIORITY) {
      return active_queues_;
    }
    if (tree_priority_ == NEW_CONTENT_TAKES_PRIORITY) {
      return pending_queues_;
    }
  }

  // Then, use the IsHigherPriorityThan condition for
  // SAME_PRIORITY_FOR_BOTH_TREES and the rest of the priority bins.
  // TODO(crbug.com/40244895): For SAME_PRIORITY_FOR_BOTH_TREES mode and both
  // being NOW, should we give the priority to Active NOW instead?
  if (active_priority.IsHigherPriorityThan(pending_priority)) {
    return active_queues_;
  }
  return pending_queues_;
}

}  // namespace cc
