// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/eviction_tile_priority_queue.h"

#include "base/memory/raw_ptr.h"

namespace cc {

namespace {

class EvictionOrderComparator {
 public:
  explicit EvictionOrderComparator(TreePriority tree_priority)
      : tree_priority_(tree_priority) {}

  // Note that in this function, we have to return true if and only if
  // b is strictly lower priority than a.
  bool operator()(
      const std::unique_ptr<TilingSetEvictionQueue>& a_queue,
      const std::unique_ptr<TilingSetEvictionQueue>& b_queue) const {
    const PrioritizedTile& a_tile = a_queue->Top();
    const PrioritizedTile& b_tile = b_queue->Top();

    const TilePriority& a_priority = a_tile.priority();
    const TilePriority& b_priority = b_tile.priority();
    bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

    // If the priority bin is the same but one of the tiles is from a
    // non-drawing layer, then the drawing layer has a higher priority.
    if (b_priority.priority_bin == a_priority.priority_bin &&
        b_queue->is_drawing_layer() != a_queue->is_drawing_layer()) {
      return a_queue->is_drawing_layer();
    }

    // If the priority bin differs, b is lower priority if it has the higher
    // priority bin.
    if (a_priority.priority_bin != b_priority.priority_bin)
      return b_priority.priority_bin > a_priority.priority_bin;

    // Otherwise if the resolution differs, then the order will be determined by
    // whether we prioritize low res or not.
    // TODO(vmpstr): Remove this when TilePriority is no longer a member of Tile
    // class but instead produced by the iterators.
    if (b_priority.resolution != a_priority.resolution) {
      // Non ideal resolution should be sorted higher than other resolutions.
      if (a_priority.resolution == NON_IDEAL_RESOLUTION)
        return false;

      if (b_priority.resolution == NON_IDEAL_RESOLUTION)
        return true;

      if (prioritize_low_res)
        return a_priority.resolution == LOW_RESOLUTION;
      return a_priority.resolution == HIGH_RESOLUTION;
    }

    // Otherwise if the occlusion differs, b is lower priority if it is
    // occluded.
    bool a_is_occluded = a_tile.is_occluded();
    bool b_is_occluded = b_tile.is_occluded();
    if (a_is_occluded != b_is_occluded)
      return b_is_occluded;

    // b is lower priorty if it is farther from visible.
    return b_priority.distance_to_visible > a_priority.distance_to_visible;
  }

 private:
  TreePriority tree_priority_;
};

void CreateTilingSetEvictionQueues(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>& layers,
    TreePriority tree_priority,
    std::vector<std::unique_ptr<TilingSetEvictionQueue>>* queues) {
  DCHECK(queues->empty());

  for (PictureLayerImpl* layer : layers) {
    std::unique_ptr<TilingSetEvictionQueue> tiling_set_queue =
        std::make_unique<TilingSetEvictionQueue>(
            layer->picture_layer_tiling_set(),
            layer->contributes_to_drawn_render_surface());
    // Queues will only contain non empty tiling sets.
    if (!tiling_set_queue->IsEmpty())
      queues->push_back(std::move(tiling_set_queue));
  }
  std::make_heap(queues->begin(), queues->end(),
                 EvictionOrderComparator(tree_priority));
}

}  // namespace

EvictionTilePriorityQueue::EvictionTilePriorityQueue() = default;

EvictionTilePriorityQueue::~EvictionTilePriorityQueue() = default;

void EvictionTilePriorityQueue::Build(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        active_layers,
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        pending_layers,
    TreePriority tree_priority) {
  tree_priority_ = tree_priority;

  CreateTilingSetEvictionQueues(active_layers, tree_priority, &active_queues_);
  CreateTilingSetEvictionQueues(pending_layers, tree_priority,
                                &pending_queues_);
}

bool EvictionTilePriorityQueue::IsEmpty() const {
  return active_queues_.empty() && pending_queues_.empty();
}

const PrioritizedTile& EvictionTilePriorityQueue::Top() const {
  DCHECK(!IsEmpty());
  const auto& next_queues = GetNextQueues();
  return next_queues.front()->Top();
}

void EvictionTilePriorityQueue::Pop() {
  DCHECK(!IsEmpty());

  auto& next_queues = GetNextQueues();
  std::pop_heap(next_queues.begin(), next_queues.end(),
                EvictionOrderComparator(tree_priority_));
  TilingSetEvictionQueue* queue = next_queues.back().get();
  queue->Pop();

  // Remove empty queues.
  if (queue->IsEmpty()) {
    next_queues.pop_back();
  } else {
    std::push_heap(next_queues.begin(), next_queues.end(),
                   EvictionOrderComparator(tree_priority_));
  }
}

std::vector<std::unique_ptr<TilingSetEvictionQueue>>&
EvictionTilePriorityQueue::GetNextQueues() {
  const EvictionTilePriorityQueue* const_this =
      static_cast<const EvictionTilePriorityQueue*>(this);
  const auto& const_queues = const_this->GetNextQueues();
  return const_cast<std::vector<std::unique_ptr<TilingSetEvictionQueue>>&>(
      const_queues);
}

const std::vector<std::unique_ptr<TilingSetEvictionQueue>>&
EvictionTilePriorityQueue::GetNextQueues() const {
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

  // If the bins are the same and activation differs, then return the tree of
  // the tile not required for activation.
  if (active_priority.priority_bin == pending_priority.priority_bin &&
      active_tile.tile()->required_for_activation() !=
          pending_tile.tile()->required_for_activation()) {
    return active_tile.tile()->required_for_activation() ? pending_queues_
                                                         : active_queues_;
  }

  // Return tile with a lower priority.
  if (pending_priority.IsHigherPriorityThan(active_priority))
    return active_queues_;
  return pending_queues_;
}

}  // namespace cc
