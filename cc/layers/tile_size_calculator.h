// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILE_SIZE_CALCULATOR_H_
#define CC_LAYERS_TILE_SIZE_CALCULATOR_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class PictureLayerImpl;

// This class calculates the tile size only when the |affecting_params_|
// or |content_bounds_| is changed.
class CC_EXPORT TileSizeCalculator {
 public:
  explicit TileSizeCalculator(PictureLayerImpl* layer_impl);

  gfx::Size CalculateTileSize(gfx::Size content_bounds);

 private:
  struct AffectingParams {
    int max_texture_size = 0;
    bool use_gpu_rasterization = false;
    float device_scale_factor = 0.0f;
    gfx::Size max_tile_size;
    int min_height_for_gpu_raster_tile;
    gfx::Size gpu_raster_max_texture_size;
    gfx::Size max_untiled_layer_size;
    gfx::Size default_tile_size;
    gfx::Size content_bounds;

    bool operator==(const AffectingParams& other) const;
  };

  PictureLayerImpl* layer_impl() const { return layer_impl_; }
  AffectingParams GetAffectingParams(gfx::Size content_bounds) const;
  bool UpdateAffectingParams(gfx::Size content_bounds);

  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION PictureLayerImpl* layer_impl_ = nullptr;
  const bool is_using_raw_draw_;
  const double raw_draw_tile_size_factor_;

  AffectingParams affecting_params_;

  gfx::Size tile_size_;
};

}  // namespace cc

#endif  // CC_LAYERS_TILE_SIZE_CALCULATOR_H_
