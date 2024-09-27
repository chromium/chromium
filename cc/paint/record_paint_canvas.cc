// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/record_paint_canvas.h"

#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace cc {

RecordPaintCanvas::RecordPaintCanvas() = default;
RecordPaintCanvas::~RecordPaintCanvas() = default;

PaintRecord RecordPaintCanvas::ReleaseAsRecord() {
  // Some users expect that their saves are automatically closed for them.
  // Maybe we could remove this assumption and just have callers do it.
  restoreToCount(1);
  needs_flush_ = false;
  return buffer_.ReleaseAsRecord();
}

void RecordPaintCanvas::DisableLineDrawingAsPaths() {
  maybe_draw_lines_as_paths_ = false;
  draw_path_count_ = draw_line_count_ = 0;
}

PaintRecord RecordPaintCanvas::CopyAsRecord() {
  needs_flush_ = false;
  return buffer_.DeepCopyAsRecord();
}

template <typename T, typename... Args>
void RecordPaintCanvas::push(Args&&... args) {
#if DCHECK_IS_ON()
  // The following check fails if client code does not check and handle
  // NeedsFlush() before issuing draw calls.
  // Note: restore ops are tolerated when flushes are requested since they are
  // often necessary in order to bring the canvas to a flushable state.
  // SetNodeId ops are also tolerated because they may be inserted just before
  // flushing.
  DCHECK(disable_flush_check_scope_ || !needs_flush_ ||
         (std::is_same<T, RestoreOp>::value) ||
         (std::is_same<T, SetNodeIdOp>::value));
#endif
  buffer_.push<T>(std::forward<Args>(args)...);
}

void* RecordPaintCanvas::accessTopLayerPixels(SkImageInfo* info,
                                              size_t* rowBytes,
                                              SkIPoint* origin) {
  // Modifications to the underlying pixels cannot be saved.
  return nullptr;
}

void RecordPaintCanvas::flush() {
  // RecordPaintCanvas is unable to flush its own recording into the graphics
  // pipeline. So instead we make note of the flush request so that it can be
  // handled by code that owns the recording.
  //
  // Note: The value of needs_flush_ never gets reset until the end of
  // recording. That is because flushing a recording implies ReleaseAsRecord()
  // and starting a new recording.
  needs_flush_ = true;
}

bool RecordPaintCanvas::NeedsFlush() const {
  return needs_flush_;
}

int RecordPaintCanvas::save() {
  push<SaveOp>();
  return save_count_++;
}

int RecordPaintCanvas::saveLayer(const PaintFlags& flags) {
  push<SaveLayerOp>(flags);
  return save_count_++;
}

int RecordPaintCanvas::saveLayer(const SkRect& bounds,
                                 const PaintFlags& flags) {
  push<SaveLayerOp>(bounds, flags);
  return save_count_++;
}

int RecordPaintCanvas::saveLayerAlphaf(float alpha) {
  push<SaveLayerAlphaOp>(alpha);
  return save_count_++;
}

int RecordPaintCanvas::saveLayerAlphaf(const SkRect& bounds, float alpha) {
  push<SaveLayerAlphaOp>(bounds, alpha);
  return save_count_++;
}

int RecordPaintCanvas::saveLayerFilters(base::span<sk_sp<PaintFilter>> filters,
                                        const PaintFlags& flags) {
  push<SaveLayerFiltersOp>(filters, flags);
  return save_count_++;
}

void RecordPaintCanvas::restore() {
  push<RestoreOp>();
  --save_count_;
  DCHECK_GE(save_count_, 1);
}

int RecordPaintCanvas::getSaveCount() const {
  return save_count_;
}

void RecordPaintCanvas::restoreToCount(int save_count) {
  DCHECK_GE(save_count, 1);
  int diff = getSaveCount() - save_count;
  DCHECK_GE(diff, 0);
  for (int i = 0; i < diff; ++i)
    restore();
}

void RecordPaintCanvas::translate(SkScalar dx, SkScalar dy) {
  push<TranslateOp>(dx, dy);
}

void RecordPaintCanvas::scale(SkScalar sx, SkScalar sy) {
  push<ScaleOp>(sx, sy);
}

void RecordPaintCanvas::rotate(SkScalar degrees) {
  push<RotateOp>(degrees);
}

void RecordPaintCanvas::concat(const SkM44& matrix) {
  push<ConcatOp>(matrix);
}

void RecordPaintCanvas::setMatrix(const SkM44& matrix) {
  push<SetMatrixOp>(matrix);
}

void RecordPaintCanvas::clipRect(const SkRect& rect,
                                 SkClipOp op,
                                 bool antialias) {
  push<ClipRectOp>(rect, op, antialias);
}

void RecordPaintCanvas::clipRRect(const SkRRect& rrect,
                                  SkClipOp op,
                                  bool antialias) {
  if (rrect.isRect()) {
    clipRect(rrect.getBounds(), op, antialias);
    return;
  }
  clipRRectInternal(rrect, op, antialias);
}

void RecordPaintCanvas::clipRRectInternal(const SkRRect& rrect,
                                          SkClipOp op,
                                          bool antialias) {
  push<ClipRRectOp>(rrect, op, antialias);
}

void RecordPaintCanvas::clipPath(const SkPath& path,
                                 SkClipOp op,
                                 bool antialias,
                                 UsePaintCache use_paint_cache) {
  if (!path.isInverseFillType()) {
    SkRect rect;
    if (path.isRect(&rect)) {
      clipRect(rect, op, antialias);
      return;
    }
    SkRRect rrect;
    if (path.isOval(&rect)) {
      rrect.setOval(rect);
      clipRRect(rrect, op, antialias);
      return;
    }
    if (path.isRRect(&rrect)) {
      clipRRect(rrect, op, antialias);
      return;
    }
  }
  clipPathInternal(path, op, antialias, use_paint_cache);
}

void RecordPaintCanvas::clipPathInternal(const SkPath& path,
                                         SkClipOp op,
                                         bool antialias,
                                         UsePaintCache use_paint_cache) {
  push<ClipPathOp>(path, op, antialias, use_paint_cache);
}

SkImageInfo RecordPaintCanvas::imageInfo() const {
  NOTREACHED();
}

bool RecordPaintCanvas::getLocalClipBounds(SkRect* bounds) const {
  NOTREACHED();
}

bool RecordPaintCanvas::getDeviceClipBounds(SkIRect* bounds) const {
  NOTREACHED();
}

SkM44 RecordPaintCanvas::getLocalToDevice() const {
  NOTREACHED();
}

void RecordPaintCanvas::drawColor(SkColor4f color, SkBlendMode mode) {
  push<DrawColorOp>(color, mode);
}

void RecordPaintCanvas::clear(SkColor4f color) {
  push<DrawColorOp>(color, SkBlendMode::kSrc);
}

void RecordPaintCanvas::drawLine(SkScalar x0,
                                 SkScalar y0,
                                 SkScalar x1,
                                 SkScalar y1,
                                 const PaintFlags& flags) {
  if (maybe_draw_lines_as_paths_ &&
      draw_line_count_ != std::numeric_limits<uint32_t>::max()) {
    ++draw_line_count_;
    // If a bunch of paths have been drawn, only switch to drawing lines
    // after a number of lines have been drawn.
    if (draw_line_count_ > 4) {
      draw_path_count_ = 0;
    }
  }
  // TODO(crbug.com/5524058): investigate if it makes sense to add support for
  // draw_path_count > 4 to the lite op.
  if (draw_path_count_ <= 4 && AreLiteOpsEnabled() &&
      flags.CanConvertToCorePaintFlags()) {
    push<DrawLineLiteOp>(x0, y0, x1, y1, flags.ToCorePaintFlags());
    return;
  }
  // Render lines as paths if there have been a number of drawPaths() recently.
  // See description in header for more details.
  push<DrawLineOp>(x0, y0, x1, y1, flags, draw_path_count_ > 4);
}

void RecordPaintCanvas::drawArc(const SkRect& oval,
                                SkScalar start_angle_degrees,
                                SkScalar sweep_angle_degrees,
                                const PaintFlags& flags) {
  if (AreLiteOpsEnabled() && flags.CanConvertToCorePaintFlags()) {
    push<DrawArcLiteOp>(oval, start_angle_degrees, sweep_angle_degrees,
                        flags.ToCorePaintFlags());
    return;
  }
  push<DrawArcOp>(oval, start_angle_degrees, sweep_angle_degrees, flags);
}

void RecordPaintCanvas::drawRect(const SkRect& rect, const PaintFlags& flags) {
  push<DrawRectOp>(rect, flags);
}

void RecordPaintCanvas::drawIRect(const SkIRect& rect,
                                  const PaintFlags& flags) {
  push<DrawIRectOp>(rect, flags);
}

void RecordPaintCanvas::drawOval(const SkRect& oval, const PaintFlags& flags) {
  push<DrawOvalOp>(oval, flags);
}

void RecordPaintCanvas::drawRRect(const SkRRect& rrect,
                                  const PaintFlags& flags) {
  push<DrawRRectOp>(rrect, flags);
}

void RecordPaintCanvas::drawDRRect(const SkRRect& outer,
                                   const SkRRect& inner,
                                   const PaintFlags& flags) {
  if (outer.isEmpty())
    return;
  if (inner.isEmpty()) {
    drawRRect(outer, flags);
    return;
  }
  push<DrawDRRectOp>(outer, inner, flags);
}

void RecordPaintCanvas::drawRoundRect(const SkRect& rect,
                                      SkScalar rx,
                                      SkScalar ry,
                                      const PaintFlags& flags) {
  // TODO(enne): move this into base class?
  if (rx > 0 && ry > 0) {
    SkRRect rrect;
    rrect.setRectXY(rect, rx, ry);
    drawRRect(rrect, flags);
  } else {
    drawRect(rect, flags);
  }
}

void RecordPaintCanvas::drawPath(const SkPath& path,
                                 const PaintFlags& flags,
                                 UsePaintCache use_paint_cache) {
  if (maybe_draw_lines_as_paths_ &&
      draw_path_count_ != std::numeric_limits<uint32_t>::max()) {
    ++draw_path_count_;
    if (draw_path_count_ > 4) {
      draw_line_count_ = 0;
    }
  }
  push<DrawPathOp>(path, flags, use_paint_cache);
}

void RecordPaintCanvas::drawImage(const PaintImage& image,
                                  SkScalar left,
                                  SkScalar top,
                                  const SkSamplingOptions& sampling,
                                  const PaintFlags* flags) {
  DCHECK(!image.IsPaintWorklet());
  push<DrawImageOp>(image, left, top, sampling, flags);
}

void RecordPaintCanvas::drawImageRect(const PaintImage& image,
                                      const SkRect& src,
                                      const SkRect& dst,
                                      const SkSamplingOptions& sampling,
                                      const PaintFlags* flags,
                                      SkCanvas::SrcRectConstraint constraint) {
  push<DrawImageRectOp>(image, src, dst, sampling, flags, constraint);
}

void RecordPaintCanvas::drawVertices(
    scoped_refptr<RefCountedBuffer<SkPoint>> vertices,
    scoped_refptr<RefCountedBuffer<SkPoint>> uvs,
    scoped_refptr<RefCountedBuffer<uint16_t>> indices,
    const PaintFlags& flags) {
  push<DrawVerticesOp>(std::move(vertices), std::move(uvs), std::move(indices),
                       flags);
}

void RecordPaintCanvas::drawSkottie(scoped_refptr<SkottieWrapper> skottie,
                                    const SkRect& dst,
                                    float t,
                                    SkottieFrameDataMap images,
                                    const SkottieColorMap& color_map,
                                    SkottieTextPropertyValueMap text_map) {
  push<DrawSkottieOp>(std::move(skottie), dst, t, std::move(images), color_map,
                      std::move(text_map));
}

void RecordPaintCanvas::drawTextBlob(sk_sp<SkTextBlob> blob,
                                     SkScalar x,
                                     SkScalar y,
                                     const PaintFlags& flags) {
  push<DrawTextBlobOp>(std::move(blob), x, y, flags);
}

void RecordPaintCanvas::drawTextBlob(sk_sp<SkTextBlob> blob,
                                     SkScalar x,
                                     SkScalar y,
                                     NodeId node_id,
                                     const PaintFlags& flags) {
  push<DrawTextBlobOp>(std::move(blob), x, y, node_id, flags);
}

void RecordPaintCanvas::drawPicture(PaintRecord record) {
  // TODO(enne): If this is small, maybe flatten it?
  push<DrawRecordOp>(std::move(record));
}

void RecordPaintCanvas::drawPicture(PaintRecord record, bool local_ctm) {
  // TODO(enne): If this is small, maybe flatten it?
  push<DrawRecordOp>(std::move(record), local_ctm);
}

void RecordPaintCanvas::Annotate(AnnotationType type,
                                 const SkRect& rect,
                                 sk_sp<SkData> data) {
  push<AnnotateOp>(type, rect, data);
}

void RecordPaintCanvas::recordCustomData(uint32_t id) {
  push<CustomDataOp>(id);
}

void RecordPaintCanvas::setNodeId(int node_id) {
  push<SetNodeIdOp>(node_id);
}

InspectableRecordPaintCanvas::InspectableRecordPaintCanvas(
    const gfx::Size& size)
    : canvas_(size.width(), size.height()) {}

InspectableRecordPaintCanvas::InspectableRecordPaintCanvas(
    CreateChildCanvasTag,
    const InspectableRecordPaintCanvas& parent)
    : canvas_(SkIRect::MakeSize(parent.imageInfo().dimensions())) {
  canvas_.setMatrix(parent.canvas_.getLocalToDevice());
}

InspectableRecordPaintCanvas::~InspectableRecordPaintCanvas() = default;

int InspectableRecordPaintCanvas::save() {
  device_clip_bounds_.reset();
  return CheckSaveCount(RecordPaintCanvas::save(), canvas_.save());
}

int InspectableRecordPaintCanvas::saveLayer(const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  device_clip_bounds_.reset();
  return CheckSaveCount(RecordPaintCanvas::saveLayer(flags),
                        canvas_.saveLayer(nullptr, &paint));
}

int InspectableRecordPaintCanvas::saveLayer(const SkRect& bounds,
                                            const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  device_clip_bounds_.reset();
  return CheckSaveCount(RecordPaintCanvas::saveLayer(bounds, flags),
                        canvas_.saveLayer(&bounds, &paint));
}

int InspectableRecordPaintCanvas::saveLayerAlphaf(float alpha) {
  device_clip_bounds_.reset();
  return CheckSaveCount(RecordPaintCanvas::saveLayerAlphaf(alpha),
                        canvas_.saveLayerAlphaf(nullptr, alpha));
}

int InspectableRecordPaintCanvas::saveLayerAlphaf(const SkRect& bounds,
                                                  float alpha) {
  device_clip_bounds_.reset();
  return CheckSaveCount(RecordPaintCanvas::saveLayerAlphaf(bounds, alpha),
                        canvas_.saveLayerAlphaf(&bounds, alpha));
}

int InspectableRecordPaintCanvas::saveLayerFilters(
    base::span<sk_sp<PaintFilter>> filters,
    const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  device_clip_bounds_.reset();
  return CheckSaveCount(RecordPaintCanvas::saveLayerFilters(filters, flags),
                        // Don't bother copying the filter span, filters don't
                        // impact the current clip or CTM.
                        canvas_.saveLayer(/*bounds=*/nullptr, &paint));
}

void InspectableRecordPaintCanvas::restore() {
  RecordPaintCanvas::restore();
  canvas_.restore();
  device_clip_bounds_.reset();
  DCHECK_EQ(getSaveCount(), canvas_.getSaveCount());
}

int InspectableRecordPaintCanvas::CheckSaveCount(int super_prev_save_count,
                                                 int canvas_prev_save_count) {
  DCHECK_EQ(super_prev_save_count, canvas_prev_save_count);
  DCHECK_EQ(getSaveCount(), canvas_.getSaveCount());
  return super_prev_save_count;
}

void InspectableRecordPaintCanvas::translate(SkScalar dx, SkScalar dy) {
  RecordPaintCanvas::translate(dx, dy);
  canvas_.translate(dx, dy);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::scale(SkScalar sx, SkScalar sy) {
  RecordPaintCanvas::scale(sx, sy);
  canvas_.scale(sx, sy);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::rotate(SkScalar degrees) {
  RecordPaintCanvas::rotate(degrees);
  canvas_.rotate(degrees);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::concat(const SkM44& matrix) {
  RecordPaintCanvas::concat(matrix);
  canvas_.concat(matrix);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::setMatrix(const SkM44& matrix) {
  RecordPaintCanvas::setMatrix(matrix);
  canvas_.setMatrix(matrix);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::clipRect(const SkRect& rect,
                                            SkClipOp op,
                                            bool antialias) {
  RecordPaintCanvas::clipRect(rect, op, antialias);
  canvas_.clipRect(rect, op, antialias);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::clipRRectInternal(const SkRRect& rrect,
                                                     SkClipOp op,
                                                     bool antialias) {
  RecordPaintCanvas::clipRRectInternal(rrect, op, antialias);
  canvas_.clipRRect(rrect, op, antialias);
  device_clip_bounds_.reset();
}

void InspectableRecordPaintCanvas::clipPathInternal(
    const SkPath& path,
    SkClipOp op,
    bool antialias,
    UsePaintCache use_paint_cache) {
  RecordPaintCanvas::clipPathInternal(path, op, antialias, use_paint_cache);
  canvas_.clipPath(path, op, antialias);
  device_clip_bounds_.reset();
}

SkImageInfo InspectableRecordPaintCanvas::imageInfo() const {
  return canvas_.imageInfo();
}

bool InspectableRecordPaintCanvas::getLocalClipBounds(SkRect* bounds) const {
  return canvas_.getLocalClipBounds(bounds);
}

bool InspectableRecordPaintCanvas::getDeviceClipBounds(SkIRect* bounds) const {
  if (device_clip_bounds_) {
    *bounds = *device_clip_bounds_;
    return true;
  }
  if (canvas_.getDeviceClipBounds(bounds)) {
    device_clip_bounds_.emplace(*bounds);
    return true;
  }
  return false;
}

SkM44 InspectableRecordPaintCanvas::getLocalToDevice() const {
  return canvas_.getLocalToDevice();
}

}  // namespace cc
