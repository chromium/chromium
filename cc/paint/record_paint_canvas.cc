// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/record_paint_canvas.h"

#include <utility>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace cc {

RecordPaintCanvas::RecordPaintCanvas(DisplayItemList* list,
                                     const SkRect& bounds)
    : list_(list), recording_bounds_(bounds) {
  DCHECK(list_);
}

RecordPaintCanvas::~RecordPaintCanvas() = default;

SkImageInfo RecordPaintCanvas::imageInfo() const {
  return GetCanvas()->imageInfo();
}

template <typename T, typename... Args>
size_t RecordPaintCanvas::push(Args&&... args) {
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
  return list_->push<T>(std::forward<Args>(args)...);
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
  // Note: The value of needs_flush_ never gets reset. That is because
  // flushing a recording implies closing this RecordPaintCanvas and starting a
  // new one.
  needs_flush_ = true;
}

bool RecordPaintCanvas::NeedsFlush() const {
  return needs_flush_;
}

int RecordPaintCanvas::save() {
  push<SaveOp>();
  return GetCanvas()->save();
}

int RecordPaintCanvas::saveLayer(const SkRect* bounds,
                                 const PaintFlags* flags) {
  if (flags) {
    if (flags->IsSimpleOpacity()) {
      // TODO(enne): maybe more callers should know this and call
      // saveLayerAlpha instead of needing to check here.
      uint8_t alpha = SkColorGetA(flags->getColor());
      return saveLayerAlpha(bounds, alpha);
    }

    // TODO(enne): it appears that image filters affect matrices and color
    // matrices affect transparent flags on SkCanvas layers, but it's not clear
    // whether those are actually needed and we could just skip ToSkPaint here.
    push<SaveLayerOp>(bounds, flags);
    SkPaint paint = flags->ToSkPaint();
    return GetCanvas()->saveLayer(bounds, &paint);
  }
  push<SaveLayerOp>(bounds, flags);
  return GetCanvas()->saveLayer(bounds, nullptr);
}

int RecordPaintCanvas::saveLayerAlpha(const SkRect* bounds, uint8_t alpha) {
  push<SaveLayerAlphaOp>(bounds, alpha);
  return GetCanvas()->saveLayerAlpha(bounds, alpha);
}

void RecordPaintCanvas::restore() {
  push<RestoreOp>();
  GetCanvas()->restore();
}

int RecordPaintCanvas::getSaveCount() const {
  return GetCanvas()->getSaveCount();
}

void RecordPaintCanvas::restoreToCount(int save_count) {
  if (!canvas_) {
    DCHECK_EQ(save_count, 1);
    return;
  }

  DCHECK_GE(save_count, 1);
  int diff = GetCanvas()->getSaveCount() - save_count;
  DCHECK_GE(diff, 0);
  for (int i = 0; i < diff; ++i)
    restore();
}

void RecordPaintCanvas::translate(SkScalar dx, SkScalar dy) {
  push<TranslateOp>(dx, dy);
  GetCanvas()->translate(dx, dy);
}

void RecordPaintCanvas::scale(SkScalar sx, SkScalar sy) {
  push<ScaleOp>(sx, sy);
  GetCanvas()->scale(sx, sy);
}

void RecordPaintCanvas::rotate(SkScalar degrees) {
  push<RotateOp>(degrees);
  GetCanvas()->rotate(degrees);
}

void RecordPaintCanvas::concat(const SkMatrix& matrix) {
  SkM44 m = SkM44(matrix);
  push<ConcatOp>(m);
  GetCanvas()->concat(m);
}

void RecordPaintCanvas::concat(const SkM44& matrix) {
  push<ConcatOp>(matrix);
  GetCanvas()->concat(matrix);
}

void RecordPaintCanvas::setMatrix(const SkMatrix& matrix) {
  SkM44 m = SkM44(matrix);
  push<SetMatrixOp>(m);
  GetCanvas()->setMatrix(m);
}

void RecordPaintCanvas::setMatrix(const SkM44& matrix) {
  push<SetMatrixOp>(matrix);
  GetCanvas()->setMatrix(matrix);
}

void RecordPaintCanvas::clipRect(const SkRect& rect,
                                 SkClipOp op,
                                 bool antialias) {
  push<ClipRectOp>(rect, op, antialias);
  GetCanvas()->clipRect(rect, op, antialias);
}

void RecordPaintCanvas::clipRRect(const SkRRect& rrect,
                                  SkClipOp op,
                                  bool antialias) {
  // TODO(enne): does this happen? Should the caller know this?
  if (rrect.isRect()) {
    clipRect(rrect.getBounds(), op, antialias);
    return;
  }
  push<ClipRRectOp>(rrect, op, antialias);
  GetCanvas()->clipRRect(rrect, op, antialias);
}

void RecordPaintCanvas::clipPath(const SkPath& path,
                                 SkClipOp op,
                                 bool antialias,
                                 UsePaintCache use_paint_cache) {
  if (!path.isInverseFillType() &&
      GetCanvas()->getTotalMatrix().rectStaysRect()) {
    // TODO(enne): do these cases happen? should the caller know that this isn't
    // a path?
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

  push<ClipPathOp>(path, op, antialias, use_paint_cache);
  GetCanvas()->clipPath(path, op, antialias);
  return;
}

SkRect RecordPaintCanvas::getLocalClipBounds() const {
  DCHECK(InitializedWithRecordingBounds());
  return GetCanvas()->getLocalClipBounds();
}

bool RecordPaintCanvas::getLocalClipBounds(SkRect* bounds) const {
  DCHECK(InitializedWithRecordingBounds());
  return GetCanvas()->getLocalClipBounds(bounds);
}

SkIRect RecordPaintCanvas::getDeviceClipBounds() const {
  DCHECK(InitializedWithRecordingBounds());
  return GetCanvas()->getDeviceClipBounds();
}

bool RecordPaintCanvas::getDeviceClipBounds(SkIRect* bounds) const {
  DCHECK(InitializedWithRecordingBounds());
  return GetCanvas()->getDeviceClipBounds(bounds);
}

void RecordPaintCanvas::drawColor(SkColor color, SkBlendMode mode) {
  push<DrawColorOp>(color, mode);
}

void RecordPaintCanvas::clear(SkColor color) {
  push<DrawColorOp>(color, SkBlendMode::kSrc);
}

void RecordPaintCanvas::drawLine(SkScalar x0,
                                 SkScalar y0,
                                 SkScalar x1,
                                 SkScalar y1,
                                 const PaintFlags& flags) {
  push<DrawLineOp>(x0, y0, x1, y1, flags);
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

void RecordPaintCanvas::drawPicture(sk_sp<const PaintRecord> record) {
  // TODO(enne): If this is small, maybe flatten it?
  push<DrawRecordOp>(record);
}

bool RecordPaintCanvas::isClipEmpty() const {
  DCHECK(InitializedWithRecordingBounds());
  return GetCanvas()->isClipEmpty();
}

SkMatrix RecordPaintCanvas::getTotalMatrix() const {
  return GetCanvas()->getTotalMatrix();
}

SkM44 RecordPaintCanvas::getLocalToDevice() const {
  return GetCanvas()->getLocalToDevice();
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

const SkNoDrawCanvas* RecordPaintCanvas::GetCanvas() const {
  return const_cast<RecordPaintCanvas*>(this)->GetCanvas();
}

SkNoDrawCanvas* RecordPaintCanvas::GetCanvas() {
  if (canvas_)
    return &*canvas_;

  // Size the canvas to be large enough to contain the |recording_bounds|, which
  // may not be positioned at the origin.
  SkIRect enclosing_rect = recording_bounds_.roundOut();
  canvas_.emplace(enclosing_rect.right(), enclosing_rect.bottom());

  // This is part of the "recording canvases have a size, but why" dance.
  // By creating a canvas of size (right x bottom) and then clipping it,
  // It makes getDeviceClipBounds return the original cull rect, which code
  // in GraphicsContextCanvas on Mac expects.  (Just creating an SkNoDrawCanvas
  // with the recording_bounds_ makes a canvas of size (width x height) instead
  // which is incorrect.  SkRecorder cheats with private resetForNextCanvas.
  canvas_->clipRect(recording_bounds_, SkClipOp::kIntersect, false);
  return &*canvas_;
}

bool RecordPaintCanvas::InitializedWithRecordingBounds() const {
  // If the RecordPaintCanvas is initialized with an empty bounds then
  // the various clip related functions are not valid and should not
  // be called.
  return !recording_bounds_.isEmpty();
}

}  // namespace cc
