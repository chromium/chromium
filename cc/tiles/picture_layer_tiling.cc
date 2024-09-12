// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/tiles/picture_layer_tiling.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/features.h"
#include "cc/base/math_util.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/raster/raster_source.h"
#include "cc/tiles/prioritized_tile.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

PictureLayerTiling::PictureLayerTiling(
    WhichTree tree,
    const gfx::AxisTransform2d& raster_transform,
    scoped_refptr<RasterSource> raster_source,
    PictureLayerTilingClient* client,
    float min_preraster_distance,
    float max_preraster_distance,
    bool can_use_lcd_text)
    : raster_transform_(raster_transform),
      client_(client),
      tree_(tree),
      raster_source_(raster_source),
      min_preraster_distance_(min_preraster_distance),
      max_preraster_distance_(max_preraster_distance),
      can_use_lcd_text_(can_use_lcd_text) {
  DCHECK(!raster_source->IsSolidColor());
  DCHECK_GE(raster_transform.translation().x(), 0.f);
  DCHECK_LT(raster_transform.translation().x(), 1.f);
  DCHECK_GE(raster_transform.translation().y(), 0.f);
  DCHECK_LT(raster_transform.translation().y(), 1.f);

#if DCHECK_IS_ON()
  gfx::SizeF scaled_source_size(gfx::ScaleSize(
      gfx::SizeF(raster_source_->recorded_bounds().size()),
      raster_transform.scale().x(), raster_transform.scale().y()));
  gfx::Size floored_size = gfx::ToFlooredSize(scaled_source_size);
  bool is_width_empty =
      !floored_size.width() &&
      !MathUtil::IsWithinEpsilon(scaled_source_size.width(), 1.f);
  bool is_height_empty =
      !floored_size.height() &&
      !MathUtil::IsWithinEpsilon(scaled_source_size.height(), 1.f);
  DCHECK(!is_width_empty && !is_height_empty)
      << "Tiling created with scale too small as contents become empty."
      << " recorded bounds: " << raster_source_->recorded_bounds().ToString()
      << " Raster transform: " << raster_transform_.ToString();
#endif

  gfx::Rect tiling_rect = ComputeTilingRect();
  SetTilingRect(tiling_rect);
  tiling_data_.SetMaxTextureSize(
      client_->CalculateTileSize(tiling_rect.size()));
}

PictureLayerTiling::~PictureLayerTiling() = default;

Tile* PictureLayerTiling::CreateTile(const Tile::CreateInfo& info) {
  const int i = info.tiling_i_index;
  const int j = info.tiling_j_index;
  TileIndex index(i, j);
  DCHECK(!base::Contains(tiles_, index));

  if (!raster_source_->IntersectsRect(info.enclosing_layer_rect)) {
    return nullptr;
  }

  all_tiles_done_ = false;

  std::unique_ptr<Tile> tile = client_->CreateTile(info);
  Tile* tile_ptr = tile.get();
  tiles_[index] = std::move(tile);
  return tile_ptr;
}

void PictureLayerTiling::CreateMissingTilesInLiveTilesRect() {
  const PictureLayerTiling* active_twin =
      tree_ == PENDING_TREE ? client_->GetPendingOrActiveTwinTiling(this)
                            : nullptr;
  const Region* invalidation =
      active_twin ? client_->GetPendingInvalidation() : nullptr;

  bool include_borders = false;
  for (TilingData::Iterator iter(&tiling_data_, live_tiles_rect_,
                                 include_borders);
       iter; ++iter) {
    TileIndex index(iter.index());
    auto find = tiles_.find(index);
    if (find != tiles_.end())
      continue;

    Tile::CreateInfo info = CreateInfoForTile(index.i, index.j);
    if (ShouldCreateTileAt(info)) {
      Tile* tile = CreateTile(info);

      // If this is the pending tree, then the active twin tiling may contain
      // the previous content ID of these tiles. In that case, we need only
      // partially raster the tile content.
      if (tile && invalidation && TilingMatchesTileIndices(active_twin)) {
        if (const Tile* old_tile = active_twin->TileAt(index)) {
          gfx::Rect tile_rect = tile->content_rect();
          gfx::Rect invalidated;
          for (gfx::Rect rect : *invalidation) {
            gfx::Rect invalid_content_rect =
                EnclosingContentsRectFromLayerRect(rect);
            invalid_content_rect.Intersect(tile_rect);
            invalidated.Union(invalid_content_rect);
          }
          tile->SetInvalidated(invalidated, old_tile->id());
        }
      }
    }
  }
  VerifyTiles();
}

void PictureLayerTiling::TakeTilesAndPropertiesFrom(
    PictureLayerTiling* pending_twin,
    const Region& layer_invalidation) {
  SetRasterSourceAndResize(pending_twin->raster_source_);

  RemoveTilesInRegion(layer_invalidation, false /* recreate tiles */);

  resolution_ = pending_twin->resolution_;

  while (!pending_twin->tiles_.empty()) {
    auto pending_iter = pending_twin->tiles_.begin();
    pending_iter->second->set_tiling(this);
    tiles_[pending_iter->first] = std::move(pending_iter->second);
    pending_twin->tiles_.erase(pending_iter);
  }

  if (all_tiles_done_ && !pending_twin->all_tiles_done_) {
    all_tiles_done_ = false;
  }
  ComputeTilePriorityRects(
      pending_twin->current_visible_rect_in_layer_space_,
      pending_twin->current_skewport_rect_in_layer_space_,
      pending_twin->current_soon_border_rect_in_layer_space_,
      pending_twin->current_eventually_rect_in_layer_space_,
      pending_twin->current_content_to_screen_scale_ * contents_scale_key(),
      pending_twin->current_occlusion_in_layer_space_);

  DCHECK(pending_twin->tiles_.empty());
  pending_twin->all_tiles_done_ = true;

  VerifyTiles();
}

bool PictureLayerTiling::SetRasterSourceAndResize(
    scoped_refptr<RasterSource> raster_source) {
  DCHECK(!raster_source->IsSolidColor());
  raster_source_ = std::move(raster_source);
  gfx::Rect tiling_rect = ComputeTilingRect();
  gfx::Size tile_size = client_->CalculateTileSize(tiling_rect.size());

  if (tile_size != tiling_data_.max_texture_size() ||
      tiling_rect.origin() != tiling_data_.tiling_rect().origin()) {
    SetTilingRect(tiling_rect);
    tiling_data_.SetMaxTextureSize(tile_size);
    // When the origin of recorded bounds or tile size changes, the TilingData
    // positions no longer work as valid indices to the TileMap, so just drop
    // all tiles and clear the live tiles rect.
    Reset();
    // When the tile size changes, all tiles and all tile priority rects
    // including the live tiles rect should be updated, therefore return true to
    // notify the caller to call |ComputeTilePriorityRects| to do this.
    return true;
  }

  // When the tiling rect is the same, we need not notify the caller as it
  // will update tiling as needed, so return false.
  if (tiling_rect == tiling_data_.tiling_rect()) {
    return false;
  }

  // We can't use SetPriorityRect(EVENTUALLY_RECT) and SetLiveTilesRect()
  // to drop and create tiles according to the new bounds. This is because
  // resizing the tiling causes the number of tiles in the tiling_data_ to
  // change.
  int before_eventually_left =
      tiling_data_.TileXIndexFromSrcCoord(current_eventually_rect_.x());
  int before_eventually_top =
      tiling_data_.TileYIndexFromSrcCoord(current_eventually_rect_.y());
  int before_eventually_right =
      tiling_data_.TileXIndexFromSrcCoord(current_eventually_rect_.right() - 1);
  int before_eventually_bottom = tiling_data_.TileYIndexFromSrcCoord(
      current_eventually_rect_.bottom() - 1);

  int before_live_left =
      tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.x());
  int before_live_top =
      tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.y());
  int before_live_right =
      tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.right() - 1);
  int before_live_bottom =
      tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.bottom() - 1);

  // The live_tiles_rect_ is clamped to stay within the tiling rect as we
  // change it.
  live_tiles_rect_.Intersect(tiling_rect);
  SetTilingRect(tiling_rect);

  // Evict tiles outside the new tiling rect.
  int after_eventually_right = tiling_data_.num_tiles_x() - 1;
  int after_eventually_bottom = tiling_data_.num_tiles_y() - 1;

  int after_live_right = -1;
  int after_live_bottom = -1;
  if (!live_tiles_rect_.IsEmpty()) {
    after_live_right =
        tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.right() - 1);
    after_live_bottom =
        tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.bottom() - 1);
  }

  // There is no recycled twin since this is run on the pending tiling
  // during commit, and on the active tree during activate.
  // Drop tiles outside the new recorded bounds if the they shrank.
  for (int i = after_eventually_right + 1; i <= before_eventually_right; ++i) {
    for (int j = before_eventually_top; j <= before_eventually_bottom; ++j) {
      TakeTileAt(i, j);
    }
  }
  for (int i = before_eventually_left; i <= after_eventually_right; ++i) {
    for (int j = after_eventually_bottom + 1; j <= before_eventually_bottom;
         ++j) {
      TakeTileAt(i, j);
    }
  }

  if (after_live_right > before_live_right) {
    DCHECK_EQ(after_live_right, before_live_right + 1);
    for (int j = before_live_top; j <= after_live_bottom; ++j) {
      Tile::CreateInfo info = CreateInfoForTile(after_live_right, j);
      if (ShouldCreateTileAt(info))
        CreateTile(info);
    }
  }
  if (after_live_bottom > before_live_bottom) {
    // Using the smallest horizontal bound here makes sure we don't
    // create tiles twice and don't iterate into deleted tiles.
    int boundary_right = std::min(after_live_right, before_live_right);
    DCHECK_EQ(after_live_bottom, before_live_bottom + 1);
    for (int i = before_live_left; i <= boundary_right; ++i) {
      Tile::CreateInfo info = CreateInfoForTile(i, after_live_bottom);
      if (ShouldCreateTileAt(info))
        CreateTile(info);
    }
  }
  VerifyTiles();
  // We need not notify the caller as it will update tiling as needed, so return
  // false to ensure the existing logic remains unchanged.
  return false;
}

void PictureLayerTiling::Invalidate(const Region& layer_invalidation) {
  DCHECK(tree_ != ACTIVE_TREE || !client_->GetPendingOrActiveTwinTiling(this));
  RemoveTilesInRegion(layer_invalidation, true /* recreate tiles */);
}

void PictureLayerTiling::RemoveTilesInRegion(const Region& layer_invalidation,
                                             bool recreate_tiles) {
  // We only invalidate the active tiling when it's orphaned: it has no pending
  // twin, so it's slated for removal in the future.
  if (live_tiles_rect_.IsEmpty())
    return;

  base::flat_map<TileIndex, gfx::Rect> remove_tiles;
  gfx::Rect expanded_eventually_rect =
      tiling_data_.ExpandRectToTileBounds(current_eventually_rect_);
  for (gfx::Rect layer_rect : layer_invalidation) {
    // The pixels which are invalid in content space.
    gfx::Rect invalid_content_rect =
        EnclosingContentsRectFromLayerRect(layer_rect);
    gfx::Rect coverage_content_rect = invalid_content_rect;
    // Avoid needless work by not bothering to invalidate where there aren't
    // tiles.
    coverage_content_rect.Intersect(expanded_eventually_rect);
    if (coverage_content_rect.IsEmpty())
      continue;
    // Since the content_rect needs to invalidate things that only touch a
    // border of a tile, we need to include the borders while iterating.
    bool include_borders = true;
    for (TilingData::Iterator iter(&tiling_data_, coverage_content_rect,
                                   include_borders);
         iter; ++iter) {
      // This also adds the TileIndex to the map.
      remove_tiles[TileIndex(iter.index())].Union(invalid_content_rect);
    }
  }

  for (const auto& pair : remove_tiles) {
    const TileIndex& index = pair.first;
    const gfx::Rect& invalid_content_rect = pair.second;
    // TODO(danakj): This old_tile will not exist if we are committing to a
    // pending tree since there is no tile there to remove, which prevents
    // tiles from knowing the invalidation rect and content id. crbug.com/490847
    std::unique_ptr<Tile> old_tile = TakeTileAt(index.i, index.j);
    if (recreate_tiles && old_tile) {
      Tile::CreateInfo info = CreateInfoForTile(index.i, index.j);
      if (Tile* tile = CreateTile(info))
        tile->SetInvalidated(invalid_content_rect, old_tile->id());
    }
  }
}

Tile::CreateInfo PictureLayerTiling::CreateInfoForTile(int i, int j) const {
  gfx::Rect tile_rect = tiling_data_.TileBoundsWithBorder(i, j);
  tile_rect.set_size(tiling_data_.max_texture_size());
  gfx::Rect enclosing_layer_rect =
      EnclosingLayerRectFromContentsRect(tile_rect);
  return Tile::CreateInfo{this,
                          i,
                          j,
                          enclosing_layer_rect,
                          tile_rect,
                          raster_transform_,
                          can_use_lcd_text_};
}

bool PictureLayerTiling::ShouldCreateTileAt(
    const Tile::CreateInfo& info) const {
  const int i = info.tiling_i_index;
  const int j = info.tiling_j_index;
  // Active tree should always create a tile. The reason for this is that active
  // tree represents content that we draw on screen, which means that whenever
  // we check whether a tile should exist somewhere, the answer is yes. This
  // doesn't mean it will actually be created (if raster source doesn't cover
  // the tile for instance). Pending tree, on the other hand, should only be
  // creating tiles that are different from the current active tree, which is
  // represented by the logic in the rest of the function.
  if (tree_ == ACTIVE_TREE)
    return true;

  // If the pending tree has no active twin, then it needs to create all tiles.
  const PictureLayerTiling* active_twin =
      client_->GetPendingOrActiveTwinTiling(this);
  if (!active_twin)
    return true;

  // Pending tree will override the entire active tree if indices don't match.
  if (!TilingMatchesTileIndices(active_twin))
    return true;

  // If our settings don't match the active twin, it means that the active
  // tiles will all be removed when we activate. So we need all the tiles on the
  // pending tree to be created. See
  // PictureLayerTilingSet::CopyTilingsAndPropertiesFromPendingTwin.
  if (can_use_lcd_text() != active_twin->can_use_lcd_text() ||
      raster_transform() != active_twin->raster_transform())
    return true;

  // If the active tree can't create a tile, because of its raster source, then
  // the pending tree should create one.
  if (!active_twin->raster_source()->IntersectsRect(
          info.enclosing_layer_rect)) {
    return true;
  }

  const Region* layer_invalidation = client_->GetPendingInvalidation();

  // If this tile is invalidated, then the pending tree should create one.
  // Do the intersection test in content space to match the corresponding check
  // on the active tree and avoid floating point inconsistencies.
  for (gfx::Rect layer_rect : *layer_invalidation) {
    gfx::Rect invalid_content_rect =
        EnclosingContentsRectFromLayerRect(layer_rect);
    if (invalid_content_rect.Intersects(info.content_rect))
      return true;
  }
  // If the active tree doesn't have a tile here, but it's in the pending tree's
  // visible rect, then the pending tree should create a tile. This can happen
  // if the pending visible rect is outside of the active tree's live tiles
  // rect. In those situations, we need to block activation until we're ready to
  // display content, which will have to come from the pending tree.
  if (!active_twin->TileAt(i, j) &&
      current_visible_rect_.Intersects(info.content_rect))
    return true;

  // In all other cases, the pending tree doesn't need to create a tile.
  return false;
}

bool PictureLayerTiling::TilingMatchesTileIndices(
    const PictureLayerTiling* twin) const {
  return tiling_data_.max_texture_size() ==
             twin->tiling_data_.max_texture_size() &&
         tiling_rect().origin() == twin->tiling_rect().origin();
}

std::unique_ptr<Tile> PictureLayerTiling::TakeTileAt(int i, int j) {
  auto found = tiles_.find(TileIndex(i, j));
  if (found == tiles_.end())
    return nullptr;
  std::unique_ptr<Tile> result = std::move(found->second);
  tiles_.erase(found);
  return result;
}

void PictureLayerTiling::SetTilePriorityRectsForTesting(
    const gfx::Rect& visible_rect,
    const gfx::Rect& skewport_rect,
    const gfx::Rect& soon_border_rect,
    const gfx::Rect& eventually_rect,
    bool evicts_tiles) {
  current_occlusion_in_layer_space_ = Occlusion();
  current_content_to_screen_scale_ = 1.0;

  SetPriorityRect(EnclosingLayerRectFromContentsRect(visible_rect),
                  VISIBLE_RECT);
  SetPriorityRect(EnclosingLayerRectFromContentsRect(skewport_rect),
                  SKEWPORT_RECT);
  SetPriorityRect(EnclosingLayerRectFromContentsRect(soon_border_rect),
                  SOON_BORDER_RECT);
  SetPriorityRect(EnclosingLayerRectFromContentsRect(eventually_rect),
                  EVENTUALLY_RECT, evicts_tiles);

  // Note that we use the largest skewport extent from the viewport as the
  // "skewport extent". Also note that this math can't produce negative numbers,
  // since skewport.Contains(visible_rect) is always true.
  max_skewport_extent_in_screen_space_ = std::max(
      {current_visible_rect_.x() - current_skewport_rect_.x(),
       current_skewport_rect_.right() - current_visible_rect_.right(),
       current_visible_rect_.y() - current_skewport_rect_.y(),
       current_skewport_rect_.bottom() - current_visible_rect_.bottom()});
}

void PictureLayerTiling::Reset() {
  live_tiles_rect_ = gfx::Rect();
  tiles_.clear();
  all_tiles_done_ = true;
}

void PictureLayerTiling::ComputeTilePriorityRects(
    const gfx::Rect& visible_rect_in_layer_space,
    const gfx::Rect& skewport_rect_in_layer_space,
    const gfx::Rect& soon_border_rect_in_layer_space,
    const gfx::Rect& eventually_rect_in_layer_space,
    float ideal_contents_scale,
    const Occlusion& occlusion_in_layer_space) {
  // If we have, or had occlusions, mark the tiles as 'not done' to ensure that
  // we reiterate the tiles for rasterization.
  if (occlusion_in_layer_space.HasOcclusion() ||
      current_occlusion_in_layer_space_.HasOcclusion()) {
    all_tiles_done_ = false;
  }

  TileMemoryLimitPolicy memory_limit_policy =
      client_->global_tile_state().memory_limit_policy;

  current_occlusion_in_layer_space_ = occlusion_in_layer_space;
  current_content_to_screen_scale_ =
      ideal_contents_scale / contents_scale_key();

  SetPriorityRect(visible_rect_in_layer_space, VISIBLE_RECT);
  SetPriorityRect(skewport_rect_in_layer_space, SKEWPORT_RECT);
  SetPriorityRect(soon_border_rect_in_layer_space, SOON_BORDER_RECT);
  SetPriorityRect(eventually_rect_in_layer_space, EVENTUALLY_RECT,
                  /*evicts_tiles=*/true);

  // Note that we use the largest skewport extent from the viewport as the
  // "skewport extent". Also note that this math can't produce negative numbers,
  // since skewport.Contains(visible_rect) is always true.
  max_skewport_extent_in_screen_space_ =
      current_content_to_screen_scale_ *
      std::max(
          {current_visible_rect_.x() - current_skewport_rect_.x(),
           current_skewport_rect_.right() - current_visible_rect_.right(),
           current_visible_rect_.y() - current_skewport_rect_.y(),
           current_skewport_rect_.bottom() - current_visible_rect_.bottom()});

  gfx::Rect live_tiles_rect;
  if (features::IsCCSlimmingEnabled()) {
    live_tiles_rect = current_visible_rect_;
    bool draws_tiles = has_visible_rect_tiles_;

    if (memory_limit_policy >= TileMemoryLimitPolicy::ALLOW_PREPAINT_ONLY) {
      draws_tiles |= has_skewport_rect_tiles_;
      if (has_skewport_rect_tiles_) {
        live_tiles_rect.Union(current_skewport_rect_);
      }

      draws_tiles |= has_soon_border_rect_tiles_;
      if (has_soon_border_rect_tiles_) {
        live_tiles_rect.Union(current_soon_border_rect_);
      }
    }

    if (memory_limit_policy >= TileMemoryLimitPolicy::ALLOW_ANYTHING) {
      draws_tiles |= has_eventually_rect_tiles_;
      if (has_eventually_rect_tiles_) {
        live_tiles_rect.Union(current_eventually_rect_);
      }
    }
    if (!draws_tiles) {
      all_tiles_done_ = true;
    }
  } else {
    live_tiles_rect = current_eventually_rect_;
  }
  live_tiles_rect.Intersect(tiling_rect());
  SetLiveTilesRect(live_tiles_rect);
}

void PictureLayerTiling::SetPriorityRect(const gfx::Rect& rect_in_layer_space,
                                         PriorityRectType rect_type,
                                         bool evicts_tiles) {
  DCHECK(!evicts_tiles || rect_type == EVENTUALLY_RECT);
  switch (rect_type) {
    case VISIBLE_RECT:
      if (current_visible_rect_in_layer_space_ != rect_in_layer_space) {
        current_visible_rect_in_layer_space_ = rect_in_layer_space;
        current_visible_rect_ =
            EnclosingContentsRectFromLayerRect(rect_in_layer_space);
        has_visible_rect_tiles_ =
            tiling_rect().Intersects(current_visible_rect_);
      }
      break;
    case SKEWPORT_RECT:
      if (current_skewport_rect_in_layer_space_ != rect_in_layer_space) {
        current_skewport_rect_in_layer_space_ = rect_in_layer_space;
        current_skewport_rect_ =
            EnclosingContentsRectFromLayerRect(rect_in_layer_space);
        has_skewport_rect_tiles_ =
            tiling_rect().Intersects(current_skewport_rect_);
      }
      break;
    case SOON_BORDER_RECT:
      if (current_soon_border_rect_in_layer_space_ != rect_in_layer_space) {
        current_soon_border_rect_in_layer_space_ = rect_in_layer_space;
        current_soon_border_rect_ =
            EnclosingContentsRectFromLayerRect(rect_in_layer_space);
        has_soon_border_rect_tiles_ =
            tiling_rect().Intersects(current_soon_border_rect_);
      }
      break;
    case EVENTUALLY_RECT:
      if (current_eventually_rect_in_layer_space_ != rect_in_layer_space) {
        current_eventually_rect_in_layer_space_ = rect_in_layer_space;
        gfx::Rect rect =
            EnclosingContentsRectFromLayerRect(rect_in_layer_space);
        if (evicts_tiles) {
          // Iterate to delete all tiles outside of our new live_tiles rect.
          for (TilingData::DifferenceIterator iter(
                   &tiling_data_, current_eventually_rect_, rect);
               iter; ++iter) {
            TakeTileAt(iter.index_x(), iter.index_y());
          }
        }
        current_eventually_rect_ = rect;
        has_eventually_rect_tiles_ =
            tiling_rect().Intersects(current_eventually_rect_);
      }
      break;
    default:
      NOTREACHED();
  }
}

void PictureLayerTiling::SetLiveTilesRect(
    const gfx::Rect& new_live_tiles_rect) {
  DCHECK(new_live_tiles_rect.IsEmpty() ||
         tiling_rect().Contains(new_live_tiles_rect))
      << "tiling_rect: " << tiling_rect().ToString()
      << " new_live_tiles_rect: " << new_live_tiles_rect.ToString();
  if (live_tiles_rect_ == new_live_tiles_rect)
    return;

  // We don't rasterize non ideal resolution tiles, so there is no need to
  // create any new tiles.
  if (resolution_ == NON_IDEAL_RESOLUTION) {
    live_tiles_rect_.Intersect(new_live_tiles_rect);
    VerifyTiles();
    return;
  }

  // Iterate to allocate new tiles for all regions with newly exposed area.
  for (TilingData::DifferenceIterator iter(&tiling_data_, new_live_tiles_rect,
                                           live_tiles_rect_);
       iter; ++iter) {
    Tile::CreateInfo info = CreateInfoForTile(iter.index_x(), iter.index_y());
    if (ShouldCreateTileAt(info)) {
      Tile* tile = TileAt(iter.index_x(), iter.index_y());
      if (tile) {
        if (!tile->IsReadyToDraw()) {
          all_tiles_done_ = false;
        }
      } else {
        CreateTile(info);
      }
    }
  }
  live_tiles_rect_ = new_live_tiles_rect;
  if (tiles_.size() == 0) {
    all_tiles_done_ = true;
  }
  VerifyTiles();
}

void PictureLayerTiling::VerifyTiles() const {
#if DCHECK_IS_ON()
  for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
    DCHECK(it->second);
    TileIndex index = it->first;
    DCHECK(index.i < tiling_data_.num_tiles_x())
        << this << " " << index.i << "," << index.j << " num_tiles_x "
        << tiling_data_.num_tiles_x() << " eventually_rect "
        << current_eventually_rect_.ToString();
    DCHECK(index.j < tiling_data_.num_tiles_y())
        << this << " " << index.i << "," << index.j << " num_tiles_y "
        << tiling_data_.num_tiles_y() << " eventually_rect "
        << current_eventually_rect_.ToString();
    DCHECK(tiling_data_.TileBounds(index.i, index.j)
               .Intersects(current_eventually_rect_))
        << this << " " << index.i << "," << index.j << " tile bounds "
        << tiling_data_.TileBounds(index.i, index.j).ToString()
        << " eventually_rect " << current_eventually_rect_.ToString();
  }
#endif
}

bool PictureLayerTiling::IsTileOccludedOnCurrentTree(const Tile* tile) const {
  if (!current_occlusion_in_layer_space_.HasOcclusion())
    return false;
  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());
  gfx::Rect tile_query_rect =
      gfx::IntersectRects(tile_bounds, current_visible_rect_);
  // Explicitly check if the tile is outside the viewport. If so, we need to
  // return false, since occlusion for this tile is unknown.
  if (tile_query_rect.IsEmpty())
    return false;

  tile_query_rect = EnclosingLayerRectFromContentsRect(tile_query_rect);
  return current_occlusion_in_layer_space_.IsOccluded(tile_query_rect);
}

bool PictureLayerTiling::ShouldDecodeCheckeredImagesForTile(
    const Tile* tile) const {
  // If this is the pending tree and the tile is not occluded, any checkered
  // images on this tile should be decoded.
  if (tree_ == PENDING_TREE)
    return !IsTileOccludedOnCurrentTree(tile);

  DCHECK_EQ(tree_, ACTIVE_TREE);
  const PictureLayerTiling* pending_twin =
      client_->GetPendingOrActiveTwinTiling(this);

  // If we don't have a pending twin, then 2 cases are possible. Either we don't
  // have a pending tree, in which case we should be decoding images for tiles
  // which are unoccluded.
  // If we do have a pending tree, then not having a twin implies that this
  // tiling will be evicted upon activation. TODO(khushalsagar): Plumb this
  // information here and return false for this case.
  if (!pending_twin)
    return !IsTileOccludedOnCurrentTree(tile);

  // If the tile will be replaced upon activation, then we don't need to process
  // it for checkered images. Since once the pending tree is activated, it is
  // the new active tree's content that we will invalidate and replace once the
  // decode finishes.
  if (!TilingMatchesTileIndices(pending_twin) ||
      pending_twin->TileAt(tile->tiling_i_index(), tile->tiling_j_index())) {
    return false;
  }

  // Ask the pending twin if this tile will become occluded upon activation.
  return !pending_twin->IsTileOccludedOnCurrentTree(tile);
}

void PictureLayerTiling::UpdateRequiredStatesOnTile(Tile* tile) const {
  tile->set_required_for_activation(IsTileRequiredForActivation(
      tile, [this](const Tile* tile) { return IsTileVisible(tile); },
      IsTileOccluded(tile)));
  tile->set_required_for_draw(IsTileRequiredForDraw(
      tile, [this](const Tile* tile) { return IsTileVisible(tile); }));
}

PictureLayerTiling::CoverageIterator PictureLayerTiling::Cover(
    const gfx::Rect& rect,
    float scale) const {
  return CoverageIterator(this, scale, rect);
}

PrioritizedTile PictureLayerTiling::MakePrioritizedTile(
    Tile* tile,
    PriorityRectType priority_rect_type,
    bool is_tile_occluded) const {
  DCHECK(tile);
  DCHECK(raster_source()->IntersectsRect(tile->enclosing_layer_rect()))
      << "Recording rect: "
      << EnclosingLayerRectFromContentsRect(tile->content_rect()).ToString();

  tile->set_required_for_activation(IsTileRequiredForActivation(
      tile,
      [priority_rect_type](const Tile*) {
        return priority_rect_type == VISIBLE_RECT;
      },
      is_tile_occluded));
  tile->set_required_for_draw(
      IsTileRequiredForDraw(tile, [priority_rect_type](const Tile*) {
        return priority_rect_type == VISIBLE_RECT;
      }));

  const auto& tile_priority =
      ComputePriorityForTile(tile, priority_rect_type, is_tile_occluded);
  DCHECK((!tile->required_for_activation() && !tile->required_for_draw()) ||
         tile_priority.priority_bin == TilePriority::NOW ||
         !client_->HasValidTilePriorities());

  // Note that TileManager will consider this flag but may rasterize the tile
  // anyway (if tile is required for activation for example). We should process
  // the tile for images only if it's further than half of the skewport extent.
  bool process_for_images_only =
      tile_priority.distance_to_visible > min_preraster_distance_ &&
      (tile_priority.distance_to_visible > max_preraster_distance_ ||
       tile_priority.distance_to_visible >
           0.5f * max_skewport_extent_in_screen_space_);

  return PrioritizedTile(tile, this, tile_priority, is_tile_occluded,
                         process_for_images_only,
                         ShouldDecodeCheckeredImagesForTile(tile));
}

void PictureLayerTiling::CreateAllTilesForTesting(
    const gfx::Rect& rect_to_raster) {
  SetTilePriorityRectsForTesting(  // IN-TEST
      rect_to_raster, rect_to_raster, rect_to_raster, rect_to_raster,
      /*evicts_tiles=*/true);
  SetLiveTilesRect(rect_to_raster);
}

std::map<const Tile*, PrioritizedTile>
PictureLayerTiling::UpdateAndGetAllPrioritizedTilesForTesting() const {
  std::map<const Tile*, PrioritizedTile> result;
  for (const auto& index_tile_pair : tiles_) {
    Tile* tile = index_tile_pair.second.get();
    PrioritizedTile prioritized_tile = MakePrioritizedTile(
        tile, ComputePriorityRectTypeForTile(tile), IsTileOccluded(tile));
    result.insert(std::make_pair(prioritized_tile.tile(), prioritized_tile));
  }
  return result;
}

TilePriority PictureLayerTiling::ComputePriorityForTile(
    const Tile* tile,
    PriorityRectType priority_rect_type,
    bool is_tile_occluded) const {
  // TODO(vmpstr): See if this can be moved to iterators.
  DCHECK_EQ(ComputePriorityRectTypeForTile(tile), priority_rect_type);
  DCHECK_EQ(TileAt(tile->tiling_i_index(), tile->tiling_j_index()), tile);

  TilePriority::PriorityBin priority_bin;
  if (client_->HasValidTilePriorities()) {
    // Occluded tiles are given a lower PriorityBin to ensure they are evicted
    // before non-occluded tiles.
    priority_bin = is_tile_occluded ? TilePriority::SOON : TilePriority::NOW;
  } else {
    priority_bin = TilePriority::EVENTUALLY;
  }

  switch (priority_rect_type) {
    case VISIBLE_RECT:
    case PENDING_VISIBLE_RECT:
      return TilePriority(resolution_, priority_bin, 0);
    case SKEWPORT_RECT:
    case SOON_BORDER_RECT:
      if (priority_bin < TilePriority::SOON)
        priority_bin = TilePriority::SOON;
      break;
    case EVENTUALLY_RECT:
      priority_bin = TilePriority::EVENTUALLY;
      break;
  }

  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());
  DCHECK_GT(current_content_to_screen_scale_, 0.f);
  float distance_to_visible =
      current_content_to_screen_scale_ *
      current_visible_rect_.ManhattanInternalDistance(tile_bounds);

  return TilePriority(resolution_, priority_bin, distance_to_visible);
}

PictureLayerTiling::PriorityRectType
PictureLayerTiling::ComputePriorityRectTypeForTile(const Tile* tile) const {
  DCHECK_EQ(TileAt(tile->tiling_i_index(), tile->tiling_j_index()), tile);
  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());

  if (current_visible_rect_.Intersects(tile_bounds))
    return VISIBLE_RECT;

  if (pending_visible_rect().Intersects(tile_bounds))
    return PENDING_VISIBLE_RECT;

  if (current_skewport_rect_.Intersects(tile_bounds))
    return SKEWPORT_RECT;

  if (current_soon_border_rect_.Intersects(tile_bounds))
    return SOON_BORDER_RECT;

  DCHECK(current_eventually_rect_.Intersects(tile_bounds));
  return EVENTUALLY_RECT;
}

void PictureLayerTiling::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (const auto& tile_pair : tiles_) {
    Tile* tile = tile_pair.second.get();
    prioritized_tiles->push_back(MakePrioritizedTile(
        tile, ComputePriorityRectTypeForTile(tile), IsTileOccluded(tile)));
  }
}

void PictureLayerTiling::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("num_tiles", base::saturated_cast<int>(tiles_.size()));
  state->SetDouble("content_scale", contents_scale_key());

  state->BeginDictionary("raster_transform");
  state->BeginArray("scale");
  state->AppendDouble(raster_transform_.scale().x());
  state->AppendDouble(raster_transform_.scale().y());
  state->EndArray();
  state->BeginArray("translation");
  state->AppendDouble(raster_transform_.translation().x());
  state->AppendDouble(raster_transform_.translation().y());
  state->EndArray();
  state->EndDictionary();

  MathUtil::AddToTracedValue("visible_rect", current_visible_rect_, state);
  MathUtil::AddToTracedValue("skewport_rect", current_skewport_rect_, state);
  MathUtil::AddToTracedValue("soon_rect", current_soon_border_rect_, state);
  MathUtil::AddToTracedValue("eventually_rect", current_eventually_rect_,
                             state);
  MathUtil::AddToTracedValue("tiling_rect", tiling_rect(), state);
}

size_t PictureLayerTiling::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
    const Tile* tile = it->second.get();
    amount += tile->GPUMemoryUsageInBytes();
  }
  return amount;
}

gfx::Rect PictureLayerTiling::EnclosingContentsRectFromLayerRect(
    const gfx::Rect& layer_rect) const {
  return ToEnclosingRect(raster_transform_.MapRect(gfx::RectF(layer_rect)));
}

gfx::Rect PictureLayerTiling::EnclosingLayerRectFromContentsRect(
    const gfx::Rect& contents_rect) const {
  return ToEnclosingRect(
      raster_transform_.InverseMapRect(gfx::RectF(contents_rect)));
}

gfx::Rect PictureLayerTiling::ComputeTilingRect() const {
  gfx::Rect recorded_bounds = raster_source_->recorded_bounds();
  gfx::Rect tiling_rect = EnclosingContentsRectFromLayerRect(recorded_bounds);
  gfx::Rect layer_bounds(raster_source_->size());
  if (recorded_bounds != layer_bounds) {
    gfx::Rect layer_contents_rect =
        EnclosingContentsRectFromLayerRect(layer_bounds);
    // Snap tiling_rect to avoid full tiling invalidation on small change of
    // tiling rect origin.
    constexpr int kSnapTexels = 128;
    tiling_rect.SetByBounds(
        MathUtil::UncheckedRoundDown(tiling_rect.x(), kSnapTexels),
        MathUtil::UncheckedRoundDown(tiling_rect.y(), kSnapTexels),
        // Snap to the layer edge if the tiling edge is near the layer edge.
        tiling_rect.right() + kSnapTexels > layer_contents_rect.right()
            ? layer_contents_rect.right()
            : tiling_rect.right(),
        tiling_rect.bottom() + kSnapTexels > layer_contents_rect.bottom()
            ? layer_contents_rect.bottom()
            : tiling_rect.bottom());
    DCHECK(layer_contents_rect.Contains(tiling_rect));
  }
  return tiling_rect;
}

void PictureLayerTiling::SetTilingRect(const gfx::Rect& tiling_rect) {
  if (tiling_data_.tiling_rect() == tiling_rect) {
    return;
  }

  has_visible_rect_tiles_ = tiling_rect.Intersects(current_visible_rect_);
  has_skewport_rect_tiles_ = tiling_rect.Intersects(current_skewport_rect_);
  has_soon_border_rect_tiles_ =
      tiling_rect.Intersects(current_soon_border_rect_);
  has_eventually_rect_tiles_ = tiling_rect.Intersects(current_eventually_rect_);
  tiling_data_.SetTilingRect(tiling_rect);
  tiling_rect_in_layer_space_ = EnclosingLayerRectFromContentsRect(tiling_rect);
}

PictureLayerTiling::TileIterator::TileIterator(PictureLayerTiling* tiling)
    : tiling_(tiling), iter_(tiling->tiles_.begin()) {}

PictureLayerTiling::TileIterator::~TileIterator() = default;

Tile* PictureLayerTiling::TileIterator::GetCurrent() {
  return AtEnd() ? nullptr : iter_->second.get();
}

void PictureLayerTiling::TileIterator::Next() {
  if (!AtEnd())
    ++iter_;
}

bool PictureLayerTiling::TileIterator::AtEnd() const {
  return iter_ == tiling_->tiles_.end();
}

}  // namespace cc
