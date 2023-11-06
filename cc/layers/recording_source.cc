// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/recording_source.h"

#include <stdint.h>

#include <algorithm>

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

void RecordingSource::UpdateInvalidationForNewViewport(
    const gfx::Rect& old_recorded_viewport,
    const gfx::Rect& new_recorded_viewport,
    Region* invalidation) {
  // Invalidate newly-exposed and no-longer-exposed areas.
  Region newly_exposed_region(new_recorded_viewport);
  newly_exposed_region.Subtract(old_recorded_viewport);
  invalidation->Union(newly_exposed_region);

  Region no_longer_exposed_region(old_recorded_viewport);
  no_longer_exposed_region.Subtract(new_recorded_viewport);
  invalidation->Union(no_longer_exposed_region);
}

void RecordingSource::FinishDisplayItemListUpdate() {
  TRACE_EVENT0("cc", "RecordingSource::FinishDisplayItemListUpdate");
  DetermineIfSolidColor();
  display_list_->EmitTraceSnapshot();
}

void RecordingSource::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  if (!layer_rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    invalidation_.Union(gfx::IntersectRects(layer_rect, gfx::Rect(size_)));
  }
}

bool RecordingSource::UpdateAndExpandInvalidation(
    Region* invalidation,
    const gfx::Size& layer_size,
    const gfx::Rect& new_recorded_viewport) {
  bool updated = false;

  if (size_ != layer_size)
    size_ = layer_size;

  invalidation_.Swap(invalidation);
  invalidation_.Clear();

  if (new_recorded_viewport != recorded_viewport_) {
    UpdateInvalidationForNewViewport(recorded_viewport_, new_recorded_viewport,
                                     invalidation);
    recorded_viewport_ = new_recorded_viewport;
    updated = true;
  }

  if (!updated && !invalidation->Intersects(recorded_viewport_))
    return false;

  if (invalidation->IsEmpty())
    return false;

  return true;
}

void RecordingSource::UpdateDisplayItemList(
    const scoped_refptr<DisplayItemList>& display_list,
    float recording_scale_factor) {
  recording_scale_factor_ = recording_scale_factor;

  if (display_list_ != display_list) {
    display_list_ = display_list;
    // Do the following only if the display list changes. Though we use
    // recording_scale_factor in DetermineIfSolidColor(), change of it doesn't
    // affect whether the same display list is solid or not.
    FinishDisplayItemListUpdate();
  }
}

gfx::Size RecordingSource::GetSize() const {
  return size_;
}

void RecordingSource::SetEmptyBounds() {
  size_ = gfx::Size();
  is_solid_color_ = false;

  recorded_viewport_ = gfx::Rect();
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

scoped_refptr<RasterSource> RecordingSource::CreateRasterSource() const {
  return scoped_refptr<RasterSource>(new RasterSource(this));
}

void RecordingSource::DetermineIfSolidColor() {
  DCHECK(display_list_);
  is_solid_color_ = false;
  solid_color_ = SkColors::kTransparent;

  if (display_list_->TotalOpCount() > kMaxOpsToAnalyzeForLayer)
    return;

  TRACE_EVENT1("cc", "RecordingSource::DetermineIfSolidColor", "opcount",
               display_list_->TotalOpCount());
  is_solid_color_ = display_list_->GetColorIfSolidInRect(
      gfx::ScaleToRoundedRect(gfx::Rect(GetSize()), recording_scale_factor_),
      &solid_color_, kMaxOpsToAnalyzeForLayer);
}

}  // namespace cc
