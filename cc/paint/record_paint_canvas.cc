// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/record_paint_canvas.h"

#include <utility>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkAnnotation.h"
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

void* RecordPaintCanvas::accessTopLayerPixels(SkImageInfo* info,
                                              size_t* rowBytes,
                                              SkIPoint* origin) {
  // Modifications to the underlying pixels cannot be saved.
  return nullptr;
}

void RecordPaintCanvas::flush() {
  // This is a noop when recording.
}

int RecordPaintCanvas::save() {
  list_->push<SaveOp>();
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
    list_->push<SaveLayerOp>(bounds, flags);
    SkPaint paint = flags->ToSkPaint();
    return GetCanvas()->saveLayer(bounds, &paint);
  }
  list_->push<SaveLayerOp>(bounds, flags);
  return GetCanvas()->saveLayer(bounds, nullptr);
}

int RecordPaintCanvas::saveLayerAlpha(const SkRect* bounds, uint8_t alpha) {
  list_->push<SaveLayerAlphaOp>(bounds, alpha);
  return GetCanvas()->saveLayerAlpha(bounds, alpha);
}

void RecordPaintCanvas::restore() {
  list_->push<RestoreOp>();
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
  list_->push<TranslateOp>(dx, dy);
  GetCanvas()->translate(dx, dy);
}

void RecordPaintCanvas::scale(SkScalar sx, SkScalar sy) {
  list_->push<ScaleOp>(sx, sy);
  GetCanvas()->scale(sx, sy);
}

void RecordPaintCanvas::rotate(SkScalar degrees) {
  list_->push<RotateOp>(degrees);
  GetCanvas()->rotate(degrees);
}

void RecordPaintCanvas::concat(const SkMatrix& matrix) {
  list_->push<ConcatOp>(matrix);
  GetCanvas()->concat(matrix);
}

void RecordPaintCanvas::setMatrix(const SkMatrix& matrix) {
  list_->push<SetMatrixOp>(matrix);
  GetCanvas()->setMatrix(matrix);
}

void RecordPaintCanvas::clipRect(const SkRect& rect,
                                 SkClipOp op,
                                 bool antialias) {
  list_->push<ClipRectOp>(rect, op, antialias);
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
  list_->push<ClipRRectOp>(rrect, op, antialias);
  GetCanvas()->clipRRect(rrect, op, antialias);
}

void RecordPaintCanvas::clipPath(const SkPath& path,
                                 SkClipOp op,
                                 bool antialias) {
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

  list_->push<ClipPathOp>(path, op, antialias);
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
  list_->push<DrawColorOp>(color, mode);
}

void RecordPaintCanvas::clear(SkColor color) {
  list_->push<DrawColorOp>(color, SkBlendMode::kSrc);
}

void RecordPaintCanvas::drawLine(SkScalar x0,
                                 SkScalar y0,
                                 SkScalar x1,
                                 SkScalar y1,
                                 const PaintFlags& flags) {
  list_->push<DrawLineOp>(x0, y0, x1, y1, flags);
}

void RecordPaintCanvas::drawRect(const SkRect& rect, const PaintFlags& flags) {
  list_->push<DrawRectOp>(rect, flags);
}

void RecordPaintCanvas::drawIRect(const SkIRect& rect,
                                  const PaintFlags& flags) {
  list_->push<DrawIRectOp>(rect, flags);
}

void RecordPaintCanvas::drawOval(const SkRect& oval, const PaintFlags& flags) {
  list_->push<DrawOvalOp>(oval, flags);
}

void RecordPaintCanvas::drawRRect(const SkRRect& rrect,
                                  const PaintFlags& flags) {
  list_->push<DrawRRectOp>(rrect, flags);
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
  list_->push<DrawDRRectOp>(outer, inner, flags);
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

void RecordPaintCanvas::drawPath(const SkPath& path, const PaintFlags& flags) {
  list_->push<DrawPathOp>(path, flags);
}

void RecordPaintCanvas::drawImage(const PaintImage& image,
                                  SkScalar left,
                                  SkScalar top,
                                  const PaintFlags* flags) {
  DCHECK(!image.IsPaintWorklet());
  list_->push<DrawImageOp>(image, left, top, flags);
}

void RecordPaintCanvas::drawImageRect(const PaintImage& image,
                                      const SkRect& src,
                                      const SkRect& dst,
                                      const PaintFlags* flags,
                                      SrcRectConstraint constraint) {
  list_->push<DrawImageRectOp>(image, src, dst, flags, constraint);
}

void RecordPaintCanvas::drawSkottie(scoped_refptr<SkottieWrapper> skottie,
                                    const SkRect& dst,
                                    float t) {
  list_->push<DrawSkottieOp>(std::move(skottie), dst, t);
}

void RecordPaintCanvas::drawTextBlob(sk_sp<SkTextBlob> blob,
                                     SkScalar x,
                                     SkScalar y,
                                     const PaintFlags& flags) {
  list_->push<DrawTextBlobOp>(std::move(blob), x, y, flags);
}

void RecordPaintCanvas::drawTextBlob(sk_sp<SkTextBlob> blob,
                                     SkScalar x,
                                     SkScalar y,
                                     NodeId node_id,
                                     const PaintFlags& flags) {
  list_->push<DrawTextBlobOp>(std::move(blob), x, y, node_id, flags);
}

void RecordPaintCanvas::drawPicture(sk_sp<const PaintRecord> record) {
  // TODO(enne): If this is small, maybe flatten it?
  list_->push<DrawRecordOp>(record);
}

bool RecordPaintCanvas::isClipEmpty() const {
  DCHECK(InitializedWithRecordingBounds());
  return GetCanvas()->isClipEmpty();
}

const SkMatrix& RecordPaintCanvas::getTotalMatrix() const {
  return GetCanvas()->getTotalMatrix();
}

void RecordPaintCanvas::Annotate(AnnotationType type,
                                 const SkRect& rect,
                                 sk_sp<SkData> data) {
  list_->push<AnnotateOp>(type, rect, data);
}

void RecordPaintCanvas::recordCustomData(uint32_t id) {
  list_->push<CustomDataOp>(id);
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
