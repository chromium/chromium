// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_H_
#define CC_TILES_TILE_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/draw_image.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/tile_draw_info.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class PictureLayerTiling;
class TileManager;

class CC_EXPORT Tile {
 public:
  struct CreateInfo {
    // RAW_PTR_EXCLUSION: Performance reasons: on-stack pointer + based on
    // analysis of sampling profiler data
    // (PictureLayerTilingSet::UpdateTilePriorities ->
    // PictureLayerTiling::ComputeTilePriorityRects ->
    // PictureLayerTiling::SetLiveTilesRect -> creates Tile::CreateInfo).
    RAW_PTR_EXCLUSION const PictureLayerTiling* tiling = nullptr;
    int tiling_i_index = 0;
    int tiling_j_index = 0;
    gfx::Rect enclosing_layer_rect;
    gfx::Rect content_rect;
    gfx::AxisTransform2d raster_transform;
    bool can_use_lcd_text = false;
  };

  enum TileRasterFlags { USE_PICTURE_ANALYSIS = 1 << 0, IS_OPAQUE = 1 << 1 };

  typedef uint64_t Id;

  Tile(const Tile&) = delete;
  ~Tile();

  Tile& operator=(const Tile&) = delete;

  Id id() const {
    return id_;
  }

  // TODO(vmpstr): Move this to the iterators.
  bool required_for_activation() const { return required_for_activation_; }
  void set_required_for_activation(bool is_required) {
    required_for_activation_ = is_required;
  }
  bool required_for_draw() const { return required_for_draw_; }
  void set_required_for_draw(bool is_required) {
    required_for_draw_ = is_required;
  }

  bool is_prepaint() const {
    return !required_for_activation() && !required_for_draw();
  }

  bool use_picture_analysis() const {
    return !!(flags_ & USE_PICTURE_ANALYSIS);
  }

  bool is_opaque() const { return !!(flags_ & IS_OPAQUE); }

  void AsValueInto(base::trace_event::TracedValue* value) const;

  const TileDrawInfo& draw_info() const { return draw_info_; }
  TileDrawInfo& draw_info() { return draw_info_; }

  float contents_scale_key() const {
    const gfx::Vector2dF& scale = raster_transform_.scale();
    return std::max(scale.x(), scale.y());
  }
  const gfx::AxisTransform2d& raster_transform() const {
    return raster_transform_;
  }
  const gfx::Rect& content_rect() const { return content_rect_; }
  const gfx::Rect& enclosing_layer_rect() const {
    return enclosing_layer_rect_;
  }

  int layer_id() const { return layer_id_; }

  int source_frame_number() const { return source_frame_number_; }

  bool IsReadyToDraw() const { return draw_info().IsReadyToDraw(); }

  size_t GPUMemoryUsageInBytes() const;

  const gfx::Size& desired_texture_size() const { return content_rect_.size(); }

  int tiling_i_index() const { return tiling_i_index_; }
  int tiling_j_index() const { return tiling_j_index_; }

  void SetInvalidated(const gfx::Rect& invalid_content_rect,
                      Id previous_tile_id) {
    invalidated_content_rect_ = invalid_content_rect;
    invalidated_id_ = previous_tile_id;
  }

  Id invalidated_id() const { return invalidated_id_; }
  const gfx::Rect& invalidated_content_rect() const {
    return invalidated_content_rect_;
  }

  bool HasRasterTask() const { return !!raster_task_.get(); }

  bool HasMissingLCPCandidateImages() const;

  void set_solid_color_analysis_performed(bool performed) {
    is_solid_color_analysis_performed_ = performed;
  }
  bool is_solid_color_analysis_performed() const {
    return is_solid_color_analysis_performed_;
  }
  bool can_use_lcd_text() const { return can_use_lcd_text_; }

  bool set_raster_task_scheduled_with_checker_images(bool has_checker_images) {
    bool previous_value = raster_task_scheduled_with_checker_images_;
    raster_task_scheduled_with_checker_images_ = has_checker_images;
    return previous_value;
  }
  bool raster_task_scheduled_with_checker_images() const {
    return raster_task_scheduled_with_checker_images_;
  }

  const PictureLayerTiling* tiling() const { return tiling_; }
  void set_tiling(const PictureLayerTiling* tiling) { tiling_ = tiling; }

  void mark_used() { used_ = true; }
  void clear_used() { used_ = false; }
  bool used() const { return used_; }

 private:
  friend class TileManager;
  friend class FakeTileManager;
  friend class FakePictureLayerImpl;

  // Methods called by by tile manager.
  Tile(TileManager* tile_manager,
       const CreateInfo& info,
       int layer_id,
       int source_frame_number,
       int flags);

  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data (PictureLayerTilingSet::UpdateTilePriorities ->
  // PictureLayerTiling::ComputeTilePriorityRects ->
  // PictureLayerTiling::SetLiveTilesRect -> PictureLayerTiling::CreateTile ->
  // allocates Tile).
  RAW_PTR_EXCLUSION TileManager* const tile_manager_;
  RAW_PTR_EXCLUSION const PictureLayerTiling* tiling_;

  const gfx::Rect content_rect_;
  const gfx::Rect enclosing_layer_rect_;
  const gfx::AxisTransform2d raster_transform_;

  TileDrawInfo draw_info_;

  const int layer_id_;
  const int source_frame_number_;
  const int flags_;
  const int tiling_i_index_;
  const int tiling_j_index_;

  // The |id_| of the Tile that was invalidated and replaced by this tile.
  Id invalidated_id_ = 0;

  unsigned scheduled_priority_ = 0;

  bool required_for_activation_ : 1 = false;
  bool required_for_draw_ : 1 = false;
  bool is_solid_color_analysis_performed_ : 1 = false;
  const bool can_use_lcd_text_ : 1;

  // Set to true if there is a raster task scheduled for this tile that will
  // rasterize a resource with checker images.
  bool raster_task_scheduled_with_checker_images_ : 1 = false;

  Id id_;

  // List of Rect-Transform pairs, representing unoccluded parts of the
  // tile, to support raster culling. See Bug: 1071932
  std::vector<std::pair<const gfx::Rect, const gfx::AxisTransform2d>>
      raster_rects_;

  // The rect bounding the changes in this Tile vs the previous tile it
  // replaced.
  gfx::Rect invalidated_content_rect_;

  scoped_refptr<TileTask> raster_task_;

  bool used_ = false;
};

}  // namespace cc

#endif  // CC_TILES_TILE_H_
