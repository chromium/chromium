// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_PICTURE_LAYER_TILING_H_
#define CC_TILES_PICTURE_LAYER_TILING_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/base/region.h"
#include "cc/base/tiling_data.h"
#include "cc/cc_export.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/paint/scroll_offset_map.h"
#include "cc/raster/raster_source.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_index.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_coverage_iterator.h"
#include "cc/trees/occlusion.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {

class DrawImage;
class GlobalStateThatImpactsTilePriority;
class PictureLayerTiling;
class PrioritizedTile;

class CC_EXPORT PictureLayerTilingClient {
 public:
  // Create a tile at the given content_rect (in the contents scale of the
  // tiling) This might return null if the client cannot create such a tile.
  virtual std::unique_ptr<Tile> CreateTile(const Tile::CreateInfo& info) = 0;
  virtual gfx::Size CalculateTileSize(const gfx::Size& content_bounds) = 0;
  // This invalidation region defines the area (if any, it can by null) that
  // tiles can not be shared between pending and active trees.
  virtual const Region* GetPendingInvalidation() = 0;
  virtual const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const = 0;
  virtual bool HasValidTilePriorities() const = 0;
  virtual bool RequiresHighResToDraw() const = 0;
  virtual const PaintWorkletRecordMap& GetPaintWorkletRecords() const = 0;
  virtual std::vector<const DrawImage*> GetDiscardableImagesInRect(
      const gfx::Rect& rect) const = 0;
  virtual ScrollOffsetMap GetRasterInducingScrollOffsets() const = 0;
  virtual const GlobalStateThatImpactsTilePriority& global_tile_state()
      const = 0;

 protected:
  virtual ~PictureLayerTilingClient() {}
};

class PictureLayerTilingCoverageIterator;

class CC_EXPORT PictureLayerTiling {
 public:
  using Tile = Tile;
  using CoverageIterator = PictureLayerTilingCoverageIterator;

  static const int kBorderTexels = 1;

  // Note on raster_transform: In general raster_transform could be arbitrary,
  // the only restriction is that the layer bounds after transform should
  // be positive (because the tiling logic doesn't support negative space).
  // Also the implementation checks the transformed bounds leaves less than
  // 1px margin on top left edges, because there is few reason to do so.
  PictureLayerTiling(WhichTree tree,
                     const gfx::AxisTransform2d& raster_transform,
                     scoped_refptr<RasterSource> raster_source,
                     PictureLayerTilingClient* client,
                     float min_preraster_distance,
                     float max_preraster_distance,
                     bool can_use_lcd_text);
  PictureLayerTiling(const PictureLayerTiling&) = delete;
  ~PictureLayerTiling();

  PictureLayerTiling& operator=(const PictureLayerTiling&) = delete;

  PictureLayerTilingClient* client() const { return client_; }

  // Returns true if the current tiling needs to update tile priority rects and
  // tiles.
  bool SetRasterSourceAndResize(scoped_refptr<RasterSource> raster_source);
  void Invalidate(const Region& layer_invalidation);
  void CreateMissingTilesInLiveTilesRect();
  void TakeTilesAndPropertiesFrom(PictureLayerTiling* pending_twin,
                                  const Region& layer_invalidation);

  bool IsTileRequiredForActivation(const Tile* tile) const {
    return IsTileRequiredForActivation(
        tile, [this](const Tile* tile) { return IsTileVisible(tile); },
        IsTileOccluded(tile));
  }

  bool IsTileRequiredForDraw(const Tile* tile) const {
    return IsTileRequiredForDraw(
        tile, [this](const Tile* tile) { return IsTileVisible(tile); });
  }

  // Returns true if the tile should be processed for decoding images skipped
  // during rasterization.
  bool ShouldDecodeCheckeredImagesForTile(const Tile* tile) const;

  void set_resolution(TileResolution resolution) {
    resolution_ = resolution;
    may_contain_low_resolution_tiles_ |= resolution == LOW_RESOLUTION;
  }
  TileResolution resolution() const { return resolution_; }
  bool may_contain_low_resolution_tiles() const {
    return may_contain_low_resolution_tiles_;
  }
  void reset_may_contain_low_resolution_tiles() {
    may_contain_low_resolution_tiles_ = false;
  }
  void set_can_require_tiles_for_activation(bool can_require_tiles) {
    can_require_tiles_for_activation_ = can_require_tiles;
  }
  bool can_require_tiles_for_activation() const {
    return can_require_tiles_for_activation_;
  }

  gfx::Size raster_size() const { return raster_source()->size(); }
  const scoped_refptr<RasterSource>& raster_source() const {
    return raster_source_;
  }
  const PaintWorkletRecordMap& GetPaintWorkletRecords() const {
    return client_->GetPaintWorkletRecords();
  }
  ScrollOffsetMap GetRasterInducingScrollOffsets() const {
    return client_->GetRasterInducingScrollOffsets();
  }
  const gfx::Rect& tiling_rect() const { return tiling_data_.tiling_rect(); }
  const gfx::Rect& live_tiles_rect() const { return live_tiles_rect_; }
  gfx::Size tile_size() const { return tiling_data_.max_texture_size(); }
  // PictureLayerTilingSet uses the scale component of the raster transform
  // as the key for indexing and sorting. In theory we can have multiple
  // tilings with the same scale but different translation, but currently
  // we only allow tilings with unique scale for the sake of simplicity.
  float contents_scale_key() const {
    const gfx::Vector2dF& scale = raster_transform_.scale();
    return std::max(scale.x(), scale.y());
  }
  const gfx::AxisTransform2d& raster_transform() const {
    return raster_transform_;
  }
  const TilingData* tiling_data() const { return &tiling_data_; }

  Tile* TileAt(int i, int j) const { return TileAt(TileIndex(i, j)); }

  Tile* TileAt(const TileIndex& index) const {
    TileMap::const_iterator iter = tiles_.find(index);
    return iter == tiles_.end() ? nullptr : iter->second.get();
  }

  bool has_tiles() const { return !tiles_.empty(); }
  // all_tiles_done() can return false negatives.
  bool all_tiles_done() const { return all_tiles_done_; }
  void set_all_tiles_done(bool all_tiles_done) {
    all_tiles_done_ = all_tiles_done;
  }

  bool can_use_lcd_text() const { return can_use_lcd_text_; }

  WhichTree tree() const { return tree_; }

  void VerifyNoTileNeedsRaster() const {
#if DCHECK_IS_ON()
    for (const auto& tile_pair : tiles_) {
      DCHECK(!tile_pair.second->draw_info().NeedsRaster() ||
             IsTileOccluded(tile_pair.second.get()));
    }
#endif  // DCHECK_IS_ON()
  }

  // For testing functionality.
  void CreateAllTilesForTesting() {
    CreateAllTilesForTesting(tiling_data_.tiling_rect());
  }
  void CreateAllTilesForTesting(const gfx::Rect& rect_to_raster);
  const TilingData& TilingDataForTesting() const { return tiling_data_; }
  std::vector<Tile*> AllTilesForTesting() const {
    std::vector<Tile*> all_tiles;
    for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
      all_tiles.push_back(it->second.get());
    return all_tiles;
  }

  void UpdateAllRequiredStateForTesting() {
    for (const auto& [index, tile] : tiles_) {
      UpdateRequiredStatesOnTile(tile.get());
    }
  }
  std::map<const Tile*, PrioritizedTile>
  UpdateAndGetAllPrioritizedTilesForTesting() const;

  void SetAllTilesOccludedForTesting() {
    gfx::Rect viewport_in_layer_space =
        EnclosingLayerRectFromContentsRect(current_visible_rect_);
    current_occlusion_in_layer_space_ =
        Occlusion(gfx::Transform(),
                  SimpleEnclosedRegion(viewport_in_layer_space),
                  SimpleEnclosedRegion(viewport_in_layer_space));
  }
  const gfx::Rect& GetCurrentVisibleRectForTesting() const {
    return current_visible_rect_;
  }
  void SetTilePriorityRectsForTesting(const gfx::Rect& visible_rect,
                                      const gfx::Rect& skewport_rect,
                                      const gfx::Rect& soon_border_rect,
                                      const gfx::Rect& eventually_rect,
                                      bool evicts_tiles = false);

  using TileMap = std::unordered_map<TileIndex, std::unique_ptr<Tile>>;

  // Iterates over the tiles of a PictureLayerTiling. Order of iteration is not
  // defined.
  class CC_EXPORT TileIterator {
   public:
    explicit TileIterator(PictureLayerTiling* tiling);
    ~TileIterator();

    Tile* GetCurrent();
    void Next();
    bool AtEnd() const;

   private:
    raw_ptr<PictureLayerTiling> tiling_;
    PictureLayerTiling::TileMap::iterator iter_;
  };

  void Reset();

  void ComputeTilePriorityRects(
      const gfx::Rect& visible_rect_in_layer_space,
      const gfx::Rect& skewport_rect_in_layer_space,
      const gfx::Rect& soon_border_rect_in_layer_space,
      const gfx::Rect& eventually_rect_in_layer_space,
      float ideal_contents_scale,
      const Occlusion& occlusion_in_layer_space);

  void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  void AsValueInto(base::trace_event::TracedValue* array) const;
  size_t GPUMemoryUsageInBytes() const;

  void UpdateRequiredStatesOnTile(Tile* tile) const;

  // Creates a coverage iterator to iterate over a minimal set of tiles from
  // this tiling to cover `rect`, which has been pre-scaled by `scale`.
  CoverageIterator Cover(const gfx::Rect& rect, float scale) const;

 protected:
  friend class PrioritizedTile;
  friend class TileIterator;
  friend class TilingSetRasterQueueAll;
  friend class TilingSetRasterQueueRequired;
  friend class TilingSetEvictionQueue;
  friend class TilesWithResourceIterator;

  // PENDING VISIBLE RECT refers to the visible rect that will become current
  // upon activation (ie, the pending tree's visible rect). Tiles in this
  // region that are not part of the current visible rect are all handled
  // here. Note that when processing a pending tree, this rect is the same as
  // the visible rect so no tiles are processed in this case.
  enum PriorityRectType {
    VISIBLE_RECT,
    PENDING_VISIBLE_RECT,
    SKEWPORT_RECT,
    SOON_BORDER_RECT,
    EVENTUALLY_RECT
  };

  bool IsTileVisible(const Tile* tile) const {
    gfx::Rect tile_bounds =
        tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());
    return tile_bounds.Intersects(current_visible_rect_);
  }

  template <typename VisibilityChecker>
  bool IsTileRequiredForActivation(const Tile* tile,
                                   VisibilityChecker is_visible,
                                   bool is_tile_occluded) const {
    if (tree_ == PENDING_TREE) {
      if (!can_require_tiles_for_activation_ ||
          resolution_ != HIGH_RESOLUTION || is_tile_occluded) {
        return false;
      }

      // We may be checking the active tree tile here (since this function is
      // also called for active trees below, ensure that this is at all a valid
      // tile on the pending tree.
      if (tile->tiling_i_index() >= tiling_data_.num_tiles_x() ||
          tile->tiling_j_index() >= tiling_data_.num_tiles_y()) {
        return false;
      }

      if (!is_visible(tile))
        return false;

      if (client_->RequiresHighResToDraw())
        return true;

      const PictureLayerTiling* active_twin =
          client_->GetPendingOrActiveTwinTiling(this);
      if (!active_twin || !TilingMatchesTileIndices(active_twin))
        return true;

      if (active_twin->raster_source()->size() != raster_source()->size() ||
          active_twin->raster_source()->recorded_bounds() !=
              raster_source()->recorded_bounds()) {
        return true;
      }

      if (active_twin->current_visible_rect_ != current_visible_rect_)
        return true;

      Tile* twin_tile =
          active_twin->TileAt(tile->tiling_i_index(), tile->tiling_j_index());
      if (!twin_tile)
        return false;
      return true;
    }

    DCHECK_EQ(tree_, ACTIVE_TREE);
    const PictureLayerTiling* pending_twin =
        client_->GetPendingOrActiveTwinTiling(this);
    // If we don't have a pending tree, or the pending tree will overwrite the
    // given tile, then it is not required for activation.
    if (!pending_twin || !TilingMatchesTileIndices(pending_twin) ||
        pending_twin->TileAt(tile->tiling_i_index(), tile->tiling_j_index())) {
      return false;
    }
    // Otherwise, ask the pending twin if this tile is required for activation.
    return pending_twin->IsTileRequiredForActivation(tile);
  }

  template <typename VisibilityChecker>
  bool IsTileRequiredForDraw(const Tile* tile,
                             VisibilityChecker is_visible) const {
    return tree_ == ACTIVE_TREE && resolution_ == HIGH_RESOLUTION &&
           is_visible(tile) && !IsTileOccludedOnCurrentTree(tile);
  }

  void SetLiveTilesRect(const gfx::Rect& live_tiles_rect);
  void VerifyTiles() const;
  Tile* CreateTile(const Tile::CreateInfo& info);
  // Removes the tile at i, j and returns it. Returns nullptr if the tile did
  // not exist.
  std::unique_ptr<Tile> TakeTileAt(int i, int j);
  bool TilingMatchesTileIndices(const PictureLayerTiling* twin) const;

  void SetPriorityRect(const gfx::Rect& rect_in_layer_space,
                       PriorityRectType rect_type,
                       bool evicts_tiles = false);

  bool IsTileOccludedOnCurrentTree(const Tile* tile) const;
  Tile::CreateInfo CreateInfoForTile(int i, int j) const;
  bool ShouldCreateTileAt(const Tile::CreateInfo& info) const;

  bool IsTileOccluded(const Tile* tile) const {
    // If this tile is not occluded on this tree, then it is not occluded.
    if (!IsTileOccludedOnCurrentTree(tile))
      return false;

    // Otherwise, if this is the pending tree, we're done and the tile is
    // occluded.
    if (tree_ == PENDING_TREE)
      return true;

    // On the active tree however, we need to check if this tile will be
    // unoccluded upon activation, in which case it has to be considered
    // unoccluded.
    const PictureLayerTiling* pending_twin =
        client_->GetPendingOrActiveTwinTiling(this);
    if (pending_twin) {
      // If there's a pending tile in the same position. Or if the pending twin
      // would have to be creating all tiles, then we don't need to worry about
      // occlusion on the twin.
      if (!TilingMatchesTileIndices(pending_twin) ||
          pending_twin->TileAt(tile->tiling_i_index(),
                               tile->tiling_j_index())) {
        return true;
      }
      return pending_twin->IsTileOccludedOnCurrentTree(tile);
    }
    return true;
  }

  PrioritizedTile MakePrioritizedTile(Tile* tile,
                                      PriorityRectType priority_rect_type,
                                      bool is_tile_occluded) const;
  TilePriority ComputePriorityForTile(const Tile* tile,
                                      PriorityRectType priority_rect_type,
                                      bool is_tile_occluded) const;
  PriorityRectType ComputePriorityRectTypeForTile(const Tile* tile) const;
  bool has_visible_rect_tiles() const { return has_visible_rect_tiles_; }
  bool has_skewport_rect_tiles() const { return has_skewport_rect_tiles_; }
  bool has_soon_border_rect_tiles() const {
    return has_soon_border_rect_tiles_;
  }
  bool has_eventually_rect_tiles() const { return has_eventually_rect_tiles_; }

  const gfx::Rect& current_visible_rect() const {
    return current_visible_rect_;
  }
  gfx::Rect pending_visible_rect() const {
    const PictureLayerTiling* pending_tiling =
        tree_ == ACTIVE_TREE ? client_->GetPendingOrActiveTwinTiling(this)
                             : this;
    if (pending_tiling)
      return pending_tiling->current_visible_rect();
    return gfx::Rect();
  }
  const gfx::Rect& current_skewport_rect() const {
    return current_skewport_rect_;
  }
  const gfx::Rect& current_soon_border_rect() const {
    return current_soon_border_rect_;
  }
  const gfx::Rect& current_eventually_rect() const {
    return current_eventually_rect_;
  }
  void RemoveTilesInRegion(const Region& layer_region, bool recreate_tiles);

  gfx::Rect EnclosingContentsRectFromLayerRect(
      const gfx::Rect& layer_rect) const;
  gfx::Rect EnclosingLayerRectFromContentsRect(
      const gfx::Rect& contents_rect) const;

  gfx::Rect ComputeTilingRect() const;
  void SetTilingRect(const gfx::Rect& tiling_rect);

  // Given properties.
  const gfx::AxisTransform2d raster_transform_;
  const raw_ptr<PictureLayerTilingClient> client_;
  const WhichTree tree_;
  scoped_refptr<RasterSource> raster_source_;
  const float min_preraster_distance_;
  const float max_preraster_distance_;
  TileResolution resolution_ = NON_IDEAL_RESOLUTION;
  bool may_contain_low_resolution_tiles_ = false;

  // Internal data.
  TilingData tiling_data_{gfx::Size(), gfx::Rect(), kBorderTexels};
  TileMap tiles_;  // It is not legal to have a NULL tile in the tiles_ map.
  gfx::Rect live_tiles_rect_;

  bool can_require_tiles_for_activation_ = false;

  gfx::Rect tiling_rect_in_layer_space_;

  // Iteration rects in content space.
  gfx::Rect current_visible_rect_;
  gfx::Rect current_skewport_rect_;
  gfx::Rect current_soon_border_rect_;
  gfx::Rect current_eventually_rect_;

  // Iteration rects in layer space.
  gfx::Rect current_visible_rect_in_layer_space_;
  gfx::Rect current_skewport_rect_in_layer_space_;
  gfx::Rect current_soon_border_rect_in_layer_space_;
  gfx::Rect current_eventually_rect_in_layer_space_;

  // Other properties used for tile iteration and prioritization.
  float current_content_to_screen_scale_ = 0.f;
  Occlusion current_occlusion_in_layer_space_;
  float max_skewport_extent_in_screen_space_ = 0.f;

  bool has_visible_rect_tiles_ : 1 = false;
  bool has_skewport_rect_tiles_ : 1 = false;
  bool has_soon_border_rect_tiles_ : 1 = false;
  bool has_eventually_rect_tiles_ : 1 = false;
  bool all_tiles_done_ : 1 = true;
  bool can_use_lcd_text_ : 1;
};

class PictureLayerTilingCoverageIterator
    : public TilingCoverageIterator<PictureLayerTiling> {
 public:
  using TilingCoverageIterator<PictureLayerTiling>::TilingCoverageIterator;
};

}  // namespace cc

#endif  // CC_TILES_PICTURE_LAYER_TILING_H_
