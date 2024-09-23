// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILING_SET_RASTER_QUEUE_REQUIRED_H_
#define CC_TILES_TILING_SET_RASTER_QUEUE_REQUIRED_H_

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "cc/cc_export.h"
#include "cc/tiles/picture_layer_tiling_set.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/tile.h"

namespace cc {

// This queue only returns tiles that are required for either activation or
// draw, as specified by RasterTilePriorityQueue::Type passed in the
// constructor.
class CC_EXPORT TilingSetRasterQueueRequired {
 public:
  static std::unique_ptr<TilingSetRasterQueueRequired> Create(
      PictureLayerTilingSet* tiling_set,
      RasterTilePriorityQueue::Type type);
  ~TilingSetRasterQueueRequired();

  const PrioritizedTile& Top() const;
  void Pop();
  bool IsEmpty() const;

 private:
  // This iterator will return all tiles that are in the NOW bin on the given
  // tiling. The queue can then use these tiles and further filter them based on
  // whether they are required or not.
  class TilingIterator {
   public:
    TilingIterator();
    explicit TilingIterator(PictureLayerTiling* tiling,
                            TilingData* tiling_data,
                            const gfx::Rect& rect);
    ~TilingIterator();

    bool done() const { return !current_tile_.tile(); }
    const PrioritizedTile& operator*() const { return current_tile_; }
    TilingIterator& operator++();

   private:
    // `tiling_` and `tiling_data_` are not a raw_ptr<...> for performance
    // reasons (based on analysis of sampling profiler data and
    // tab_search:top100:2020).
    RAW_PTR_EXCLUSION PictureLayerTiling* tiling_;
    RAW_PTR_EXCLUSION TilingData* tiling_data_;

    PrioritizedTile current_tile_;
    TilingData::Iterator visible_iterator_;
  };

  TilingSetRasterQueueRequired();
  TilingSetRasterQueueRequired(PictureLayerTiling* tiling,
                               RasterTilePriorityQueue::Type type);

  bool IsTileRequired(const PrioritizedTile& prioritized_tile) const;

  TilingIterator iterator_;
  RasterTilePriorityQueue::Type type_;
};

}  // namespace cc

#endif  // CC_TILES_TILING_SET_RASTER_QUEUE_REQUIRED_H_
