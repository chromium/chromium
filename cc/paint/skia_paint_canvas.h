// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKIA_PAINT_CANVAS_H_
#define CC_PAINT_SKIA_PAINT_CANVAS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skottie_color_map.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkTextBlob.h"

class SkCanvas;
class SkM44;
class SkPath;
class SkRRect;
class SkSurfaceProps;
enum class SkClipOp;
struct SkImageInfo;
struct SkIPoint;
struct SkIRect;
struct SkRect;

namespace cc {
class ImageProvider;
class PaintFilter;
class PaintFlags;

// A PaintCanvas derived class that passes PaintCanvas APIs through to
// an SkCanvas.  This is more efficient than recording to a PaintRecord
// and then playing back to an SkCanvas.
class CC_PAINT_EXPORT SkiaPaintCanvas final : public PaintCanvas {
 public:
  struct CC_PAINT_EXPORT ContextFlushes {
    ContextFlushes();

    bool enable;
    int max_draws_before_flush;
  };

  explicit SkiaPaintCanvas(SkCanvas* canvas,
                           ImageProvider* image_provider = nullptr,
                           ContextFlushes context_flushes = ContextFlushes());
  explicit SkiaPaintCanvas(const SkBitmap& bitmap,
                           ImageProvider* image_provider = nullptr);
  explicit SkiaPaintCanvas(const SkBitmap& bitmap, const SkSurfaceProps& props);

  SkiaPaintCanvas(const SkiaPaintCanvas&) = delete;
  ~SkiaPaintCanvas() override;

  SkiaPaintCanvas& operator=(const SkiaPaintCanvas&) = delete;

  void reset_image_provider() { image_provider_ = nullptr; }

  SkImageInfo imageInfo() const override;

  void* accessTopLayerPixels(SkImageInfo* info,
                             size_t* rowBytes,
                             SkIPoint* origin = nullptr) override;

  void flush() override;

  int save() override;
  int saveLayer(const PaintFlags& flags) override;
  int saveLayer(const SkRect& bounds, const PaintFlags& flags) override;
  int saveLayerAlphaf(float alpha) override;
  int saveLayerAlphaf(const SkRect& bounds, float alpha) override;
  int saveLayerFilters(base::span<sk_sp<PaintFilter>> filters,
                       const PaintFlags& flags) override;

  void restore() override;
  int getSaveCount() const override;
  void restoreToCount(int save_count) override;
  void translate(SkScalar dx, SkScalar dy) override;
  void scale(SkScalar sx, SkScalar sy) override;
  void rotate(SkScalar degrees) override;
  void concat(const SkM44& matrix) override;
  void setMatrix(const SkM44& matrix) override;

  void clipRect(const SkRect& rect, SkClipOp op, bool do_anti_alias) override;
  void clipRRect(const SkRRect& rrect,
                 SkClipOp op,
                 bool do_anti_alias) override;
  void clipPath(const SkPath& path,
                SkClipOp op,
                bool do_anti_alias,
                UsePaintCache) override;
  bool getLocalClipBounds(SkRect* bounds) const override;
  bool getDeviceClipBounds(SkIRect* bounds) const override;
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
                UsePaintCache) override;
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

  SkM44 getLocalToDevice() const override;

  bool NeedsFlush() const override;

  void Annotate(AnnotationType type,
                const SkRect& rect,
                sk_sp<SkData> data) override;

  void setNodeId(int) override;

  // Don't shadow non-virtual helper functions.
  using PaintCanvas::clipPath;
  using PaintCanvas::clipRect;
  using PaintCanvas::clipRRect;
  using PaintCanvas::drawColor;
  using PaintCanvas::drawImage;
  using PaintCanvas::drawPicture;

  // Same as the above drawPicture() except using the given custom data
  // raster callback.
  void drawPicture(
      PaintRecord record,
      PlaybackCallbacks::CustomDataRasterCallback custom_raster_callback,
      bool local_ctm = true);

  int pendingOps() const { return num_of_ops_; }

 private:
  void FlushAfterDrawIfNeeded();

  int GetMaxTextureSize() const;

  raw_ptr<SkCanvas, DanglingUntriaged> canvas_;
  SkBitmap bitmap_;
  std::unique_ptr<SkCanvas> owned_;
  raw_ptr<ImageProvider, DanglingUntriaged> image_provider_ = nullptr;

  const ContextFlushes context_flushes_;
  int num_of_ops_ = 0;
};

}  // namespace cc

#endif  // CC_PAINT_SKIA_PAINT_CANVAS_H_
