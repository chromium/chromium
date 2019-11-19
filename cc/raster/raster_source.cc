// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_source.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {
namespace {

// These enum values are persisted to logs and must never by renumbered or
// reused.
enum class RasterSourceClearType {
  kNone = 0,
  kFull = 1,
  kBorder = 2,
  kCount = 3
};

void TrackRasterSourceNeededClear(RasterSourceClearType clear_type) {
  UMA_HISTOGRAM_ENUMERATION("Renderer4.RasterSourceClearType", clear_type,
                            RasterSourceClearType::kCount);
}

}  // namespace

RasterSource::RasterSource(const RecordingSource* other)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      recording_scale_factor_(other->recording_scale_factor_) {}
RasterSource::~RasterSource() = default;

void RasterSource::ClearForOpaqueRaster(
    SkCanvas* raster_canvas,
    const gfx::Size& content_size,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect) const {
  // The last texel of this content is not guaranteed to be fully opaque, so
  // inset by one to generate the fully opaque coverage rect.  This rect is
  // in device space.
  SkIRect coverage_device_rect =
      SkIRect::MakeWH(content_size.width() - canvas_bitmap_rect.x() - 1,
                      content_size.height() - canvas_bitmap_rect.y() - 1);

  // If not fully covered, we need to clear one texel inside the coverage
  // rect (because of blending during raster) and one texel outside the canvas
  // bitmap rect (because of bilinear filtering during draw).  See comments
  // in RasterSource.
  SkIRect device_column = SkIRect::MakeXYWH(coverage_device_rect.right(), 0, 2,
                                            coverage_device_rect.bottom());
  // row includes the corner, column excludes it.
  SkIRect device_row = SkIRect::MakeXYWH(0, coverage_device_rect.bottom(),
                                         coverage_device_rect.right() + 2, 2);

  bool right_edge = content_size.width() == canvas_playback_rect.right();
  bool bottom_edge = content_size.height() == canvas_playback_rect.bottom();

  // If the playback rect is touching either edge of the content rect
  // extend it by one pixel to include the extra texel outside the canvas
  // bitmap rect that was added to device column and row above.
  SkIRect playback_device_rect =
      SkIRect::MakeXYWH(canvas_playback_rect.x() - canvas_bitmap_rect.x(),
                        canvas_playback_rect.y() - canvas_bitmap_rect.y(),
                        canvas_playback_rect.width() + (right_edge ? 1 : 0),
                        canvas_playback_rect.height() + (bottom_edge ? 1 : 0));

  // Intersect the device column and row with the playback rect and only
  // clear inside of that rect if needed.
  RasterSourceClearType clear_type = RasterSourceClearType::kNone;
  if (device_column.intersect(playback_device_rect)) {
    clear_type = RasterSourceClearType::kBorder;
    raster_canvas->save();
    raster_canvas->clipRect(SkRect::Make(device_column), SkClipOp::kIntersect,
                            false);
    raster_canvas->drawColor(background_color_, SkBlendMode::kSrc);
    raster_canvas->restore();
  }
  if (device_row.intersect(playback_device_rect)) {
    clear_type = RasterSourceClearType::kBorder;
    raster_canvas->save();
    raster_canvas->clipRect(SkRect::Make(device_row), SkClipOp::kIntersect,
                            false);
    raster_canvas->drawColor(background_color_, SkBlendMode::kSrc);
    raster_canvas->restore();
  }
  TrackRasterSourceNeededClear(clear_type);
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

  bool is_partial_raster = canvas_bitmap_rect != canvas_playback_rect;
  if (!requires_clear_) {
    // Clear opaque raster sources.  Opaque rasters sources guarantee that all
    // pixels inside the opaque region are painted.  However, due to scaling
    // it's possible that the last row and column might include pixels that
    // are not painted.  Because this raster source is required to be opaque,
    // we may need to do extra clearing outside of the clip.  This needs to
    // be done for both full and partial raster.
    ClearForOpaqueRaster(raster_canvas, content_size, canvas_bitmap_rect,
                         canvas_playback_rect);
  } else if (!is_partial_raster) {
    // For non-opaque raster sources that are rastering the full tile,
    // just clear the entire canvas (even if stretches past the canvas
    // bitmap rect) as it's cheap to do so.
    TrackRasterSourceNeededClear(RasterSourceClearType::kFull);
    raster_canvas->clear(SK_ColorTRANSPARENT);
  }

  raster_canvas->save();
  raster_canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
  raster_canvas->clipRect(SkRect::Make(raster_bounds));
  raster_canvas->translate(raster_transform.translation().x(),
                           raster_transform.translation().y());
  raster_canvas->scale(raster_transform.scale() / recording_scale_factor_,
                       raster_transform.scale() / recording_scale_factor_);

  if (is_partial_raster && requires_clear_) {
    // TODO(enne): Should this be considered a partial clear?
    TrackRasterSourceNeededClear(RasterSourceClearType::kFull);
    // Because Skia treats painted regions as transparent by default, we don't
    // need to clear outside of the playback rect in the same way that
    // ClearForOpaqueRaster must handle.
    raster_canvas->clear(SK_ColorTRANSPARENT);
  }

  PlaybackToCanvas(raster_canvas, settings.image_provider);
  raster_canvas->restore();
}

void RasterSource::PlaybackToCanvas(SkCanvas* raster_canvas,
                                    ImageProvider* image_provider) const {
  // TODO(enne): Temporary CHECK debugging for http://crbug.com/823835
  CHECK(display_list_.get());
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  for (int i = 0; i < repeat_count; ++i)
    display_list_->Raster(raster_canvas, image_provider);
}

sk_sp<SkPicture> RasterSource::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "RasterSource::GetFlattenedPicture");

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(size_.width(), size_.height());
  if (!size_.IsEmpty()) {
    canvas->clear(SK_ColorTRANSPARENT);
    PlaybackToCanvas(canvas, nullptr);
  }

  return recorder.finishRecordingAsPicture();
}

size_t RasterSource::GetMemoryUsage() const {
  if (!display_list_)
    return 0;
  return display_list_->BytesUsed() + painter_reported_memory_usage_;
}

bool RasterSource::PerformSolidColorAnalysis(gfx::Rect layer_rect,
                                             SkColor* color) const {
  TRACE_EVENT0("cc", "RasterSource::PerformSolidColorAnalysis");

  layer_rect.Intersect(gfx::Rect(size_));
  layer_rect = gfx::ScaleToRoundedRect(layer_rect, recording_scale_factor_);
  return display_list_->GetColorIfSolidInRect(layer_rect, color);
}

void RasterSource::GetDiscardableImagesInRect(
    const gfx::Rect& layer_rect,
    std::vector<const DrawImage*>* images) const {
  DCHECK_EQ(0u, images->size());
  display_list_->discardable_image_map().GetDiscardableImagesInRect(layer_rect,
                                                                    images);
}

base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
RasterSource::TakeDecodingModeMap() {
  return display_list_->TakeDecodingModeMap();
}

bool RasterSource::CoversRect(const gfx::Rect& layer_rect) const {
  if (size_.IsEmpty())
    return false;
  gfx::Rect bounded_rect = layer_rect;
  bounded_rect.Intersect(gfx::Rect(size_));
  return recorded_viewport_.Contains(bounded_rect);
}

gfx::Size RasterSource::GetSize() const {
  return size_;
}

gfx::Size RasterSource::GetContentSize(float content_scale) const {
  return gfx::ScaleToCeiledSize(GetSize(), content_scale);
}

bool RasterSource::IsSolidColor() const {
  return is_solid_color_;
}

SkColor RasterSource::GetSolidColor() const {
  DCHECK(IsSolidColor());
  return solid_color_;
}

bool RasterSource::HasRecordings() const {
  return !!display_list_.get();
}

gfx::Rect RasterSource::RecordedViewport() const {
  return recorded_viewport_;
}

bool RasterSource::HasText() const {
  return display_list_ && display_list_->HasText();
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
