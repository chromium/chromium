// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_EVICTION_TILE_PRIORITY_QUEUE_H_
#define CC_TILES_EVICTION_TILE_PRIORITY_QUEUE_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_set_eviction_queue.h"

namespace cc {
class PrioritizedTile;

class CC_EXPORT EvictionTilePriorityQueue {
 public:
  EvictionTilePriorityQueue();
  EvictionTilePriorityQueue(const EvictionTilePriorityQueue&) = delete;
  ~EvictionTilePriorityQueue();

  EvictionTilePriorityQueue& operator=(const EvictionTilePriorityQueue&) =
      delete;

  void Build(const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
                 active_layers,
             const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
                 pending_layers,
             TreePriority tree_priority);

  bool IsEmpty() const;
  const PrioritizedTile& Top() const;
  void Pop();

 private:
  std::vector<std::unique_ptr<TilingSetEvictionQueue>>& GetNextQueues();
  const std::vector<std::unique_ptr<TilingSetEvictionQueue>>& GetNextQueues()
      const;

  std::vector<std::unique_ptr<TilingSetEvictionQueue>> active_queues_;
  std::vector<std::unique_ptr<TilingSetEvictionQueue>> pending_queues_;
  TreePriority tree_priority_;
};

}  // namespace cc

#endif  // CC_TILES_EVICTION_TILE_PRIORITY_QUEUE_H_
