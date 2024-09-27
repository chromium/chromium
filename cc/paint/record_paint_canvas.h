// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RECORD_PAINT_CANVAS_H_
#define CC_PAINT_RECORD_PAINT_CANVAS_H_

#include <optional>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skottie_color_map.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class PaintFilter;

// This implementation of PaintCanvas records paint operations into the given
// PaintOpBuffer. The methods that inspect the current clip or CTM are not
// implemented (DCHECK will fail if called). Use InspectableRecordPaintCanvas
// instead if the client needs to call those methods.
class CC_PAINT_EXPORT RecordPaintCanvas : public PaintCanvas {
 public:
  RecordPaintCanvas();
  ~RecordPaintCanvas() override;

  RecordPaintCanvas(const RecordPaintCanvas&) = delete;
  RecordPaintCanvas& operator=(const RecordPaintCanvas&) = delete;

  // Returns the set of paint ops recorded so far and clears it from the
  // internal buffer maintained by the canvas.
  virtual PaintRecord ReleaseAsRecord();
  // Returns the set of paint ops recorded so far without clearing it from the
  // internal buffer.
  virtual PaintRecord CopyAsRecord();

  // See comments around `maybe_draw_lines_as_paths_` for details.
  void DisableLineDrawingAsPaths();

  bool HasRecordedDrawOps() const { return buffer_.has_draw_ops(); }
  size_t TotalOpCount() const { return buffer_.total_op_count(); }
  size_t OpBytesUsed() const { return buffer_.paint_ops_size(); }

  void* accessTopLayerPixels(SkImageInfo* info,
                             size_t* rowBytes,
                             SkIPoint* origin = nullptr) override;

  void flush() override;
  bool NeedsFlush() const override;

  int save() override;
  int saveLayer(const PaintFlags& flags) override;
  int saveLayer(const SkRect& bounds, const PaintFlags& flags) override;
  int saveLayerAlphaf(float alpha) override;
  int saveLayerAlphaf(const SkRect& bounds, float alpha) override;
  int saveLayerFilters(base::span<sk_sp<PaintFilter>> filters,
                       const PaintFlags& flags) override;
  void restore() override;
  int getSaveCount() const final;
  void restoreToCount(int save_count) override;

  void translate(SkScalar dx, SkScalar dy) override;
  void scale(SkScalar sx, SkScalar sy) override;
  void rotate(SkScalar degrees) override;
  void concat(const SkM44& matrix) override;
  void setMatrix(const SkM44& matrix) override;

  void clipRect(const SkRect& rect, SkClipOp op, bool antialias) override;
  void clipRRect(const SkRRect& rrect, SkClipOp op, bool antialias) final;
  void clipPath(const SkPath& path,
                SkClipOp op,
                bool antialias,
                UsePaintCache use_paint_cache) final;

  // These state-query functions can be called only if `size` is not empty in
  // the constructor. With this restriction, we don't need to create
  // SkNoDrawCanvas for clients that only need recording.
  SkImageInfo imageInfo() const override;
  bool getLocalClipBounds(SkRect* bounds) const override;
  bool getDeviceClipBounds(SkIRect* bounds) const override;
  SkM44 getLocalToDevice() const override;

  void drawColor(SkColor4f color, SkBlendMode mode) override;
  void clear(SkColor4f color) override;
  void drawLine(SkScalar x0,
                SkScalar y0,
                SkScalar x1,
                SkScalar y1,
                const PaintFlags& flags) override;
  void drawArc(const SkRect& oval,
               SkScalar start_angle_degrees,
               SkScalar sweep_angle_degrees,
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
  void drawVertices(scoped_refptr<RefCountedBuffer<SkPoint>> vertices,
                    scoped_refptr<RefCountedBuffer<SkPoint>> uvs,
                    scoped_refptr<RefCountedBuffer<uint16_t>> indices,
                    const PaintFlags& flags) override;
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
  void drawPicture(PaintRecord record) override;
  void drawPicture(PaintRecord record, bool local_ctm) override;

  void Annotate(AnnotationType type,
                const SkRect& rect,
                sk_sp<SkData> data) override;
  void recordCustomData(uint32_t id) override;
  void setNodeId(int) override;

  // Don't shadow non-virtual helper functions.
  using PaintCanvas::clipPath;
  using PaintCanvas::clipRect;
  using PaintCanvas::clipRRect;
  using PaintCanvas::drawColor;
  using PaintCanvas::drawImage;
  using PaintCanvas::drawPath;
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

 protected:
  virtual void clipRRectInternal(const SkRRect& rrect,
                                 SkClipOp op,
                                 bool antialias);
  virtual void clipPathInternal(const SkPath& path,
                                SkClipOp op,
                                bool antialias,
                                UsePaintCache use_paint_cache);

  bool IsDrawLinesAsPathsEnabled() const { return maybe_draw_lines_as_paths_; }

 private:
  template <typename T, typename... Args>
  void push(Args&&... args);

  PaintOpBuffer buffer_;
  int save_count_ = 1;

  bool needs_flush_ = false;
#if DCHECK_IS_ON()
  unsigned disable_flush_check_scope_ = 0;
#endif
  // These fields are used to determine if lines should be rastered as paths.
  // Rasterization may batch operations, and that batching may be disabled if
  // drawLine() is used instead of drawPath(). These members are used to
  // determine is a drawLine() should be rastered as a drawPath().
  // TODO(crbug.com/40045234): figure out better heurstics.
  bool maybe_draw_lines_as_paths_ = true;
  uint32_t draw_path_count_ = 0;
  uint32_t draw_line_count_ = 0;
};

// Besides the recording functions, this implementation of PaintCanvas allows
// inspection of the current clip and CTM during recording.
class CC_PAINT_EXPORT InspectableRecordPaintCanvas : public RecordPaintCanvas {
 public:
  explicit InspectableRecordPaintCanvas(const gfx::Size& size);
  ~InspectableRecordPaintCanvas() override;

  int save() override;
  int saveLayer(const PaintFlags& flags) override;
  int saveLayer(const SkRect& bounds, const PaintFlags& flags) override;
  int saveLayerAlphaf(float alpha) override;
  int saveLayerAlphaf(const SkRect& bounds, float alpha) override;
  int saveLayerFilters(base::span<sk_sp<PaintFilter>> filters,
                       const PaintFlags& flags) override;
  void restore() override;

  void translate(SkScalar dx, SkScalar dy) override;
  void scale(SkScalar sx, SkScalar sy) override;
  void rotate(SkScalar degrees) override;
  void concat(const SkM44& matrix) override;
  void setMatrix(const SkM44& matrix) override;

  void clipRect(const SkRect& rect, SkClipOp op, bool antialias) override;

  SkImageInfo imageInfo() const override;
  bool getLocalClipBounds(SkRect* bounds) const override;
  bool getDeviceClipBounds(SkIRect* bounds) const override;
  SkM44 getLocalToDevice() const override;

  // Don't shadow non-virtual helper functions.
  using RecordPaintCanvas::clipRect;

 protected:
  // Creates a child canvas that has the same transform matrix and size as
  // `parent`. `CreateChildCanvasTag` is used to differentiate this from a copy
  // constructor.
  struct CreateChildCanvasTag {};
  InspectableRecordPaintCanvas(CreateChildCanvasTag,
                               const InspectableRecordPaintCanvas& parent);

 private:
  void clipRRectInternal(const SkRRect& rrect,
                         SkClipOp op,
                         bool antialias) override;
  void clipPathInternal(const SkPath& path,
                        SkClipOp op,
                        bool antialias,
                        UsePaintCache use_paint_cache) override;

  int CheckSaveCount(int super_prev_save_count, int canvas_prev_save_count);

  SkNoDrawCanvas canvas_;

  // Cached value of `canvas.getDeviceClipBounds()`. Cached as this value is
  // used in every fill/stroke operation and calculating is on the expensive
  // side.
  mutable std::optional<SkIRect> device_clip_bounds_;
};

}  // namespace cc

#endif  // CC_PAINT_RECORD_PAINT_CANVAS_H_
