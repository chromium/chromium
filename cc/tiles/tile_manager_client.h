// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_MANAGER_CLIENT_H_
#define CC_TILES_TILE_MANAGER_CLIENT_H_

#include <stddef.h>

#include <memory>

#include "cc/cc_export.h"
#include "cc/paint/target_color_params.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "ui/gfx/display_color_spaces.h"

namespace cc {

class DisplayItemList;
class EvictionTilePriorityQueue;
class PaintImage;
class Tile;
class TilesWithResourceIterator;

class CC_EXPORT TileManagerClient {
 public:
  // Called when all tiles marked as required for activation are ready to draw.
  virtual void NotifyReadyToActivate() = 0;

  // Called when all tiles marked as required for draw are ready to draw.
  virtual void NotifyReadyToDraw() = 0;

  // Called when all tile tasks started by the most recent call to PrepareTiles
  // are completed.
  virtual void NotifyAllTileTasksCompleted() = 0;

  // Called when the visible representation of a tile might have changed. Some
  // examples are:
  // - Tile version initialized.
  // - Tile resources freed.
  // - Tile marked for on-demand raster.
  virtual void NotifyTileStateChanged(const Tile* tile,
                                      bool update_damage = true) = 0;

  // Given an empty raster tile priority queue, this will build a priority queue
  // that will return tiles in the order in which they should be rasterized.
  // Note if the queue was previously built, Reset must be called on it.
  virtual std::unique_ptr<RasterTilePriorityQueue> BuildRasterQueue(
      TreePriority tree_priority,
      RasterTilePriorityQueue::Type type) = 0;

  // Given an empty eviction tile priority queue, this will build a priority
  // queue that will return tiles in the order in which they should be evicted.
  // Note if the queue was previously built, Reset must be called on it.
  virtual std::unique_ptr<EvictionTilePriorityQueue> BuildEvictionQueue(
      TreePriority tree_priority) = 0;

  // Returns an iterator over all the tiles that have a resource.
  virtual std::unique_ptr<TilesWithResourceIterator>
  CreateTilesWithResourceIterator() = 0;

  // Informs the client that due to the currently rasterizing (or scheduled to
  // be rasterized) tiles, we will be in a position that will likely require a
  // draw. This can be used to preemptively start a frame.
  virtual void SetIsLikelyToRequireADraw(bool is_likely_to_require_a_draw) = 0;

  // Requests the color parameters in which the tiles should be rasterized.
  virtual TargetColorParams GetTargetColorParams(
      gfx::ContentColorUsage content_color_usage) const = 0;

  // Returns the format to use for the tiles.
  virtual viz::SharedImageFormat GetTileFormat() const = 0;

  // Requests that a pending tree be scheduled to invalidate content on the
  // pending on active tree. This is currently used when tiles that are
  // rasterized with missing images need to be invalidated.
  virtual void RequestImplSideInvalidationForCheckerImagedTiles() = 0;

  // Returns the frame index to display for the given image on the given tree.
  virtual size_t GetFrameIndexForImage(const PaintImage& paint_image,
                                       WhichTree tree) const = 0;

  // Returns the sample count to use if MSAA is enabled for a tile.
  virtual int GetMSAASampleCountForRaster(
      const DisplayItemList& display_list) const = 0;

  // True if there is a pending tree.
  virtual bool HasPendingTree() = 0;

 protected:
  virtual ~TileManagerClient() {}
};

}  // namespace cc

#endif  // CC_TILES_TILE_MANAGER_CLIENT_H_
