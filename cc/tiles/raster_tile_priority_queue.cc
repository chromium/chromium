// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/raster_tile_priority_queue.h"

#include "base/notreached.h"
#include "cc/tiles/raster_tile_priority_queue_all.h"
#include "cc/tiles/raster_tile_priority_queue_required.h"

namespace cc {

// static
std::unique_ptr<RasterTilePriorityQueue> RasterTilePriorityQueue::Create(
    const std::vector<PictureLayerImpl*>& active_layers,
    const std::vector<PictureLayerImpl*>& pending_layers,
    TreePriority tree_priority,
    Type type) {
  switch (type) {
    case Type::ALL: {
      std::unique_ptr<RasterTilePriorityQueueAll> queue(
          new RasterTilePriorityQueueAll);
      queue->Build(active_layers, pending_layers, tree_priority);
      return std::move(queue);
    }
    case Type::REQUIRED_FOR_ACTIVATION:
    case Type::REQUIRED_FOR_DRAW: {
      std::unique_ptr<RasterTilePriorityQueueRequired> queue(
          new RasterTilePriorityQueueRequired);
      queue->Build(active_layers, pending_layers, type);
      return std::move(queue);
    }
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace cc
