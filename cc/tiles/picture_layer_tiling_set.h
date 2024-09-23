// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_PICTURE_LAYER_TILING_SET_H_
#define CC_TILES_PICTURE_LAYER_TILING_SET_H_

#include <stddef.h>

#include <deque>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/base/region.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/tiling_set_coverage_iterator.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {

class CC_EXPORT PictureLayerTilingSet {
 public:
  enum TilingRangeType {
    HIGHER_THAN_HIGH_RES,
    HIGH_RES,
    BETWEEN_HIGH_AND_LOW_RES,
    LOW_RES,
    LOWER_THAN_LOW_RES
  };
  struct TilingRange {
    TilingRange(size_t start, size_t end) : start(start), end(end) {}

    size_t start;
    size_t end;
  };

  static std::unique_ptr<PictureLayerTilingSet> Create(
      WhichTree tree,
      PictureLayerTilingClient* client,
      int tiling_interest_area_padding,
      float skewport_target_time_in_seconds,
      int skewport_extrapolation_limit_in_screen_pixels,
      float max_preraster_distance);

  PictureLayerTilingSet(const PictureLayerTilingSet&) = delete;
  ~PictureLayerTilingSet();

  PictureLayerTilingSet& operator=(const PictureLayerTilingSet&) = delete;

  const PictureLayerTilingClient* client() const { return client_; }

  void CleanUpTilings(
      float min_acceptable_high_res_scale_key,
      float max_acceptable_high_res_scale_key,
      const std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>>&
          needed_tilings,
      PictureLayerTilingSet* twin_set);
  void RemoveNonIdealTilings();

  // This function is called on the active tree during activation.
  void UpdateTilingsToCurrentRasterSourceForActivation(
      scoped_refptr<RasterSource> raster_source,
      const PictureLayerTilingSet* pending_twin_set,
      const Region& layer_invalidation,
      float minimum_contents_scale,
      float maximum_contents_scale);

  // This function is called on the sync tree during commit.
  void UpdateTilingsToCurrentRasterSourceForCommit(
      scoped_refptr<RasterSource> raster_source,
      const Region& layer_invalidation,
      float minimum_contents_scale,
      float maximum_contents_scale);

  // Invalidates the region on all tilings and recreates the tiles as needed.
  void Invalidate(const Region& layer_invalidation);

  PictureLayerTiling* AddTiling(const gfx::AxisTransform2d& raster_transform,
                                scoped_refptr<RasterSource> raster_source,
                                bool can_use_lcd_text = false);
  size_t num_tilings() const { return tilings_.size(); }
  // all_tiles_done() can return false negatives because:
  //   1) PictureLayerTiling::all_tiles_done() could return false negatives;
  //   2) it's not re-computed when a PictureLayerTiling is removed from
  //      the set.
  bool all_tiles_done() const { return all_tiles_done_; }
  void set_all_tiles_done(bool done) { all_tiles_done_ = done; }
  int NumHighResTilings() const;
  PictureLayerTiling* tiling_at(size_t idx) { return tilings_[idx].get(); }
  const PictureLayerTiling* tiling_at(size_t idx) const {
    return tilings_[idx].get();
  }
  WhichTree tree() const { return tree_; }

  PictureLayerTiling* FindTilingWithScaleKey(float scale_key) const;
  PictureLayerTiling* FindTilingWithResolution(TileResolution resolution) const;

  // If a tiling exists whose scale is within |snap_to_existing_tiling_ratio|
  // ratio of |start_scale|, then return that tiling. Otherwise, return null.
  // If multiple tilings match the criteria, return the one with the least ratio
  // to |start_scale|.
  PictureLayerTiling* FindTilingWithNearestScaleKey(
      float start_scale,
      float snap_to_existing_tiling_ratio) const;

  void MarkAllTilingsNonIdeal();

  // Returns the maximum contents scale of all tilings, or 0 if no tilings
  // exist. Note that this returns the maximum of x and y scales depending on
  // the aspect ratio.
  float GetMaximumContentsScale() const;

  // Remove one tiling.
  void Remove(PictureLayerTiling* tiling);

  // Removes all tilings with a contents scale key < |minimum_scale_key|.
  void RemoveTilingsBelowScaleKey(float minimum_scale_key);

  // Removes all tilings with a contents scale key > |maximum_scale_key|.
  void RemoveTilingsAboveScaleKey(float maximum_scale);

  // Removes all resources (tilings, raster source).
  void ReleaseAllResources();

  // Remove all tilings.
  void RemoveAllTilings();

  // Remove all tiles; keep all tilings.
  void RemoveAllTiles();

  // Update the rects and priorities for tiles based on the given information.
  // Returns true if PrepareTiles is required.
  bool UpdateTilePriorities(const gfx::Rect& required_rect_in_layer_space,
                            float ideal_contents_scale,
                            double current_frame_time_in_seconds,
                            const Occlusion& occlusion_in_layer_space,
                            bool can_require_tiles_for_activation);

  void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;

  using CoverageIterator = TilingSetCoverageIterator<PictureLayerTiling>;
  CoverageIterator Cover(const gfx::Rect& coverage_rect,
                         float coverage_scale,
                         float ideal_contents_scale);

  void AsValueInto(base::trace_event::TracedValue* array) const;
  size_t GPUMemoryUsageInBytes() const;

  TilingRange GetTilingRange(TilingRangeType type) const;

 protected:
  struct FrameVisibleRect {
    FrameVisibleRect(const gfx::Rect& rect, double time_in_seconds)
        : visible_rect_in_layer_space(rect),
          frame_time_in_seconds(time_in_seconds) {}

    gfx::Rect visible_rect_in_layer_space;
    double frame_time_in_seconds;
  };

  struct StateSinceLastTilePriorityUpdate {
    class AutoClear {
     public:
      explicit AutoClear(StateSinceLastTilePriorityUpdate* state_to_clear)
          : state_to_clear_(state_to_clear) {}
      ~AutoClear() { *state_to_clear_ = StateSinceLastTilePriorityUpdate(); }

     private:
      // RAW_PTR_EXCLUSION: Performance reasons: on-stack pointer + based on
      // analysis of sampling profiler data
      // (PictureLayerTilingSet::UpdateTilePriorities -> creates AutoClear
      // on stack).
      RAW_PTR_EXCLUSION StateSinceLastTilePriorityUpdate* state_to_clear_;
    };

    StateSinceLastTilePriorityUpdate() = default;

    bool invalidated = false;
    bool added_tilings = false;
    bool tiling_needs_update = false;
  };

  explicit PictureLayerTilingSet(
      WhichTree tree,
      PictureLayerTilingClient* client,
      int tiling_interest_area_padding,
      float skewport_target_time_in_seconds,
      int skewport_extrapolation_limit_in_screen_pixels,
      float max_preraster_distance);

  void CopyTilingsAndPropertiesFromPendingTwin(
      const PictureLayerTilingSet* pending_twin_set,
      scoped_refptr<RasterSource> raster_source,
      const Region& layer_invalidation);

  void VerifyTilings(const PictureLayerTilingSet* pending_twin_set) const;

  bool TilingsNeedUpdate(const gfx::Rect& required_rect_in_layer_space,
                         double current_frame_time_in_Seconds);
  gfx::Rect ComputeSkewport(const gfx::Rect& visible_rect_in_layer_space,
                            double current_frame_time_in_seconds,
                            float ideal_contents_scale);
  gfx::Rect ComputeSoonBorderRect(const gfx::Rect& visible_rect_in_layer_space,
                                  float ideal_contents_scale);
  void UpdatePriorityRects(const gfx::Rect& visible_rect_in_layer_space,
                           double current_frame_time_in_seconds,
                           float ideal_contents_scale);

  std::vector<std::unique_ptr<PictureLayerTiling>> tilings_;

  bool all_tiles_done_ : 1 = true;

  const int tiling_interest_area_padding_;
  const float skewport_target_time_in_seconds_;
  const int skewport_extrapolation_limit_in_screen_pixels_;
  WhichTree tree_;
  raw_ptr<PictureLayerTilingClient> client_;
  const float max_preraster_distance_;
  // State saved for computing velocities based on finite differences.
  // .front() of the deque refers to the most recent FrameVisibleRect.
  std::deque<FrameVisibleRect> visible_rect_history_;
  StateSinceLastTilePriorityUpdate state_since_last_tile_priority_update_;

  scoped_refptr<RasterSource> raster_source_;

  gfx::Rect visible_rect_in_layer_space_;
  gfx::Rect skewport_rect_in_layer_space_;
  gfx::Rect soon_border_rect_in_layer_space_;
  gfx::Rect eventually_rect_in_layer_space_;

  friend class Iterator;
};

}  // namespace cc

#endif  // CC_TILES_PICTURE_LAYER_TILING_SET_H_
