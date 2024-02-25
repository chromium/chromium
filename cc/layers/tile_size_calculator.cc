// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_size_calculator.h"

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/base/ui_base_features.h"

namespace cc {
namespace {

// When making odd-sized tiles, round them up to increase the chances
// of using the same tile size.
const int kTileRoundUp = 64;

// Round GPU default tile sizes to a multiple of 32. This helps prevent
// rounding errors during compositing.
const int kGpuDefaultTileRoundUp = 32;

// For performance reasons and to support compressed tile textures, tile
// width and height should be an even multiple of 4 in size.
const int kTileMinimalAlignment = 4;

// This function converts the given |device_pixels_size| to the expected size
// of content which was generated to fill it at 100%.  This takes into account
// the ceil operations that occur as device pixels are converted to/from DIPs
// (content size must be a whole number of DIPs).
gfx::Size ApplyDsfAdjustment(const gfx::Size& device_pixels_size, float dsf) {
  gfx::Size content_size_in_dips =
      gfx::ScaleToCeiledSize(device_pixels_size, 1.0f / dsf);
  gfx::Size content_size_in_dps =
      gfx::ScaleToCeiledSize(content_size_in_dips, dsf);
  return content_size_in_dps;
}

gfx::Size AdjustGpuTileSize(int tile_width,
                            int tile_height,
                            const gfx::Size& max_tile_size,
                            int min_height_for_gpu_raster_tile) {
  // Grow default sizes to account for overlapping border texels.
  tile_width += 2 * PictureLayerTiling::kBorderTexels;
  tile_height += 2 * PictureLayerTiling::kBorderTexels;

  // Round GPU default tile sizes to a multiple of kGpuDefaultTileAlignment.
  // This helps prevent rounding errors in our CA path. https://crbug.com/632274
  tile_width = MathUtil::UncheckedRoundUp(tile_width, kGpuDefaultTileRoundUp);
  tile_height = MathUtil::UncheckedRoundUp(tile_height, kGpuDefaultTileRoundUp);

  tile_height = std::max(tile_height, min_height_for_gpu_raster_tile);

  if (!max_tile_size.IsEmpty()) {
    tile_width = std::min(tile_width, max_tile_size.width());
    tile_height = std::min(tile_height, max_tile_size.height());
  }

  return gfx::Size(tile_width, tile_height);
}

// For GPU rasterization, we pick an ideal tile size using the viewport so we
// don't need any settings. The current approach uses 4 tiles to cover the
// viewport vertically.
gfx::Size CalculateGpuTileSize(const gfx::Size& base_tile_size,
                               const gfx::Size& content_bounds,
                               const gfx::Size& max_tile_size,
                               int min_height_for_gpu_raster_tile) {
  int tile_width = base_tile_size.width();

  // Increase the height proportionally as the width decreases, and pad by our
  // border texels to make the tiles exactly match the viewport.
  int divisor = 4;
  if (content_bounds.width() <= base_tile_size.width() / 2)
    divisor = 2;
  if (content_bounds.width() <= base_tile_size.width() / 4)
    divisor = 1;
  int tile_height =
      MathUtil::UncheckedRoundUp(base_tile_size.height(), divisor) / divisor;

  return AdjustGpuTileSize(tile_width, tile_height, max_tile_size,
                           min_height_for_gpu_raster_tile);
}

gfx::Size CalculateGpuRawDrawTileSize(const gfx::Size& base_tile_size,
                                      const gfx::Size& content_bounds,
                                      const gfx::Size& max_tile_size,
                                      int min_height_for_gpu_raster_tile,
                                      double raw_draw_tile_size_factor) {
  // Sometime the |base_tile_size| could be (0x0) or (1x1), so set a min base
  // tile size, to avoid incorrect calculation.
  // Use 2280 (mid range phone screen height) as the min base tile size for now.
  // TODO(penghuang): find better numbers for different platforms.
  constexpr int kMinBaseTileSize = 2280;
  int tile_size = std::max(
      {base_tile_size.width(), base_tile_size.height(), kMinBaseTileSize});
  tile_size = std::ceil(tile_size * raw_draw_tile_size_factor);

  // If the content area is not greater than the calculated tile area, then
  // content bounds is used for the tile size.
  // Sometime |content_bounds| is longer than |tile_size| in one direction, but
  // it is much shorter in the other direction. In that case, we don't want to
  // split the content into several very small tiles.
  if (content_bounds.Area64() <=
      static_cast<uint64_t>(tile_size) * static_cast<uint64_t>(tile_size)) {
    return AdjustGpuTileSize(content_bounds.width(), content_bounds.height(),
                             max_tile_size, min_height_for_gpu_raster_tile);
  }

  // Clamp tile size with content bounds
  int tile_width = std::min(tile_size, content_bounds.width());
  int tile_height = std::min(tile_size, content_bounds.height());

  return AdjustGpuTileSize(tile_width, tile_height, max_tile_size,
                           min_height_for_gpu_raster_tile);
}

}  // namespace

// AffectingParams.
bool TileSizeCalculator::AffectingParams::operator==(
    const AffectingParams& other) const = default;

// TileSizeCalculator.
TileSizeCalculator::TileSizeCalculator(PictureLayerImpl* layer_impl)
    : layer_impl_(layer_impl),
      is_using_raw_draw_(features::IsUsingRawDraw()),
      raw_draw_tile_size_factor_(features::RawDrawTileSizeFactor()) {}

bool TileSizeCalculator::UpdateAffectingParams(gfx::Size content_bounds) {
  AffectingParams new_params = GetAffectingParams(content_bounds);

  if (affecting_params_ == new_params)
    return false;

  affecting_params_ = new_params;
  return true;
}

TileSizeCalculator::AffectingParams TileSizeCalculator::GetAffectingParams(
    gfx::Size content_bounds) const {
  AffectingParams params;
  LayerTreeImpl* layer_tree_impl = layer_impl()->layer_tree_impl();
  params.max_texture_size = layer_tree_impl->max_texture_size();
  params.use_gpu_rasterization = layer_tree_impl->use_gpu_rasterization();
  params.max_tile_size = layer_tree_impl->settings().max_gpu_raster_tile_size;
  params.min_height_for_gpu_raster_tile =
      layer_tree_impl->settings().min_height_for_gpu_raster_tile;
  params.gpu_raster_max_texture_size =
      layer_impl()->gpu_raster_max_texture_size();
  params.device_scale_factor = layer_tree_impl->device_scale_factor();
  params.max_untiled_layer_size =
      layer_tree_impl->settings().max_untiled_layer_size;
  params.default_tile_size = layer_tree_impl->settings().default_tile_size;
  params.content_bounds = content_bounds;
  return params;
}

gfx::Size TileSizeCalculator::CalculateTileSize(gfx::Size content_bounds) {
  if (layer_impl()->is_backdrop_filter_mask()) {
    // Backdrop filter masks are not tiled, so if we can't cover the whole mask
    // with one tile, we shouldn't have such a tiling at all.
    DCHECK_LE(content_bounds.width(),
              layer_impl()->layer_tree_impl()->max_texture_size());
    DCHECK_LE(content_bounds.height(),
              layer_impl()->layer_tree_impl()->max_texture_size());
    return content_bounds;
  }

  // If |affecting_params_| is already computed and not changed, return
  // pre-calculated tile size.
  if (!UpdateAffectingParams(content_bounds)) {
    return tile_size_;
  }

  int default_tile_width = 0;
  int default_tile_height = 0;
  if (affecting_params_.use_gpu_rasterization) {
    const gfx::Size& max_tile_size = affecting_params_.max_tile_size;

    // Calculate |base_tile_size| based on |gpu_raster_max_texture_size|,
    // adjusting for ceil operations that may occur due to DSF.
    gfx::Size base_tile_size =
        ApplyDsfAdjustment(affecting_params_.gpu_raster_max_texture_size,
                           affecting_params_.device_scale_factor);

    // Set our initial size assuming a |base_tile_size| equal to our
    // |viewport_size|.
    gfx::Size default_tile_size;
    if (is_using_raw_draw_) {
      default_tile_size = CalculateGpuRawDrawTileSize(
          base_tile_size, content_bounds, max_tile_size,
          affecting_params_.min_height_for_gpu_raster_tile,
          raw_draw_tile_size_factor_);
    } else {
      default_tile_size = CalculateGpuTileSize(
          base_tile_size, content_bounds, max_tile_size,
          affecting_params_.min_height_for_gpu_raster_tile);

      // Use half-width GPU tiles when the content_width is greater than our
      // calculated tile size.
      if (content_bounds.width() > default_tile_size.width()) {
        // Divide width by 2 and round up.
        base_tile_size.set_width((base_tile_size.width() + 1) / 2);
        default_tile_size = CalculateGpuTileSize(
            base_tile_size, content_bounds, max_tile_size,
            affecting_params_.min_height_for_gpu_raster_tile);
      }
    }

    default_tile_width = default_tile_size.width();
    default_tile_height = default_tile_size.height();
  } else {
    // For CPU rasterization we use tile-size settings.
    int max_untiled_content_width =
        affecting_params_.max_untiled_layer_size.width();
    int max_untiled_content_height =
        affecting_params_.max_untiled_layer_size.height();
    default_tile_width = affecting_params_.default_tile_size.width();
    default_tile_height = affecting_params_.default_tile_size.height();

    // If the content width is small, increase tile size vertically.
    // If the content height is small, increase tile size horizontally.
    // If both are less than the untiled-size, use a single tile.
    if (content_bounds.width() < default_tile_width)
      default_tile_height = max_untiled_content_height;
    if (content_bounds.height() < default_tile_height)
      default_tile_width = max_untiled_content_width;
    if (content_bounds.width() < max_untiled_content_width &&
        content_bounds.height() < max_untiled_content_height) {
      default_tile_height = max_untiled_content_height;
      default_tile_width = max_untiled_content_width;
    }
  }

  int tile_width = default_tile_width;
  int tile_height = default_tile_height;

  // Clamp the tile width/height to the content width/height to save space.
  if (content_bounds.width() < default_tile_width) {
    tile_width =
        MathUtil::UncheckedRoundUp(content_bounds.width(), kTileRoundUp);
    tile_width = std::min(tile_width, default_tile_width);
  }
  if (content_bounds.height() < default_tile_height) {
    tile_height =
        MathUtil::UncheckedRoundUp(content_bounds.height(), kTileRoundUp);
    tile_height = std::min(tile_height, default_tile_height);
  }

  // Ensure that tile width and height are properly aligned.
  tile_width = MathUtil::UncheckedRoundUp(tile_width, kTileMinimalAlignment);
  tile_height = MathUtil::UncheckedRoundUp(tile_height, kTileMinimalAlignment);

  // Under no circumstance should we be larger than the max texture size.
  tile_width = std::min(tile_width, affecting_params_.max_texture_size);
  tile_height = std::min(tile_height, affecting_params_.max_texture_size);

  // Store the calculated tile size.
  tile_size_ = gfx::Size(tile_width, tile_height);

  return tile_size_;
}

}  // namespace cc
