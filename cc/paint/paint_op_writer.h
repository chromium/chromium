// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_WRITER_H_
#define CC_PAINT_PAINT_OP_WRITER_H_

#include <unordered_set>

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "third_party/skia/include/core/SkImageInfo.h"

struct SkRect;
struct SkIRect;
class SkRRect;

namespace cc {

class DrawImage;
class PaintShader;

class CC_PAINT_EXPORT PaintOpWriter {
 public:
  // The SerializeOptions passed to the writer must set the required fields
  // if it can be used for serializing images, paint records or text blobs.
  PaintOpWriter(void* memory,
                size_t size,
                const PaintOp::SerializeOptions& options,
                bool enable_security_constraints = false);
  ~PaintOpWriter();

  static size_t constexpr HeaderBytes() { return 4u; }
  static size_t constexpr Alignment() { return 4u; }
  static size_t GetFlattenableSize(const SkFlattenable* flattenable);
  static size_t GetImageSize(const PaintImage& image);
  static size_t GetRecordSize(const PaintRecord* record);

  // Write a sequence of arbitrary bytes.
  void WriteData(size_t bytes, const void* input);

  size_t size() const { return valid_ ? size_ - remaining_bytes_ : 0u; }

  uint64_t* WriteSize(size_t size);

  void Write(SkScalar data);
  void Write(SkMatrix data);
  void Write(uint8_t data);
  void Write(uint32_t data);
  void Write(uint64_t data);
  void Write(int32_t data);
  void Write(const SkRect& rect);
  void Write(const SkIRect& rect);
  void Write(const SkRRect& rect);

  void Write(const SkPath& path);
  void Write(const PaintFlags& flags);
  void Write(const sk_sp<SkData>& data);
  void Write(const SkColorSpace* data);
  void Write(const PaintShader* shader, SkFilterQuality quality);
  void Write(const PaintFilter* filter);
  void Write(const sk_sp<SkTextBlob>& blob);
  void Write(SkColorType color_type);
  void Write(SkYUVColorSpace yuv_color_space);

  void Write(SkClipOp op) { Write(static_cast<uint8_t>(op)); }
  void Write(PaintCanvas::AnnotationType type) {
    Write(static_cast<uint8_t>(type));
  }
  void Write(PaintCanvas::SrcRectConstraint constraint) {
    Write(static_cast<uint8_t>(constraint));
  }
  void Write(SkFilterQuality filter_quality) {
    Write(static_cast<uint8_t>(filter_quality));
  }
  void Write(bool data) { Write(static_cast<uint8_t>(data)); }

  // Aligns the memory to the given alignment.
  void AlignMemory(size_t alignment);

  // sk_sp is implicitly convertible to uint8_t (likely via implicit bool
  // conversion). In order to avoid accidentally calling that overload instead
  // of a specific function (such as would be the case if one forgets to call
  // .get() on it), the following template asserts if it's instantiated.
  template <typename T>
  void Write(const sk_sp<T>&) {
    // Note that this is essentially static_assert(false, ...) but it needs to
    // be dependent on T in order to only trigger if instantiated.
    static_assert(sizeof(T) == 0,
                  "Attempted to call a non-existent sk_sp override.");
  }
  template <typename T>
  void Write(const T*) {
    static_assert(sizeof(T) == 0,
                  "Attempted to call a non-existent T* override.");
  }

  // Serializes the given |draw_image|.
  // |scale_adjustment| is set to the scale applied to the serialized image.
  // |quality| is set to the quality that should be used when rasterizing this
  // image.
  void Write(const DrawImage& draw_image, SkSize* scale_adjustment);

 private:
  template <typename T>
  void WriteSimple(const T& val);

  void WriteFlattenable(const SkFlattenable* val);

  // The main entry point is Write(const PaintFilter* filter) which casts the
  // filter and calls one of the following functions.
  void Write(const ColorFilterPaintFilter& filter);
  void Write(const BlurPaintFilter& filter);
  void Write(const DropShadowPaintFilter& filter);
  void Write(const MagnifierPaintFilter& filter);
  void Write(const ComposePaintFilter& filter);
  void Write(const AlphaThresholdPaintFilter& filter);
  void Write(const XfermodePaintFilter& filter);
  void Write(const ArithmeticPaintFilter& filter);
  void Write(const MatrixConvolutionPaintFilter& filter);
  void Write(const DisplacementMapEffectPaintFilter& filter);
  void Write(const ImagePaintFilter& filter);
  void Write(const RecordPaintFilter& filter);
  void Write(const MergePaintFilter& filter);
  void Write(const MorphologyPaintFilter& filter);
  void Write(const OffsetPaintFilter& filter);
  void Write(const TilePaintFilter& filter);
  void Write(const TurbulencePaintFilter& filter);
  void Write(const PaintFlagsPaintFilter& filter);
  void Write(const MatrixPaintFilter& filter);
  void Write(const LightingDistantPaintFilter& filter);
  void Write(const LightingPointPaintFilter& filter);
  void Write(const LightingSpotPaintFilter& filter);

  void Write(const PaintRecord* record,
             const gfx::Rect& playback_rect,
             const gfx::SizeF& post_scale,
             const SkMatrix& post_matrix_for_analysis);
  void Write(const SkRegion& region);
  void WriteImage(uint32_t transfer_cache_entry_id, bool needs_mips);

  void EnsureBytes(size_t required_bytes);
  sk_sp<PaintShader> TransformShaderIfNecessary(
      const PaintShader* original,
      SkFilterQuality quality,
      uint32_t* paint_image_transfer_cache_entry_id,
      gfx::SizeF* paint_record_post_scale,
      bool* paint_image_needs_mips);

  char* memory_ = nullptr;
  size_t size_ = 0u;
  size_t remaining_bytes_ = 0u;
  const PaintOp::SerializeOptions& options_;
  bool valid_ = true;

  // Indicates that the following security constraints must be applied during
  // serialization:
  // 1) PaintRecords and SkDrawLoopers must be ignored.
  // 2) Codec backed images must be decoded and only the bitmap should be
  // serialized.
  const bool enable_security_constraints_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_WRITER_H_
