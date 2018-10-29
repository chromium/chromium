// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile.h"

#include <stddef.h>

#include <algorithm>

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
           int flags,
           bool can_use_lcd_text)
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
      required_for_activation_(false),
      required_for_draw_(false),
      is_solid_color_analysis_performed_(false),
      can_use_lcd_text_(can_use_lcd_text),
      id_(tile_manager->GetUniqueTileId()),
      invalidated_id_(0),
      scheduled_priority_(0) {}

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

  value->BeginArray("raster_transform");
  value->AppendDouble(raster_transform_.scale());
  value->AppendDouble(raster_transform_.translation().x());
  value->AppendDouble(raster_transform_.translation().y());
  value->EndArray();

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

size_t Tile::GPUMemoryUsageInBytes() const {
  if (draw_info_.resource_) {
    // We can use UncheckedSizeInBytes, since the tile size is determined by the
    // compositor.
    return viz::ResourceSizes::UncheckedSizeInBytes<size_t>(
        draw_info_.resource_size(), draw_info_.resource_format());
  }
  return 0;
}

}  // namespace cc
