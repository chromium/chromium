// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_RASTER_TILE_PRIORITY_QUEUE_REQUIRED_H_
#define CC_TILES_RASTER_TILE_PRIORITY_QUEUE_REQUIRED_H_

#include <vector>

#include "cc/layers/picture_layer_impl.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/tiling_set_raster_queue_required.h"

namespace cc {
class PrioritizedTile;

class RasterTilePriorityQueueRequired : public RasterTilePriorityQueue {
 public:
  RasterTilePriorityQueueRequired();
  RasterTilePriorityQueueRequired(const RasterTilePriorityQueueRequired&) =
      delete;
  ~RasterTilePriorityQueueRequired() override;

  RasterTilePriorityQueueRequired& operator=(
      const RasterTilePriorityQueueRequired&) = delete;

  bool IsEmpty() const override;
  const PrioritizedTile& Top() const override;
  void Pop() override;

 private:
  friend class RasterTilePriorityQueue;

  void Build(const std::vector<PictureLayerImpl*>& active_layers,
             const std::vector<PictureLayerImpl*>& pending_layers,
             Type type);
  void BuildRequiredForDraw(
      const std::vector<PictureLayerImpl*>& active_layers);
  void BuildRequiredForActivation(
      const std::vector<PictureLayerImpl*>& active_layers,
      const std::vector<PictureLayerImpl*>& pending_layers);

  std::vector<std::unique_ptr<TilingSetRasterQueueRequired>> tiling_set_queues_;
};

}  // namespace cc

#endif  // CC_TILES_RASTER_TILE_PRIORITY_QUEUE_REQUIRED_H_
