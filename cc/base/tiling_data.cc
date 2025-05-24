// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/tiling_data.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {

namespace {
// IndexRect which is at left top corner of the positive quadrant.
const IndexRect kNonPositiveQuadrantIndexRect(-1, -1, -1, -1);
}

static int ComputeNumTiles(int max_texture_size,
                           int total_size,
                           int border_texels) {
  if (max_texture_size - 2 * border_texels <= 0)
    return total_size > 0 && max_texture_size >= total_size ? 1 : 0;

  int num_tiles = std::max(1,
                           1 + (total_size - 1 - 2 * border_texels) /
                           (max_texture_size - 2 * border_texels));
  return total_size > 0 ? num_tiles : 0;
}

TilingData::TilingData() : border_texels_(0) {
  RecomputeNumTiles();
}

TilingData::TilingData(const gfx::Size& max_texture_size,
                       const gfx::Rect& tiling_rect,
                       int border_texels)
    : max_texture_size_(max_texture_size), border_texels_(border_texels) {
  SetTilingRect(tiling_rect);
}

void TilingData::SetTilingRect(const gfx::Rect& tiling_rect) {
  DCHECK_GE(tiling_rect.x(), 0);
  DCHECK_GE(tiling_rect.y(), 0);
  tiling_rect_ = tiling_rect;
  RecomputeNumTiles();
}

void TilingData::SetMaxTextureSize(const gfx::Size& max_texture_size) {
  max_texture_size_ = max_texture_size;
  RecomputeNumTiles();
}

int TilingData::TileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int x = (src_position - tiling_rect_.x() - border_texels_) /
          (max_texture_size_.width() - 2 * border_texels_);
  return std::clamp(x, 0, num_tiles_x_ - 1);
}

int TilingData::TileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int y = (src_position - tiling_rect_.y() - border_texels_) /
          (max_texture_size_.height() - 2 * border_texels_);
  return std::clamp(y, 0, num_tiles_y_ - 1);
}

int TilingData::FirstBorderTileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.width() - 2 * border_texels_;
  int x =
      (src_position - tiling_rect_.x() - 2 * border_texels_) / inner_tile_size;
  return std::clamp(x, 0, num_tiles_x_ - 1);
}

int TilingData::FirstBorderTileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.height() - 2 * border_texels_;
  int y =
      (src_position - tiling_rect_.y() - 2 * border_texels_) / inner_tile_size;
  return std::clamp(y, 0, num_tiles_y_ - 1);
}

int TilingData::LastBorderTileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.width() - 2 * border_texels_;
  int x = (src_position - tiling_rect_.x()) / inner_tile_size;
  return std::clamp(x, 0, num_tiles_x_ - 1);
}

int TilingData::LastBorderTileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.height() - 2 * border_texels_;
  int y = (src_position - tiling_rect_.y()) / inner_tile_size;
  return std::clamp(y, 0, num_tiles_y_ - 1);
}

IndexRect TilingData::TileAroundIndexRect(const gfx::Rect& center_rect) const {
  int around_left = 0;
  // Determine around left, such that it is between -1 and num_tiles_x.
  if (center_rect.x() < tiling_rect_.x() || center_rect.IsEmpty()) {
    around_left = -1;
  } else if (center_rect.x() >= tiling_rect_.right()) {
    around_left = num_tiles_x();
  } else {
    around_left = TileXIndexFromSrcCoord(center_rect.x());
  }

  // Determine around top, such that it is between -1 and num_tiles_y.
  int around_top = 0;
  if (center_rect.y() < tiling_rect_.y() || center_rect.IsEmpty()) {
    around_top = -1;
  } else if (center_rect.y() >= tiling_rect_.bottom()) {
    around_top = num_tiles_y();
  } else {
    around_top = TileYIndexFromSrcCoord(center_rect.y());
  }

  // Determine around right, such that it is between -1 and num_tiles_x.
  int around_right = 0;
  int right_src_coord = center_rect.right() - 1;
  if (right_src_coord < tiling_rect_.x() || center_rect.IsEmpty()) {
    around_right = -1;
  } else if (right_src_coord >= tiling_rect_.right()) {
    around_right = num_tiles_x();
  } else {
    around_right = TileXIndexFromSrcCoord(right_src_coord);
  }

  // Determine around bottom, such that it is between -1 and num_tiles_y.
  int around_bottom = 0;
  int bottom_src_coord = center_rect.bottom() - 1;
  if (bottom_src_coord < tiling_rect_.y() || center_rect.IsEmpty()) {
    around_bottom = -1;
  } else if (bottom_src_coord >= tiling_rect_.bottom()) {
    around_bottom = num_tiles_y();
  } else {
    around_bottom = TileYIndexFromSrcCoord(bottom_src_coord);
  }

  return IndexRect(around_left, around_right, around_top, around_bottom);
}

gfx::Rect TilingData::ExpandRectToTileBounds(const gfx::Rect& rect) const {
  if (rect.IsEmpty() || has_empty_bounds()) {
    return gfx::Rect();
  }
  if (rect.x() > tiling_rect_.right() || rect.y() > tiling_rect_.bottom()) {
    return gfx::Rect();
  }
  int index_x = FirstBorderTileXIndexFromSrcCoord(rect.x());
  int index_y = FirstBorderTileYIndexFromSrcCoord(rect.y());
  int index_right = LastBorderTileXIndexFromSrcCoord(rect.right() - 1);
  int index_bottom = LastBorderTileYIndexFromSrcCoord(rect.bottom() - 1);

  gfx::Rect rect_top_left(TileBounds(index_x, index_y));
  gfx::Rect rect_bottom_right(TileBounds(index_right, index_bottom));

  return gfx::UnionRects(rect_top_left, rect_bottom_right);
}

gfx::Rect TilingData::TileBounds(int i, int j) const {
  AssertTile(i, j);
  int max_texture_size_x = max_texture_size_.width() - 2 * border_texels_;
  int max_texture_size_y = max_texture_size_.height() - 2 * border_texels_;

  int lo_x = max_texture_size_x * i;
  if (i != 0)
    lo_x += border_texels_;

  int lo_y = max_texture_size_y * j;
  if (j != 0)
    lo_y += border_texels_;

  int hi_x = max_texture_size_x * (i + 1) + border_texels_;
  if (i + 1 == num_tiles_x_)
    hi_x += border_texels_;

  int hi_y = max_texture_size_y * (j + 1) + border_texels_;
  if (j + 1 == num_tiles_y_)
    hi_y += border_texels_;

  hi_x = std::min(hi_x, tiling_rect_.width());
  hi_y = std::min(hi_y, tiling_rect_.height());

  int x = lo_x;
  int y = lo_y;
  int width = hi_x - lo_x;
  int height = hi_y - lo_y;
  DCHECK_GE(x, 0);
  DCHECK_GE(y, 0);
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_LE(x, tiling_rect_.width());
  DCHECK_LE(y, tiling_rect_.height());
  return gfx::Rect(x + tiling_rect_.x(), y + tiling_rect_.y(), width, height);
}

gfx::Rect TilingData::TileBoundsWithBorder(int i, int j) const {
  AssertTile(i, j);
  int max_texture_size_x = max_texture_size_.width() - 2 * border_texels_;
  int max_texture_size_y = max_texture_size_.height() - 2 * border_texels_;

  int lo_x = max_texture_size_x * i;
  int lo_y = max_texture_size_y * j;

  int hi_x = lo_x + max_texture_size_x + 2 * border_texels_;
  int hi_y = lo_y + max_texture_size_y + 2 * border_texels_;

  hi_x = std::min(hi_x, tiling_rect_.width());
  hi_y = std::min(hi_y, tiling_rect_.height());

  int x = lo_x;
  int y = lo_y;
  int width = hi_x - lo_x;
  int height = hi_y - lo_y;
  DCHECK_GE(x, 0);
  DCHECK_GE(y, 0);
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_LE(x, tiling_rect_.width());
  DCHECK_LE(y, tiling_rect_.height());
  return gfx::Rect(x + tiling_rect_.x(), y + tiling_rect_.y(), width, height);
}

int TilingData::TilePositionX(int x_index) const {
  DCHECK_GE(x_index, 0);
  DCHECK_LT(x_index, num_tiles_x_);

  int pos = (max_texture_size_.width() - 2 * border_texels_) * x_index;
  if (x_index != 0)
    pos += border_texels_;

  return pos + tiling_rect_.x();
}

int TilingData::TilePositionY(int y_index) const {
  DCHECK_GE(y_index, 0);
  DCHECK_LT(y_index, num_tiles_y_);

  int pos = (max_texture_size_.height() - 2 * border_texels_) * y_index;
  if (y_index != 0)
    pos += border_texels_;

  return pos + tiling_rect_.y();
}

int TilingData::TileSizeX(int x_index) const {
  DCHECK_GE(x_index, 0);
  DCHECK_LT(x_index, num_tiles_x_);

  if (!x_index && num_tiles_x_ == 1) {
    return tiling_rect_.width();
  }
  if (!x_index && num_tiles_x_ > 1) {
    return max_texture_size_.width() - border_texels_;
  }
  if (x_index < num_tiles_x_ - 1) {
    return max_texture_size_.width() - 2 * border_texels_;
  }
  if (x_index == num_tiles_x_ - 1) {
    return tiling_rect_.right() - TilePositionX(x_index);
  }

  NOTREACHED();
}

int TilingData::TileSizeY(int y_index) const {
  DCHECK_GE(y_index, 0);
  DCHECK_LT(y_index, num_tiles_y_);

  if (!y_index && num_tiles_y_ == 1) {
    return tiling_rect_.height();
  }
  if (!y_index && num_tiles_y_ > 1) {
    return max_texture_size_.height() - border_texels_;
  }
  if (y_index < num_tiles_y_ - 1) {
    return max_texture_size_.height() - 2 * border_texels_;
  }
  if (y_index == num_tiles_y_ - 1) {
    return tiling_rect_.bottom() - TilePositionY(y_index);
  }

  NOTREACHED();
}

gfx::RectF TilingData::TexelExtent(int i, int j) const {
  gfx::RectF result(TileBoundsWithBorder(i, j));
  result.Inset(0.5f);
  return result;
}

gfx::Vector2d TilingData::TextureOffset(int x_index, int y_index) const {
  int left = (!x_index || num_tiles_x_ == 1) ? 0 : border_texels_;
  int top = (!y_index || num_tiles_y_ == 1) ? 0 : border_texels_;

  return gfx::Vector2d(left, top);
}

void TilingData::RecomputeNumTiles() {
  num_tiles_x_ = ComputeNumTiles(max_texture_size_.width(),
                                 tiling_rect_.width(), border_texels_);
  num_tiles_y_ = ComputeNumTiles(max_texture_size_.height(),
                                 tiling_rect_.height(), border_texels_);
}

TilingData::BaseIterator::BaseIterator() : index_x_(-1), index_y_(-1) {
}

TilingData::Iterator::Iterator() : index_rect_(kNonPositiveQuadrantIndexRect) {
  done();
}

TilingData::Iterator::Iterator(const TilingData* tiling_data,
                               const gfx::Rect& consider_rect,
                               bool include_borders)
    : index_rect_(kNonPositiveQuadrantIndexRect) {
  if (tiling_data->num_tiles_x() <= 0 || tiling_data->num_tiles_y() <= 0) {
    done();
    return;
  }

  gfx::Rect rect(consider_rect);
  rect.Intersect(tiling_data->tiling_rect());
  if (rect.IsEmpty()) {
    done();
    return;
  }

  gfx::Rect top_left_tile;
  if (include_borders) {
    index_x_ = tiling_data->FirstBorderTileXIndexFromSrcCoord(rect.x());
    index_y_ = tiling_data->FirstBorderTileYIndexFromSrcCoord(rect.y());
    index_rect_ = IndexRect(
        index_x_,
        tiling_data->LastBorderTileXIndexFromSrcCoord(rect.right() - 1),
        index_y_,
        tiling_data->LastBorderTileYIndexFromSrcCoord(rect.bottom() - 1));
    DCHECK(index_rect_.is_valid());
    top_left_tile = tiling_data->TileBoundsWithBorder(index_x_, index_y_);
  } else {
    index_x_ = tiling_data->TileXIndexFromSrcCoord(rect.x());
    index_y_ = tiling_data->TileYIndexFromSrcCoord(rect.y());
    index_rect_ = IndexRect(
        index_x_, tiling_data->TileXIndexFromSrcCoord(rect.right() - 1),
        index_y_, tiling_data->TileYIndexFromSrcCoord(rect.bottom() - 1));
    DCHECK(index_rect_.is_valid());
    top_left_tile = tiling_data->TileBounds(index_x_, index_y_);
  }

  // Index functions always return valid indices, so explicitly check
  // for non-intersecting rects.
  if (!top_left_tile.Intersects(rect))
    done();
}

TilingData::Iterator& TilingData::Iterator::operator++() {
  if (!*this)
    return *this;

  index_x_++;
  if (index_x_ > index_rect_.right()) {
    index_x_ = index_rect_.left();
    index_y_++;
    if (index_y_ > index_rect_.bottom())
      done();
  }

  return *this;
}

TilingData::BaseDifferenceIterator::BaseDifferenceIterator()
    : consider_index_rect_(kNonPositiveQuadrantIndexRect),
      ignore_index_rect_(kNonPositiveQuadrantIndexRect) {
  done();
}

TilingData::BaseDifferenceIterator::BaseDifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect)
    : consider_index_rect_(kNonPositiveQuadrantIndexRect),
      ignore_index_rect_(kNonPositiveQuadrantIndexRect) {
  if (tiling_data->num_tiles_x() <= 0 || tiling_data->num_tiles_y() <= 0) {
    done();
    return;
  }

  gfx::Rect consider(consider_rect);
  consider.Intersect(tiling_data->tiling_rect());

  if (consider.IsEmpty()) {
    done();
    return;
  }

  consider_index_rect_ =
      IndexRect(tiling_data->TileXIndexFromSrcCoord(consider.x()),
                tiling_data->TileXIndexFromSrcCoord(consider.right() - 1),
                tiling_data->TileYIndexFromSrcCoord(consider.y()),
                tiling_data->TileYIndexFromSrcCoord(consider.bottom() - 1));
  DCHECK(consider_index_rect_.is_valid());

  gfx::Rect ignore(ignore_rect);
  ignore.Intersect(tiling_data->tiling_rect());

  if (!ignore.IsEmpty()) {
    ignore_index_rect_ =
        IndexRect(tiling_data->TileXIndexFromSrcCoord(ignore.x()),
                  tiling_data->TileXIndexFromSrcCoord(ignore.right() - 1),
                  tiling_data->TileYIndexFromSrcCoord(ignore.y()),
                  tiling_data->TileYIndexFromSrcCoord(ignore.bottom() - 1));
    DCHECK(ignore_index_rect_.is_valid());

    // Clamp ignore indices to consider indices.
    ignore_index_rect_.ClampTo(consider_index_rect_);

    // If ignore rect is invalid, reset.
    if (!ignore_index_rect_.is_valid())
      ignore_index_rect_ = kNonPositiveQuadrantIndexRect;

    if (ignore_index_rect_ == consider_index_rect_) {
      consider_index_rect_ = kNonPositiveQuadrantIndexRect;
      done();
      return;
    }
  }
}

bool TilingData::BaseDifferenceIterator::HasConsiderRect() const {
  // Consider indices are either all valid or all equal to -1.
  DCHECK(consider_index_rect_.is_in_positive_quadrant() ||
         consider_index_rect_ == kNonPositiveQuadrantIndexRect);
  return consider_index_rect_.left() != -1;
}

TilingData::DifferenceIterator::DifferenceIterator() = default;

TilingData::DifferenceIterator::DifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect)
    : BaseDifferenceIterator(tiling_data, consider_rect, ignore_rect) {
  if (!HasConsiderRect()) {
    done();
    return;
  }

  index_x_ = consider_index_rect_.left();
  index_y_ = consider_index_rect_.top();

  if (ignore_index_rect_.Contains(index_x_, index_y_))
    ++(*this);
}

TilingData::DifferenceIterator& TilingData::DifferenceIterator::operator++() {
  if (!*this)
    return *this;

  index_x_++;
  if (ignore_index_rect_.Contains(index_x_, index_y_))
    index_x_ = ignore_index_rect_.right() + 1;

  if (index_x_ > consider_index_rect_.right()) {
    index_x_ = consider_index_rect_.left();
    index_y_++;

    if (ignore_index_rect_.Contains(index_x_, index_y_)) {
      index_x_ = ignore_index_rect_.right() + 1;
      // If the ignore rect spans the whole consider rect horizontally, then
      // ignore_right + 1 will be out of bounds.
      if (ignore_index_rect_.Contains(index_x_, index_y_) ||
          index_x_ > consider_index_rect_.right()) {
        index_y_ = ignore_index_rect_.bottom() + 1;
        index_x_ = consider_index_rect_.left();
      }
    }

    if (index_y_ > consider_index_rect_.bottom())
      done();
  }

  return *this;
}

TilingData::SpiralDifferenceIterator::SpiralDifferenceIterator() {
  done();
}

TilingData::SpiralDifferenceIterator::SpiralDifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect,
    const gfx::Rect& center_rect)
    : BaseDifferenceIterator(tiling_data, consider_rect, ignore_rect) {
  if (!HasConsiderRect()) {
    done();
    return;
  }

  IndexRect around_index_rect = tiling_data->TileAroundIndexRect(center_rect);
  DCHECK(around_index_rect.is_valid());

  spiral_iterator_ = SpiralIterator(around_index_rect, consider_index_rect_,
                                    ignore_index_rect_);

  if (!spiral_iterator_) {
    done();
    return;
  }

  index_x_ = spiral_iterator_.index_x();
  index_y_ = spiral_iterator_.index_y();
}

TilingData::SpiralDifferenceIterator& TilingData::SpiralDifferenceIterator::
operator++() {
  ++spiral_iterator_;

  if (!spiral_iterator_) {
    done();
    return *this;
  }

  index_x_ = spiral_iterator_.index_x();
  index_y_ = spiral_iterator_.index_y();

  return *this;
}

TilingData::ReverseSpiralDifferenceIterator::ReverseSpiralDifferenceIterator() {
  done();
}

TilingData::ReverseSpiralDifferenceIterator::ReverseSpiralDifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect,
    const gfx::Rect& center_rect)
    : BaseDifferenceIterator(tiling_data, consider_rect, ignore_rect) {
  if (!HasConsiderRect()) {
    done();
    return;
  }

  IndexRect around_index_rect = tiling_data->TileAroundIndexRect(center_rect);
  DCHECK(around_index_rect.is_valid());

  reverse_spiral_iterator_ = ReverseSpiralIterator(
      around_index_rect, consider_index_rect_, ignore_index_rect_);

  if (!reverse_spiral_iterator_) {
    done();
    return;
  }

  index_x_ = reverse_spiral_iterator_.index_x();
  index_y_ = reverse_spiral_iterator_.index_y();
}

TilingData::ReverseSpiralDifferenceIterator&
    TilingData::ReverseSpiralDifferenceIterator::
    operator++() {
  ++reverse_spiral_iterator_;

  if (!reverse_spiral_iterator_) {
    done();
    return *this;
  }

  index_x_ = reverse_spiral_iterator_.index_x();
  index_y_ = reverse_spiral_iterator_.index_y();

  return *this;
}

}  // namespace cc
