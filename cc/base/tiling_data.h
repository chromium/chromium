// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_TILING_DATA_H_
#define CC_BASE_TILING_DATA_H_

#include <utility>

#include "base/check_op.h"
#include "cc/base/base_export.h"
#include "cc/base/index_rect.h"
#include "cc/base/reverse_spiral_iterator.h"
#include "cc/base/spiral_iterator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class RectF;
class Vector2d;
}

namespace cc {

class CC_BASE_EXPORT TilingData {
 public:
  TilingData();
  TilingData(const gfx::Size& max_texture_size,
             const gfx::Size& tiling_size,
             bool has_border_texels);
  TilingData(const gfx::Size& max_texture_size,
             const gfx::Size& tiling_size,
             int border_texels);

  gfx::Size tiling_size() const { return tiling_size_; }
  void SetTilingSize(const gfx::Size& tiling_size);

  gfx::Size max_texture_size() const { return max_texture_size_; }
  void SetMaxTextureSize(const gfx::Size& max_texture_size);

  int border_texels() const { return border_texels_; }
  void SetHasBorderTexels(bool has_border_texels);
  void SetBorderTexels(int border_texels);

  bool has_empty_bounds() const { return !num_tiles_x_ || !num_tiles_y_; }
  int num_tiles_x() const { return num_tiles_x_; }
  int num_tiles_y() const { return num_tiles_y_; }
  // Return the tile index whose non-border texels include src_position.
  int TileXIndexFromSrcCoord(int src_position) const;
  int TileYIndexFromSrcCoord(int src_position) const;
  // Return the lowest tile index whose border texels include src_position.
  int FirstBorderTileXIndexFromSrcCoord(int src_position) const;
  int FirstBorderTileYIndexFromSrcCoord(int src_position) const;
  // Return the highest tile index whose border texels include src_position.
  int LastBorderTileXIndexFromSrcCoord(int src_position) const;
  int LastBorderTileYIndexFromSrcCoord(int src_position) const;
  // Return the tile indices around the given rect.
  IndexRect TileAroundIndexRect(const gfx::Rect& center_rect) const;

  gfx::Rect ExpandRectIgnoringBordersToTileBounds(const gfx::Rect& rect) const;
  gfx::Rect ExpandRectToTileBounds(const gfx::Rect& rect) const;

  gfx::Rect TileBounds(int i, int j) const;
  gfx::Rect TileBoundsWithBorder(int i, int j) const;
  int TilePositionX(int x_index) const;
  int TilePositionY(int y_index) const;
  int TileSizeX(int x_index) const;
  int TileSizeY(int y_index) const;

  gfx::RectF TexelExtent(int i, int j) const;

  // Difference between TileBound's and TileBoundWithBorder's origin().
  gfx::Vector2d TextureOffset(int x_index, int y_index) const;

  class CC_BASE_EXPORT BaseIterator {
   public:
    operator bool() const { return index_x_ != -1 && index_y_ != -1; }

    int index_x() const { return index_x_; }
    int index_y() const { return index_y_; }
    std::pair<int, int> index() const {
     return std::make_pair(index_x_, index_y_);
    }

   protected:
    BaseIterator();
    void done() {
      index_x_ = -1;
      index_y_ = -1;
    }

    int index_x_;
    int index_y_;
  };

  // Iterate through tiles whose bounds + optional border intersect with |rect|.
  class CC_BASE_EXPORT Iterator : public BaseIterator {
   public:
    Iterator();
    Iterator(const TilingData* tiling_data,
             const gfx::Rect& consider_rect,
             bool include_borders);
    Iterator& operator++();

   private:
    IndexRect index_rect_;
  };

  class CC_BASE_EXPORT BaseDifferenceIterator : public BaseIterator {
   protected:
    BaseDifferenceIterator();
    BaseDifferenceIterator(const TilingData* tiling_data,
                           const gfx::Rect& consider_rect,
                           const gfx::Rect& ignore_rect);

    bool HasConsiderRect() const;

    IndexRect consider_index_rect_;
    IndexRect ignore_index_rect_;
  };

  // Iterate through all indices whose bounds (not including borders) intersect
  // with |consider| but which also do not intersect with |ignore|.
  class CC_BASE_EXPORT DifferenceIterator : public BaseDifferenceIterator {
   public:
    DifferenceIterator();
    DifferenceIterator(const TilingData* tiling_data,
                       const gfx::Rect& consider_rect,
                       const gfx::Rect& ignore_rect);
    DifferenceIterator& operator++();
  };

  // Iterate through all indices whose bounds + border intersect with
  // |consider| but which also do not intersect with |ignore|. The iterator
  // order is a counterclockwise spiral around the given center.
  class CC_BASE_EXPORT SpiralDifferenceIterator
      : public BaseDifferenceIterator {
   public:
    SpiralDifferenceIterator();
    SpiralDifferenceIterator(const TilingData* tiling_data,
                             const gfx::Rect& consider_rect,
                             const gfx::Rect& ignore_rect,
                             const gfx::Rect& center_rect);
    SpiralDifferenceIterator& operator++();

   private:
    SpiralIterator spiral_iterator_;
  };

  class CC_BASE_EXPORT ReverseSpiralDifferenceIterator
      : public BaseDifferenceIterator {
   public:
    ReverseSpiralDifferenceIterator();
    ReverseSpiralDifferenceIterator(const TilingData* tiling_data,
                                    const gfx::Rect& consider_rect,
                                    const gfx::Rect& ignore_rect,
                                    const gfx::Rect& center_rect);
    ReverseSpiralDifferenceIterator& operator++();

   private:
    ReverseSpiralIterator reverse_spiral_iterator_;
  };

 private:
  void AssertTile(int i, int j) const {
    DCHECK_GE(i,  0);
    DCHECK_LT(i, num_tiles_x_);
    DCHECK_GE(j, 0);
    DCHECK_LT(j, num_tiles_y_);
  }

  void RecomputeNumTiles();

  gfx::Size max_texture_size_;
  gfx::Size tiling_size_;
  int border_texels_;

  // These are computed values.
  int num_tiles_x_;
  int num_tiles_y_;
};

}  // namespace cc

#endif  // CC_BASE_TILING_DATA_H_
