// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/recording_source.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_math.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/solid_color_analyzer.h"
#include "cc/raster/raster_source.h"

namespace {

// We don't perform per-layer solid color analysis when there are too many skia
// operations.
const int kMaxOpsToAnalyzeForLayer = 10;

}  // namespace

namespace cc {

RecordingSource::RecordingSource() = default;
RecordingSource::~RecordingSource() = default;

void RecordingSource::UpdateInvalidationForRecordedBounds(
    const gfx::Rect& old_recorded_bounds,
    const gfx::Rect& new_recorded_bounds,
    Region& invalidation) {
  // Invalidate newly-exposed and no-longer-exposed areas.
  Region newly_exposed_region(new_recorded_bounds);
  newly_exposed_region.Subtract(old_recorded_bounds);
  invalidation.Union(newly_exposed_region);

  Region no_longer_exposed_region(old_recorded_bounds);
  no_longer_exposed_region.Subtract(new_recorded_bounds);
  invalidation.Union(no_longer_exposed_region);
}

void RecordingSource::FinishDisplayItemListUpdate() {
  TRACE_EVENT0("cc", "RecordingSource::FinishDisplayItemListUpdate");
  DetermineIfSolidColor();
  display_list_->EmitTraceSnapshot();

  directly_composited_image_info_ =
      display_list_->GetDirectlyCompositedImageInfo();
  if (directly_composited_image_info_) {
    // Directly composited images are not guaranteed to fully cover every
    // pixel in the layer due to ceiling when calculating the tile content
    // rect from the layer bounds.
    SetRequiresClear(true);
  }
}

void RecordingSource::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  if (!layer_rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    invalidation_.Union(gfx::IntersectRects(layer_rect, gfx::Rect(size_)));
  }
}

bool RecordingSource::Update(const gfx::Size& layer_size,
                             float recording_scale_factor,
                             ContentLayerClient& client,
                             Region& invalidation) {
  invalidation_.Swap(&invalidation);
  invalidation_.Clear();

  if (size_ == layer_size && invalidation.IsEmpty()) {
    return false;
  }

  size_ = layer_size;
  recording_scale_factor_ = recording_scale_factor;

  scoped_refptr<DisplayItemList> display_list =
      client.PaintContentsToDisplayList();
  if (display_list_ == display_list) {
    return true;
  }

  // Do the following only if the display list changes. Though we use
  // recording_scale_factor in DetermineIfSolidColor(), change of it doesn't
  // affect whether the same display list is solid or not.

  gfx::Rect layer_rect(size_);
  gfx::Rect new_recorded_bounds = layer_rect;
  if (can_use_recorded_bounds_) {
    if (std::optional<gfx::Rect> display_list_bounds = display_list->bounds()) {
      new_recorded_bounds.Intersect(*display_list_bounds);
    }
  }

  if (display_list_ &&
      display_list->NeedsAdditionalInvalidationForLCDText(*display_list_)) {
    invalidation = layer_rect;
  } else if (new_recorded_bounds != recorded_bounds_) {
    UpdateInvalidationForRecordedBounds(recorded_bounds_, new_recorded_bounds,
                                        invalidation);
  }

  recorded_bounds_ = new_recorded_bounds;
  display_list_ = std::move(display_list);
  FinishDisplayItemListUpdate();
  return true;
}

void RecordingSource::SetEmptyBounds() {
  size_ = gfx::Size();
  is_solid_color_ = false;

  recorded_bounds_ = gfx::Rect();
  display_list_ = nullptr;
}

void RecordingSource::SetSlowdownRasterScaleFactor(int factor) {
  slow_down_raster_scale_factor_for_debug_ = factor;
}

void RecordingSource::SetBackgroundColor(SkColor4f background_color) {
  background_color_ = background_color;
}

void RecordingSource::SetRequiresClear(bool requires_clear) {
  requires_clear_ = requires_clear;
}

void RecordingSource::SetCanUseRecordedBounds(bool can_use_recorded_bounds) {
  if (can_use_recorded_bounds_ == can_use_recorded_bounds) {
    return;
  }
  can_use_recorded_bounds_ = can_use_recorded_bounds;
  // To force update.
  size_ = gfx::Size();
}

scoped_refptr<RasterSource> RecordingSource::CreateRasterSource() const {
  return base::WrapRefCounted(new RasterSource(*this));
}

void RecordingSource::DetermineIfSolidColor() {
  DCHECK(display_list_);
  is_solid_color_ = false;
  solid_color_ = SkColors::kTransparent;

  if (display_list_->TotalOpCount() > kMaxOpsToAnalyzeForLayer)
    return;

  // TODO(crbug.com/41490692): Allow the solid color not to fill the layer.
  TRACE_EVENT1("cc", "RecordingSource::DetermineIfSolidColor", "opcount",
               display_list_->TotalOpCount());
  is_solid_color_ = display_list_->GetColorIfSolidInRect(
      gfx::ScaleToRoundedRect(gfx::Rect(size_), recording_scale_factor_),
      &solid_color_, kMaxOpsToAnalyzeForLayer);
}

}  // namespace cc
