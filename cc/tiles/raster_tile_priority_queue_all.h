// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_
#define CC_TILES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"

namespace cc {

class CC_EXPORT RasterTilePriorityQueueAll : public RasterTilePriorityQueue {
 public:
  RasterTilePriorityQueueAll();
  RasterTilePriorityQueueAll(const RasterTilePriorityQueueAll&) = delete;
  ~RasterTilePriorityQueueAll() override;

  RasterTilePriorityQueueAll& operator=(const RasterTilePriorityQueueAll&) =
      delete;

  bool IsEmpty() const override;
  const PrioritizedTile& Top() const override;
  void Pop() override;

 private:
  friend class RasterTilePriorityQueue;

  void Build(const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
                 active_layers,
             const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
                 pending_layers,
             TreePriority tree_priority);

  std::vector<std::unique_ptr<TilingSetRasterQueueAll>>& GetNextQueues();
  const std::vector<std::unique_ptr<TilingSetRasterQueueAll>>& GetNextQueues()
      const;

  std::vector<std::unique_ptr<TilingSetRasterQueueAll>> active_queues_;
  std::vector<std::unique_ptr<TilingSetRasterQueueAll>> pending_queues_;
  TreePriority tree_priority_;
};

}  // namespace cc

#endif  // CC_TILES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_
