// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_PRIORITIZED_TILE_H_
#define CC_TILES_PRIORITIZED_TILE_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "cc/cc_export.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/raster/raster_source.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"

namespace cc {
class RasterSource;
class PictureLayerTiling;

class CC_EXPORT PrioritizedTile {
 public:
  // This class is constructed and returned by a |PictureLayerTiling|, and
  // represents a tile and its priority.
  PrioritizedTile();
  PrioritizedTile(Tile* tile,
                  const PictureLayerTiling* source_tiling,
                  const TilePriority& priority,
                  bool is_occluded,
                  bool is_process_for_images_only,
                  bool should_decode_checkered_images_for_tile);
  ~PrioritizedTile();

  Tile* tile() const { return tile_; }
  const scoped_refptr<RasterSource>& raster_source() const {
    return source_tiling_->raster_source();
  }
  const PaintWorkletRecordMap& GetPaintWorkletRecords() const {
    return source_tiling_->GetPaintWorkletRecords();
  }
  ScrollOffsetMap GetRasterInducingScrollOffsets() const {
    return source_tiling_->GetRasterInducingScrollOffsets();
  }
  const TilePriority& priority() const { return priority_; }
  bool is_occluded() const { return is_occluded_; }
  bool is_process_for_images_only() const {
    return is_process_for_images_only_;
  }
  bool should_decode_checkered_images_for_tile() const {
    return should_decode_checkered_images_for_tile_;
  }

  void AsValueInto(base::trace_event::TracedValue* value) const;

  const PictureLayerTiling* source_tiling() const { return source_tiling_; }

 private:
  // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
  // stacks.
  RAW_PTR_EXCLUSION Tile* tile_ = nullptr;
  RAW_PTR_EXCLUSION const PictureLayerTiling* source_tiling_ = nullptr;
  TilePriority priority_;
  bool is_occluded_ = false;
  bool is_process_for_images_only_ = false;
  bool should_decode_checkered_images_for_tile_ = false;
};

}  // namespace cc

#endif  // CC_TILES_PRIORITIZED_TILE_H_
