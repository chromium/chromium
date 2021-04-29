// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILE_SIZE_CALCULATOR_H_
#define CC_LAYERS_TILE_SIZE_CALCULATOR_H_

#include "cc/cc_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class PictureLayerImpl;

// This class calculates the tile size only when the |affecting_params_|
// or |content_bounds_| is changed.
class CC_EXPORT TileSizeCalculator {
 public:
  explicit TileSizeCalculator(PictureLayerImpl* layer_impl);

  gfx::Size CalculateTileSize();

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
    gfx::Size layer_content_bounds;

    bool operator==(const AffectingParams& other);
  };

  PictureLayerImpl* layer_impl() const { return layer_impl_; }
  AffectingParams GetAffectingParams();
  bool IsAffectingParamsChanged();

  PictureLayerImpl* layer_impl_;

  AffectingParams affecting_params_;

  gfx::Size tile_size_;
};

}  // namespace cc

#endif  // CC_LAYERS_TILE_SIZE_CALCULATOR_H_
