// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_source.h"

#include <stddef.h>
#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/paint/clear_for_opaque_raster.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

RasterSource::RasterSource(const RecordingSource& other)
    : display_list_(other.display_list_),
      background_color_(other.background_color_),
      requires_clear_(other.requires_clear_),
      is_solid_color_(other.is_solid_color_),
      solid_color_(other.solid_color_),
      recorded_bounds_(other.recorded_bounds_),
      size_(other.size_),
      slow_down_raster_scale_factor_for_debug_(
          other.slow_down_raster_scale_factor_for_debug_),
      recording_scale_factor_(other.recording_scale_factor_),
      directly_composited_image_info_(other.directly_composited_image_info_) {
  DCHECK(recorded_bounds_.IsEmpty() ||
         gfx::Rect(size_).Contains(recorded_bounds_));
}

RasterSource::~RasterSource() = default;

void RasterSource::ClearForOpaqueRaster(
    SkCanvas* raster_canvas,
    const gfx::AxisTransform2d& raster_transform,
    const gfx::Size& content_size,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect) const {
  gfx::Rect outer_rect;
  gfx::Rect inner_rect;
  gfx::Vector2dF scale = raster_transform.scale();
  scale.InvScale(recording_scale_factor_);
  if (!CalculateClearForOpaqueRasterRects(
          raster_transform.translation(), scale, content_size,
          canvas_bitmap_rect, canvas_playback_rect, outer_rect, inner_rect))
    return;

  raster_canvas->save();
  raster_canvas->clipRect(gfx::RectToSkRect(outer_rect), SkClipOp::kIntersect,
                          false);
  if (!inner_rect.IsEmpty()) {
    raster_canvas->clipRect(gfx::RectToSkRect(inner_rect),
                            SkClipOp::kDifference, false);
  }
  raster_canvas->drawColor(background_color_, SkBlendMode::kSrc);
  raster_canvas->restore();
}

void RasterSource::PlaybackToCanvas(
    SkCanvas* raster_canvas,
    const gfx::Size& content_size,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    const gfx::AxisTransform2d& raster_transform,
    const PlaybackSettings& settings) const {
  SkIRect raster_bounds = gfx::RectToSkIRect(canvas_bitmap_rect);
  if (!canvas_playback_rect.IsEmpty() &&
      !raster_bounds.intersect(gfx::RectToSkIRect(canvas_playback_rect)))
    return;
  // Treat all subnormal values as zero for performance.
  ScopedSubnormalFloatDisabler disabler;

  // NOTE: The following code should be kept consistent with
  // PaintOpBufferSerializer::SerializePreamble().
  bool is_partial_raster = canvas_bitmap_rect != canvas_playback_rect;
  if (!requires_clear_) {
    // Clear opaque raster sources.  Opaque rasters sources guarantee that all
    // pixels inside the opaque region are painted.  However, due to raster
    // scaling or translation it's possible that the edges of the painted
    // content might include texels that are not painted opaquely.  Because this
    // raster source is required to be opaque, we may need to do extra clearing
    // outside of the clip.  This needs to be done for both full and partial
    // raster.
    ClearForOpaqueRaster(raster_canvas, raster_transform, content_size,
                         canvas_bitmap_rect, canvas_playback_rect);
  } else if (!is_partial_raster) {
    // For non-opaque raster sources that are rastering the full tile,
    // just clear the entire canvas (even if stretches past the canvas
    // bitmap rect) as it's cheap to do so.
    raster_canvas->clear(SK_ColorTRANSPARENT);
  }

  raster_canvas->save();
  raster_canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
  raster_canvas->clipRect(SkRect::Make(raster_bounds));
  raster_canvas->translate(raster_transform.translation().x(),
                           raster_transform.translation().y());
  raster_canvas->scale(raster_transform.scale().x() / recording_scale_factor_,
                       raster_transform.scale().y() / recording_scale_factor_);

  if (is_partial_raster && requires_clear_) {
    // Because Skia treats painted regions as transparent by default, we don't
    // need to clear outside of the playback rect in the same way that
    // ClearForOpaqueRaster must handle.
    raster_canvas->clear(SK_ColorTRANSPARENT);
  }

  PlaybackDisplayListToCanvas(raster_canvas, settings);
  raster_canvas->restore();
}

void RasterSource::PlaybackDisplayListToCanvas(
    SkCanvas* raster_canvas,
    const PlaybackSettings& settings) const {
  // TODO(enne): Temporary CHECK debugging for http://crbug.com/823835
  CHECK(display_list_.get());
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  PlaybackParams params(settings.image_provider, SkM44());
  params.raster_inducing_scroll_offsets =
      settings.raster_inducing_scroll_offsets;
  for (int i = 0; i < repeat_count; ++i) {
    display_list_->Raster(raster_canvas, params);
  }
}

bool RasterSource::PerformSolidColorAnalysis(gfx::Rect layer_rect,
                                             SkColor4f* color,
                                             int max_ops_to_analyze) const {
  TRACE_EVENT0("cc", "RasterSource::PerformSolidColorAnalysis");

  layer_rect.Intersect(gfx::Rect(size_));
  layer_rect = gfx::ScaleToRoundedRect(layer_rect, recording_scale_factor_);
  return display_list_->GetColorIfSolidInRect(layer_rect, color,
                                              max_ops_to_analyze);
}

bool RasterSource::IntersectsRect(const gfx::Rect& layer_rect) const {
  return recorded_bounds().Intersects(layer_rect);
}

gfx::Size RasterSource::GetContentSize(
    const gfx::Vector2dF& content_scale) const {
  return gfx::ScaleToCeiledSize(size_, content_scale.x(), content_scale.y());
}

bool RasterSource::IsSolidColor() const {
  return is_solid_color_;
}

SkColor4f RasterSource::GetSolidColor() const {
  DCHECK(IsSolidColor());
  return solid_color_;
}

bool RasterSource::HasRecordings() const {
  return display_list_ && !recorded_bounds_.IsEmpty();
}

void RasterSource::AsValueInto(base::trace_event::TracedValue* array) const {
  if (display_list_.get())
    viz::TracedValue::AppendIDRef(display_list_.get(), array);
}

void RasterSource::DidBeginTracing() {
  if (display_list_.get())
    display_list_->EmitTraceSnapshot();
}

RasterSource::PlaybackSettings::PlaybackSettings() = default;

RasterSource::PlaybackSettings::PlaybackSettings(const PlaybackSettings&) =
    default;

RasterSource::PlaybackSettings::PlaybackSettings(PlaybackSettings&&) = default;

RasterSource::PlaybackSettings::~PlaybackSettings() = default;

}  // namespace cc
