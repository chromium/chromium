// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/tiles/tile_manager.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/traced_value.h"

namespace cc {

Tile::Tile(TileManager* tile_manager,
           const CreateInfo& info,
           int layer_id,
           int source_frame_number,
           int flags)
    : tile_manager_(tile_manager),
      tiling_(info.tiling),
      content_rect_(info.content_rect),
      enclosing_layer_rect_(info.enclosing_layer_rect),
      raster_transform_(info.raster_transform),
      layer_id_(layer_id),
      source_frame_number_(source_frame_number),
      flags_(flags),
      tiling_i_index_(info.tiling_i_index),
      tiling_j_index_(info.tiling_j_index),
      can_use_lcd_text_(info.can_use_lcd_text),
      id_(tile_manager->GetUniqueTileId()) {
  raster_rects_.emplace_back(info.content_rect, info.raster_transform);
}

Tile::~Tile() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      "cc::Tile", this);
  tile_manager_->Release(this);
}

void Tile::AsValueInto(base::trace_event::TracedValue* value) const {
  viz::TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), value, "cc::Tile", this);
  value->SetDouble("contents_scale", contents_scale_key());

  value->BeginDictionary("raster_transform");
  value->BeginArray("scale");
  value->AppendDouble(raster_transform_.scale().x());
  value->AppendDouble(raster_transform_.scale().y());
  value->EndArray();
  value->BeginArray("translation");
  value->AppendDouble(raster_transform_.translation().x());
  value->AppendDouble(raster_transform_.translation().y());
  value->EndArray();
  value->EndDictionary();

  MathUtil::AddToTracedValue("content_rect", content_rect_, value);

  value->SetInteger("layer_id", layer_id_);

  value->BeginDictionary("draw_info");
  draw_info_.AsValueInto(value);
  value->EndDictionary();

  value->SetBoolean("has_resource", draw_info().has_resource());
  value->SetBoolean("is_using_gpu_memory",
                    draw_info().has_resource() || HasRasterTask());
  value->SetInteger("scheduled_priority", scheduled_priority_);
  value->SetBoolean("use_picture_analysis", use_picture_analysis());
  value->SetInteger("gpu_memory_usage",
                    base::saturated_cast<int>(GPUMemoryUsageInBytes()));
}

bool Tile::HasMissingLCPCandidateImages() const {
  return HasRasterTask() && raster_task_->TaskContainsLCPCandidateImages();
}

size_t Tile::GPUMemoryUsageInBytes() const {
  if (draw_info_.resource_) {
    // We don't need to validate the computed size, since the tile size is
    // determined by the compositor.
    return draw_info_.resource_shared_image_format().EstimatedSizeInBytes(
        draw_info_.resource_size());
  }
  return 0;
}

}  // namespace cc
