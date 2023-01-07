// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RECORD_PAINT_CANVAS_H_
#define CC_PAINT_RECORD_PAINT_CANVAS_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skottie_color_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class DisplayItemList;
class PaintFlags;

class CC_PAINT_EXPORT RecordPaintCanvas : public PaintCanvas {
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
  // TODO(crbug.com/1167153): The concat and setMatrix methods that take an
  // SkMatrix should be removed in favor of the SkM44 versions.
  void concat(const SkMatrix& matrix) override;
  void setMatrix(const SkMatrix& matrix) override;
  void concat(const SkM44& matrix) override;
  void setMatrix(const SkM44& matrix) override;

  void clipRect(const SkRect& rect, SkClipOp op, bool antialias) override;
  void clipRRect(const SkRRect& rrect, SkClipOp op, bool antialias) override;
  void clipPath(const SkPath& path,
                SkClipOp op,
                bool antialias,
                UsePaintCache use_paint_cache) override;
  SkRect getLocalClipBounds() const override;
  bool getLocalClipBounds(SkRect* bounds) const override;
  SkIRect getDeviceClipBounds() const override;
  bool getDeviceClipBounds(SkIRect* bounds) const override;
  void drawColor(SkColor4f color, SkBlendMode mode) override;
  void clear(SkColor4f color) override;

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
  void drawPath(const SkPath& path,
                const PaintFlags& flags,
                UsePaintCache use_paint_cache) override;
  void drawImage(const PaintImage& image,
                 SkScalar left,
                 SkScalar top,
                 const SkSamplingOptions&,
                 const PaintFlags* flags) override;
  void drawImageRect(const PaintImage& image,
                     const SkRect& src,
                     const SkRect& dst,
                     const SkSamplingOptions&,
                     const PaintFlags* flags,
                     SkCanvas::SrcRectConstraint constraint) override;
  void drawSkottie(scoped_refptr<SkottieWrapper> skottie,
                   const SkRect& dst,
                   float t,
                   SkottieFrameDataMap images,
                   const SkottieColorMap& color_map,
                   SkottieTextPropertyValueMap text_map) override;
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
  SkMatrix getTotalMatrix() const override;
  SkM44 getLocalToDevice() const override;

  void Annotate(AnnotationType type,
                const SkRect& rect,
                sk_sp<SkData> data) override;
  void recordCustomData(uint32_t id) override;
  void setNodeId(int) override;

  bool NeedsFlush() const override;

  // Don't shadow non-virtual helper functions.
  using PaintCanvas::clipRect;
  using PaintCanvas::clipRRect;
  using PaintCanvas::clipPath;
  using PaintCanvas::drawColor;
  using PaintCanvas::drawImage;
  using PaintCanvas::drawPicture;

#if DCHECK_IS_ON()
  void EnterDisableFlushCheckScope() { ++disable_flush_check_scope_; }
  void LeaveDisableFlushCheckScope() { DCHECK(disable_flush_check_scope_--); }
  bool IsInDisableFlushCheckScope() { return disable_flush_check_scope_; }
#endif

  class DisableFlushCheckScope {
    // Create an object of this type to temporarily allow draw commands to be
    // recorded while the recording is marked as needing to be flushed.  This is
    // meant to be used to allow client code to issue the commands necessary to
    // reach a state where the recording can be safely flushed before beginning
    // to enforce a check that forbids recording additional draw commands after
    // a flush was requested.
   public:
    explicit DisableFlushCheckScope(RecordPaintCanvas* canvas) {
#if DCHECK_IS_ON()
      // We require that NeedsFlush be false upon entering a top-level scope
      // to prevent consecutive scopes from evading evading flush checks
      // indefinitely.
      DCHECK(!canvas->NeedsFlush() || canvas->IsInDisableFlushCheckScope());
      canvas->EnterDisableFlushCheckScope();
      canvas_ = canvas;
#endif
    }
    ~DisableFlushCheckScope() {
#if DCHECK_IS_ON()
      canvas_->LeaveDisableFlushCheckScope();
#endif
    }

   private:
#if DCHECK_IS_ON()
    raw_ptr<RecordPaintCanvas> canvas_;
#endif
  };

 private:
  template <typename T, typename... Args>
  size_t push(Args&&... args);

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
  mutable absl::optional<SkNoDrawCanvas> canvas_;
  SkRect recording_bounds_;
  bool needs_flush_ = false;
#if DCHECK_IS_ON()
  unsigned disable_flush_check_scope_ = 0;
#endif
};

}  // namespace cc

#endif  // CC_PAINT_RECORD_PAINT_CANVAS_H_
