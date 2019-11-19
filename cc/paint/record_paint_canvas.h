// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RECORD_PAINT_CANVAS_H_
#define CC_PAINT_RECORD_PAINT_CANVAS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class DisplayItemList;
class PaintFlags;

class CC_PAINT_EXPORT RecordPaintCanvas final : public PaintCanvas {
 public:
  RecordPaintCanvas(DisplayItemList* list, const SkRect& bounds);
  RecordPaintCanvas(const RecordPaintCanvas&) = delete;
  ~RecordPaintCanvas() override;

  RecordPaintCanvas& operator=(const RecordPaintCanvas&) = delete;

  SkImageInfo imageInfo() const override;

  void* accessTopLayerPixels(SkImageInfo* info,
                             size_t* rowBytes,
                             SkIPoint* origin = nullptr) override;

  void flush() override;

  int save() override;
  int saveLayer(const SkRect* bounds, const PaintFlags* flags) override;
  int saveLayerAlpha(const SkRect* bounds, uint8_t alpha) override;

  void restore() override;
  int getSaveCount() const override;
  void restoreToCount(int save_count) override;
  void translate(SkScalar dx, SkScalar dy) override;
  void scale(SkScalar sx, SkScalar sy) override;
  void rotate(SkScalar degrees) override;
  void concat(const SkMatrix& matrix) override;
  void setMatrix(const SkMatrix& matrix) override;

  void clipRect(const SkRect& rect, SkClipOp op, bool antialias) override;
  void clipRRect(const SkRRect& rrect, SkClipOp op, bool antialias) override;
  void clipPath(const SkPath& path, SkClipOp op, bool antialias) override;
  SkRect getLocalClipBounds() const override;
  bool getLocalClipBounds(SkRect* bounds) const override;
  SkIRect getDeviceClipBounds() const override;
  bool getDeviceClipBounds(SkIRect* bounds) const override;
  void drawColor(SkColor color, SkBlendMode mode) override;
  void clear(SkColor color) override;

  void drawLine(SkScalar x0,
                SkScalar y0,
                SkScalar x1,
                SkScalar y1,
                const PaintFlags& flags) override;
  void drawRect(const SkRect& rect, const PaintFlags& flags) override;
  void drawIRect(const SkIRect& rect, const PaintFlags& flags) override;
  void drawOval(const SkRect& oval, const PaintFlags& flags) override;
  void drawRRect(const SkRRect& rrect, const PaintFlags& flags) override;
  void drawDRRect(const SkRRect& outer,
                  const SkRRect& inner,
                  const PaintFlags& flags) override;
  void drawRoundRect(const SkRect& rect,
                     SkScalar rx,
                     SkScalar ry,
                     const PaintFlags& flags) override;
  void drawPath(const SkPath& path, const PaintFlags& flags) override;
  void drawImage(const PaintImage& image,
                 SkScalar left,
                 SkScalar top,
                 const PaintFlags* flags) override;
  void drawImageRect(const PaintImage& image,
                     const SkRect& src,
                     const SkRect& dst,
                     const PaintFlags* flags,
                     SrcRectConstraint constraint) override;
  void drawSkottie(scoped_refptr<SkottieWrapper> skottie,
                   const SkRect& dst,
                   float t) override;
  void drawTextBlob(sk_sp<SkTextBlob> blob,
                    SkScalar x,
                    SkScalar y,
                    const PaintFlags& flags) override;
  void drawTextBlob(sk_sp<SkTextBlob> blob,
                    SkScalar x,
                    SkScalar y,
                    NodeId node_id,
                    const PaintFlags& flags) override;

  void drawPicture(sk_sp<const PaintRecord> record) override;

  bool isClipEmpty() const override;
  const SkMatrix& getTotalMatrix() const override;

  void Annotate(AnnotationType type,
                const SkRect& rect,
                sk_sp<SkData> data) override;
  void recordCustomData(uint32_t id) override;

  // Don't shadow non-virtual helper functions.
  using PaintCanvas::clipRect;
  using PaintCanvas::clipRRect;
  using PaintCanvas::clipPath;
  using PaintCanvas::drawColor;
  using PaintCanvas::drawImage;
  using PaintCanvas::drawPicture;

 private:
  const SkNoDrawCanvas* GetCanvas() const;
  SkNoDrawCanvas* GetCanvas();

  bool InitializedWithRecordingBounds() const;

  DisplayItemList* list_;

  // TODO(enne): Although RecordPaintCanvas is mostly a write-only interface
  // where paint commands are stored, occasionally users of PaintCanvas want
  // to ask stateful questions mid-stream of clip and transform state.
  // To avoid duplicating all this code (for now?), just forward to an SkCanvas
  // that's not backed by anything but can answer these questions.
  //
  // This is mutable so that const functions (e.g. quickReject) that may
  // lazy initialize the canvas can still be const.
  mutable base::Optional<SkNoDrawCanvas> canvas_;
  SkRect recording_bounds_;
};

}  // namespace cc

#endif  // CC_PAINT_RECORD_PAINT_CANVAS_H_
