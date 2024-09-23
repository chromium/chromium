// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILING_INTERNAL_H_
#define CC_TILES_TILING_INTERNAL_H_

#include <concepts>

#include "cc/base/tiling_data.h"
#include "cc/tiles/tile_index.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/size.h"

namespace cc::internal {

// A tile-like type needs to expose whether or not its ready to draw, as this
// influences coverage iteration.
template <typename T>
concept TileLike = requires(const T t) {
  { t.IsReadyToDraw() } -> std::same_as<bool>;
};

// For a type to be used as a tiling by TilingCoverageIterator and
// TilingSetCoverageIterator it must present a nested Tile type and a few common
// methods.
template <typename T>
concept Tiling = requires(const T t, const TileIndex& index) {
  typename T::Tile;
  requires TileLike<typename T::Tile>;

  { t.TileAt(index) } -> std::same_as<typename T::Tile*>;
  { t.contents_scale_key() } -> std::same_as<float>;
  { t.tiling_data() } -> std::same_as<const TilingData*>;
  { t.raster_size() } -> std::same_as<gfx::Size>;
  { t.raster_transform() } -> std::same_as<const gfx::AxisTransform2d&>;
};

}  // namespace cc::internal

#endif  // CC_TILES_TILING_INTERNAL_H_
