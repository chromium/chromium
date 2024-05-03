// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_CANVAS_H_
#define CC_PAINT_PAINT_CANVAS_H_

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "cc/paint/node_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/refcounted_buffer.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_text_property_value.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkTextBlob;

namespace printing {
class MetafileSkia;
}  // namespace printing

namespace paint_preview {
class PaintPreviewTracker;
}  // namespace paint_preview

namespace cc {
class SkottieWrapper;
class PaintFilter;
class PaintFlags;
class PaintRecord;

enum class UsePaintCache { kDisabled = 0, kEnabled };

// PaintCanvas is the cc/paint wrapper of SkCanvas.  It has a more restricted
// interface than SkCanvas (trimmed back to only what Chrome uses).  Its reason
// for existence is so that it can do custom serialization logic into a
// PaintOpBuffer which (unlike SkPicture) is mutable, handles image replacement,
// and can be serialized in custom ways (such as using the transfer cache).
//
// PaintCanvas is usually implemented by either:
// (1) SkiaPaintCanvas, which is backed by an SkCanvas, usually for rasterizing.
// (2) RecordPaintCanvas, which records paint commands into a PaintOpBuffer.
//
// SkiaPaintCanvas allows callers to go from PaintCanvas to SkCanvas (or
// PaintRecord to SkPicture), but this is a one way trip.  There is no way to go
// from SkCanvas to PaintCanvas or from SkPicture back into PaintRecord.
class CC_PAINT_EXPORT PaintCanvas {
 public:
  PaintCanvas() = default;
  PaintCanvas(const PaintCanvas&) = delete;
  virtual ~PaintCanvas() = default;

  PaintCanvas& operator=(const PaintCanvas&) = delete;

  // TODO(enne): this only appears to mostly be used to determine if this is
  // recording or not, so could be simplified or removed.
  virtual SkImageInfo imageInfo() const = 0;

  virtual void* accessTopLayerPixels(SkImageInfo* info,
                                     size_t* rowBytes,
                                     SkIPoint* origin = nullptr) = 0;

  // TODO(enne): It would be nice to get rid of flush() entirely, as it
  // doesn't really make sense for recording.  However, this gets used by
  // PaintCanvasVideoRenderer which takes a PaintCanvas to paint both
  // software and hardware video.  This is super entangled with ImageBuffer
  // and canvas/video painting in Blink where the same paths are used for
  // both recording and gpu work.
  virtual void flush() = 0;

  virtual int save() = 0;
  virtual int saveLayer(const PaintFlags& flags) = 0;
  virtual int saveLayer(const SkRect& bounds, const PaintFlags& flags) = 0;
  virtual int saveLayerAlphaf(float alpha) = 0;
  virtual int saveLayerAlphaf(const SkRect& bounds, float alpha) = 0;
  // Opens a layer whose output texture is composited multiple times to the
  // canvas, once for every filter in `filters`. Useful to draw a foreground and
  // its shadow. Implementations may consume `filters` by moving the `sk_sp`.
  virtual int saveLayerFilters(base::span<sk_sp<PaintFilter>> filters,
                               const PaintFlags& flags) = 0;

  virtual void restore() = 0;
  virtual int getSaveCount() const = 0;
  virtual void restoreToCount(int save_count) = 0;
  virtual void translate(SkScalar dx, SkScalar dy) = 0;
  virtual void scale(SkScalar sx, SkScalar sy) = 0;
  void scale(SkScalar s) { scale(s, s); }
  virtual void rotate(SkScalar degrees) = 0;
  virtual void concat(const SkM44& matrix) = 0;
  virtual void setMatrix(const SkM44& matrix) = 0;

  virtual void clipRect(const SkRect& rect,
                        SkClipOp op,
                        bool do_anti_alias) = 0;
  void clipRect(const SkRect& rect, SkClipOp op) { clipRect(rect, op, false); }
  void clipRect(const SkRect& rect, bool do_anti_alias) {
    clipRect(rect, SkClipOp::kIntersect, do_anti_alias);
  }
  void clipRect(const SkRect& rect) {
    clipRect(rect, SkClipOp::kIntersect, false);
  }

  virtual void clipRRect(const SkRRect& rrect,
                         SkClipOp op,
                         bool do_anti_alias) = 0;
  void clipRRect(const SkRRect& rrect, bool do_anti_alias) {
    clipRRect(rrect, SkClipOp::kIntersect, do_anti_alias);
  }
  void clipRRect(const SkRRect& rrect, SkClipOp op) {
    clipRRect(rrect, op, false);
  }
  void clipRRect(const SkRRect& rrect) {
    clipRRect(rrect, SkClipOp::kIntersect, false);
  }

  virtual void clipPath(const SkPath& path,
                        SkClipOp op,
                        bool do_anti_alias,
                        UsePaintCache) = 0;
  void clipPath(const SkPath& path, SkClipOp op, bool do_anti_alias) {
    clipPath(path, op, do_anti_alias, UsePaintCache::kEnabled);
  }
  void clipPath(const SkPath& path, SkClipOp op) {
    clipPath(path, op, /*do_anti_alias=*/false, UsePaintCache::kEnabled);
  }
  void clipPath(const SkPath& path, bool do_anti_alias) {
    clipPath(path, SkClipOp::kIntersect, do_anti_alias,
             UsePaintCache::kEnabled);
  }

  virtual bool getLocalClipBounds(SkRect* bounds) const = 0;
  virtual bool getDeviceClipBounds(SkIRect* bounds) const = 0;
  virtual void drawColor(SkColor4f color, SkBlendMode mode) = 0;
  void drawColor(SkColor4f color) { drawColor(color, SkBlendMode::kSrcOver); }

  // TODO(enne): This is a synonym for drawColor with kSrc.  Remove it.
  virtual void clear(SkColor4f color) = 0;

  virtual void drawLine(SkScalar x0,
                        SkScalar y0,
                        SkScalar x1,
                        SkScalar y1,
                        const PaintFlags& flags) = 0;
  virtual void drawArc(const SkRect& oval,
                       SkScalar start_angle_degrees,
                       SkScalar sweep_angle_degrees,
                       const PaintFlags& flags) = 0;
  virtual void drawRect(const SkRect& rect, const PaintFlags& flags) = 0;
  virtual void drawIRect(const SkIRect& rect, const PaintFlags& flags) = 0;
  virtual void drawOval(const SkRect& oval, const PaintFlags& flags) = 0;
  virtual void drawRRect(const SkRRect& rrect, const PaintFlags& flags) = 0;
  virtual void drawDRRect(const SkRRect& outer,
                          const SkRRect& inner,
                          const PaintFlags& flags) = 0;
  virtual void drawRoundRect(const SkRect& rect,
                             SkScalar rx,
                             SkScalar ry,
                             const PaintFlags& flags) = 0;
  virtual void drawPath(const SkPath& path,
                        const PaintFlags& flags,
                        UsePaintCache) = 0;
  void drawPath(const SkPath& path, const PaintFlags& flags) {
    drawPath(path, flags, UsePaintCache::kEnabled);
  }
  virtual void drawImage(const PaintImage& image,
                         SkScalar left,
                         SkScalar top,
                         const SkSamplingOptions&,
                         const PaintFlags* flags) = 0;
  void drawImage(const PaintImage& image, SkScalar left, SkScalar top) {
    drawImage(image, left, top, SkSamplingOptions(), nullptr);
  }

  virtual void drawImageRect(const PaintImage& image,
                             const SkRect& src,
                             const SkRect& dst,
                             const SkSamplingOptions&,
                             const PaintFlags* flags,
                             SkCanvas::SrcRectConstraint constraint) = 0;
  void drawImageRect(const PaintImage& image,
                     const SkRect& src,
                     const SkRect& dst,
                     SkCanvas::SrcRectConstraint constraint) {
    drawImageRect(image, src, dst, SkSamplingOptions(), nullptr, constraint);
  }

  virtual void drawVertices(scoped_refptr<RefCountedBuffer<SkPoint>> vertices,
                            scoped_refptr<RefCountedBuffer<SkPoint>> uvs,
                            scoped_refptr<RefCountedBuffer<uint16_t>> indices,
                            const PaintFlags& flags) = 0;

  // Draws the frame of the |skottie| animation specified by the normalized time
  // t [0->first frame..1->last frame] at the destination bounds given by |dst|
  // onto the canvas. |images| is a map from asset id to the corresponding image
  // to use when rendering this frame; it may be empty if this animation frame
  // does not contain any images in it.
  virtual void drawSkottie(scoped_refptr<SkottieWrapper> skottie,
                           const SkRect& dst,
                           float t,
                           SkottieFrameDataMap images,
                           const SkottieColorMap& color_map,
                           SkottieTextPropertyValueMap text_map) = 0;

  virtual void drawTextBlob(sk_sp<SkTextBlob> blob,
                            SkScalar x,
                            SkScalar y,
                            const PaintFlags& flags) = 0;

  virtual void drawTextBlob(sk_sp<SkTextBlob> blob,
                            SkScalar x,
                            SkScalar y,
                            NodeId node_id,
                            const PaintFlags& flags) = 0;

  // Draws `record` into the canvas. Unlike SkCanvas::drawPicture, this only
  // plays back the PaintRecord and does not add an additional clip.  This is
  // closer to SkPicture::playback.
  //
  // If `local_ctm` is `true`, transform ops in `record` are treated as local to
  // that recording: `SetMatrixOp` acts relatively to the current canvas
  // transform and any transform changes are restored before `drawPicture`
  // returns. Otherwise, transforms in `record` are treated as global to the
  // canvas: `SetMatrixOp` ignores and overrides any previously set transforms
  // and all CTM changes are preserved after `drawPicture` returns.
  virtual void drawPicture(PaintRecord record) = 0;
  virtual void drawPicture(PaintRecord record, bool local_ctm) = 0;

  virtual SkM44 getLocalToDevice() const = 0;

  virtual bool NeedsFlush() const = 0;

  // Used for printing
  enum class AnnotationType {
    kUrl,
    kNameDestination,
    kLinkToDestination,
  };
  virtual void Annotate(AnnotationType type,
                        const SkRect& rect,
                        sk_sp<SkData> data) = 0;
  printing::MetafileSkia* GetPrintingMetafile() const { return metafile_; }
  void SetPrintingMetafile(printing::MetafileSkia* metafile) {
    metafile_ = metafile;
  }
  paint_preview::PaintPreviewTracker* GetPaintPreviewTracker() const {
    return tracker_;
  }
  void SetPaintPreviewTracker(paint_preview::PaintPreviewTracker* tracker) {
    tracker_ = tracker;
  }

  // Subclasses can override to handle custom data.
  virtual void recordCustomData(uint32_t id) {}

  // Used for marked content in PDF files.
  virtual void setNodeId(int) = 0;

 private:
  raw_ptr<printing::MetafileSkia> metafile_ = nullptr;
  raw_ptr<paint_preview::PaintPreviewTracker, DanglingUntriaged> tracker_ =
      nullptr;
};

class CC_PAINT_EXPORT PaintCanvasAutoRestore {
 public:
  PaintCanvasAutoRestore(PaintCanvas* canvas, bool save) : canvas_(canvas) {
    if (canvas_) {
      save_count_ = canvas_->getSaveCount();
      if (save) {
        canvas_->save();
      }
    }
  }

  ~PaintCanvasAutoRestore() {
    if (canvas_) {
      canvas_->restoreToCount(save_count_);
    }
  }

  void restore() {
    if (canvas_) {
      canvas_->restoreToCount(save_count_);
      canvas_ = nullptr;
    }
  }

 private:
  raw_ptr<PaintCanvas> canvas_ = nullptr;
  int save_count_ = 0;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_CANVAS_H_
