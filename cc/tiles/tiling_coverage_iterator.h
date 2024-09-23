// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILING_COVERAGE_ITERATOR_H_
#define CC_TILES_TILING_COVERAGE_ITERATOR_H_

#include <algorithm>
#include <concepts>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/base/tiling_data.h"
#include "cc/cc_export.h"
#include "cc/tiles/tile_index.h"
#include "cc/tiles/tiling_internal.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// TilingCoverageIterator iterates over a generic tiling to expose the minimal
// set of tiles required to cover a given content rectangle.
//
// Iteration terminates once either the content area has been fully covered by
// by visited tiles, or all applicable tiles in the tiling have been visited.
template <typename T>
  requires internal::Tiling<T>
class CC_EXPORT TilingCoverageIterator {
 public:
  using Tile = typename T::Tile;

  TilingCoverageIterator() = default;

  // Constructs an iterable coverage view for `tiling` which attempts to fully
  // cover the content area given by `coverage_rect`, a rectangle that has been
  // pre-scaled by `coverage_scale` relative to layer space.
  TilingCoverageIterator(const T* tiling,
                         float coverage_scale,
                         const gfx::Rect& coverage_rect)
      : tiling_(tiling),
        coverage_rect_max_bounds_(
            ComputeCoverageRectMaxBounds(*tiling,
                                         tiling->raster_size(),
                                         coverage_scale)),
        coverage_rect_(
            gfx::IntersectRects(coverage_rect, coverage_rect_max_bounds_)),
        coverage_to_content_(
            gfx::PreScaleAxisTransform2d(tiling->raster_transform(),
                                         1 / coverage_scale)) {
    if (coverage_rect_.IsEmpty()) {
      return;
    }
    const gfx::Rect wanted_texels =
        ComputeWantedTexels(coverage_to_content_, coverage_rect_);
    const TilingData& data = *tiling->tiling_data();
    top_left_.i = data.LastBorderTileXIndexFromSrcCoord(wanted_texels.x());
    top_left_.j = data.LastBorderTileYIndexFromSrcCoord(wanted_texels.y());
    bottom_right_.i =
        1 +
        std::max(data.FirstBorderTileXIndexFromSrcCoord(wanted_texels.right()),
                 top_left_.i);
    bottom_right_.j =
        1 +
        std::max(data.FirstBorderTileYIndexFromSrcCoord(wanted_texels.bottom()),
                 top_left_.j);
    index_ = top_left_;
    AdvanceUntilTileIsRelevant();
  }

  TilingCoverageIterator(const TilingCoverageIterator&) = default;
  TilingCoverageIterator& operator=(const TilingCoverageIterator&) = default;
  ~TilingCoverageIterator() = default;

  // Returns true if and only if this iterator has been initialized for a
  // specific tiling and has not yet advanced to the end of its coverage. Other
  // methods on this object may only be called when this returns true, and the
  // value returned here may only change after assigning or incrementing the
  // iterator.
  bool IsValid() const { return index_.j < bottom_right_.j; }
  explicit operator bool() const { return IsValid(); }

  // Advances the iterator to the next unvisited tile which covers some portion
  // of the coverage rect.
  TilingCoverageIterator& operator++() {
    if (IsValid()) {
      IncrementIndex();
      AdvanceUntilTileIsRelevant();
    }
    return *this;
  }

  // The index of the current tile.
  const TileIndex& index() const { return index_; }
  int i() const { return index_.i; }
  int j() const { return index_.j; }

  // The current tile.
  Tile* operator*() const { return current_tile_; }
  Tile* operator->() const { return current_tile_; }

  // The rect covered by the current tile within the space of the coverage rect.
  const gfx::Rect& geometry_rect() const { return geometry_rect_; }

  // The rect in texture space of the current tile's intersection with the
  // coverage rect.
  gfx::RectF texture_rect() const {
    auto tex_origin = gfx::PointF(tiling_->tiling_data()
                                      ->TileBoundsWithBorder(index_.i, index_.j)
                                      .origin());

    // Convert from coverage space => content space => texture space.
    gfx::RectF texture_rect =
        coverage_to_content_.MapRect(gfx::RectF(geometry_rect_));
    texture_rect.Offset(-tex_origin.OffsetFromOrigin());
    return texture_rect;
  }

 private:
  static gfx::Rect ComputeCoverageRectMaxBounds(const T& tiling,
                                                const gfx::Size& layer_bounds,
                                                float coverage_scale) {
    gfx::Rect tiling_rect_in_layer_space =
        gfx::ToEnclosingRect(tiling.raster_transform().InverseMapRect(
            gfx::RectF(tiling.tiling_data()->tiling_rect())));
    tiling_rect_in_layer_space.Intersect(gfx::Rect(layer_bounds));
    return gfx::ScaleToEnclosingRect(tiling_rect_in_layer_space,
                                     coverage_scale);
  }

  static gfx::Rect ComputeWantedTexels(
      const gfx::AxisTransform2d& coverage_to_content,
      const gfx::Rect& coverage_rect) {
    gfx::RectF content_rect =
        coverage_to_content.MapRect(gfx::RectF(coverage_rect));
    content_rect.Offset(-0.5f, -0.5f);
    return gfx::ToEnclosingRect(content_rect);
  }

  void IncrementIndex() {
    ++index_.i;
    if (index_.i >= bottom_right_.i) {
      index_.i = top_left_.i;
      ++index_.j;
    }
  }

  void AdvanceUntilTileIsRelevant() {
    const TilingData& data = *tiling_->tiling_data();
    gfx::Rect last_geometry_rect;
    Tile* next_tile = nullptr;
    while (IsValid()) {
      // Calculate the current geometry rect. As we reserved overlap between
      // tiles to accommodate bilinear filtering and rounding errors in
      // destination space, the geometry rect might overlap on the edges.
      //
      // We allow the tile to overreach by 1/1024 texels to avoid seams between
      // tiles. The constant 1/1024 is picked by the fact that with bilinear
      // filtering, the maximum error in color value introduced by clamping
      // error in both u/v axis can't exceed
      // 255 * (1 - (1 - 1/1024) * (1 - 1/1024)) ~= 0.498
      // i.e. The color value can never flip over a rounding threshold.
      gfx::RectF texel_extent = data.TexelExtent(index_.i, index_.j);
      constexpr float kEpsilon = 1. / 1024.f;
      texel_extent.Inset(-kEpsilon);

      // Convert texel_extent to coverage scale, which is what we have to report
      // geometry_rect in.
      //
      // We also adjust external edges to cover the whole recorded bounds in
      // dest space if any edge of the tiling rect touches the recorded edge.
      //
      // For external edges, extend the tile to scaled recorded bounds. This is
      // needed to fully cover the coverage space because the sample extent
      // doesn't cover the last 0.5 texel to the recorded edge, and also the
      // coverage space can be rounded up for up to 1 pixel. This overhang will
      // never be sampled as the AA fragment shader clamps sample coordinate and
      // antialiasing itself.
      gfx::Rect geometry_rect = gfx::ToEnclosedRect(
          coverage_to_content_.InverseMapRect(texel_extent));
      geometry_rect.SetByBounds(
          index_.i == 0 ? coverage_rect_max_bounds_.x() : geometry_rect.x(),
          index_.j == 0 ? coverage_rect_max_bounds_.y() : geometry_rect.y(),
          index_.i == data.num_tiles_x() - 1 ? coverage_rect_max_bounds_.right()
                                             : geometry_rect.right(),
          index_.j == data.num_tiles_y() - 1
              ? coverage_rect_max_bounds_.bottom()
              : geometry_rect.bottom());
      geometry_rect.Intersect(coverage_rect_);
      if (!geometry_rect.IsEmpty()) {
        next_tile = tiling_->TileAt(index_);
        last_geometry_rect = std::exchange(geometry_rect_, geometry_rect);
        break;
      }

      IncrementIndex();
    }

    current_tile_ = next_tile;
    if (last_geometry_rect.IsEmpty()) {
      // First tile or end of iteration. Nothing more to do in either case.
      return;
    }

    // Iteration happens left->right, top->bottom.  Running off the bottom-right
    // edge is handled by the intersection above.  Here we make sure that the
    // new current geometry rect doesn't overlap with the previous one.
    int min_left, min_top;
    const bool new_row = index_.i == top_left_.i;
    if (new_row) {
      min_left = coverage_rect_.x();
      min_top = last_geometry_rect.bottom();
    } else {
      min_left = last_geometry_rect.right();
      min_top = last_geometry_rect.y();
    }
    const int inset_left = std::max(0, min_left - geometry_rect_.x());
    const int inset_top = std::max(0, min_top - geometry_rect_.y());
    geometry_rect_.Inset(gfx::Insets::TLBR(inset_top, inset_left, 0, 0));

#if DCHECK_IS_ON()
    // Sometimes we run into an extreme case where we are at the edge of integer
    // precision. When doing so, rect calculations may end up changing values
    // unexpectedly. Unfortunately, there isn't much we can do at this point, so
    // we just do the correctness checks if both y and x offsets are
    // 'reasonable', meaning they are less than the specified value.
    static constexpr int kReasonableOffsetForDcheck = 100'000'000;
    if (!new_row && geometry_rect_.x() <= kReasonableOffsetForDcheck &&
        geometry_rect_.y() <= kReasonableOffsetForDcheck) {
      DCHECK_EQ(last_geometry_rect.right(), geometry_rect_.x());
      DCHECK_EQ(last_geometry_rect.bottom(), geometry_rect_.bottom());
      DCHECK_EQ(last_geometry_rect.y(), geometry_rect_.y());
    }
#endif
  }

  RAW_PTR_EXCLUSION const T* tiling_;
  gfx::Rect coverage_rect_max_bounds_;
  gfx::Rect coverage_rect_;
  gfx::AxisTransform2d coverage_to_content_;
  TileIndex top_left_;
  TileIndex bottom_right_;

  TileIndex index_;
  gfx::Rect geometry_rect_;
  RAW_PTR_EXCLUSION Tile* current_tile_ = nullptr;
};

}  // namespace cc

#endif  // CC_TILES_TILING_COVERAGE_ITERATOR_H_
