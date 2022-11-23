// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_WRITER_H_
#define CC_PAINT_PAINT_OP_WRITER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

struct SkRect;
struct SkIRect;
class SkRRect;

namespace gpu {
struct Mailbox;
}

namespace cc {

class DecodedDrawImage;
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
  void Write(const SkM44& data);
  void Write(uint8_t data);
  void Write(uint32_t data);
  void Write(uint64_t data);
  void Write(int32_t data);
  void Write(const SkRect& rect);
  void Write(const SkIRect& rect);
  void Write(const SkRRect& rect);
  void Write(const SkColor4f& color);
  void Write(const SkPath& path, UsePaintCache);
  void Write(const sk_sp<SkData>& data);
  void Write(const SkColorSpace* data);
  void Write(const SkSamplingOptions&);
  void Write(const sk_sp<GrSlug>& slug);
  void Write(SkYUVColorSpace yuv_color_space);
  void Write(SkYUVAInfo::PlaneConfig plane_config);
  void Write(SkYUVAInfo::Subsampling subsampling);
  void Write(const gpu::Mailbox& mailbox);

  // Shaders and filters need to know the current transform in order to lock in
  // the scale factor they will be evaluated at after deserialization. This is
  // critical to ensure that nested PaintRecords are analyzed and rasterized
  // identically when text is involved.
  void Write(const PaintFlags& flags, const SkM44& current_ctm);
  void Write(const PaintShader* shader,
             PaintFlags::FilterQuality quality,
             const SkM44& current_ctm);
  void Write(const PaintFilter* filter, const SkM44& current_ctm);

  void Write(SkClipOp op) { WriteEnum(op); }
  void Write(PaintCanvas::AnnotationType type) { WriteEnum(type); }
  void Write(SkCanvas::SrcRectConstraint constraint) { WriteEnum(constraint); }
  void Write(SkColorType color_type) { WriteEnum(color_type); }
  void Write(PaintFlags::FilterQuality filter_quality) {
    WriteEnum(filter_quality);
  }
  void Write(SkBlendMode blend_mode) { WriteEnum(blend_mode); }
  void Write(SkTileMode tile_mode) { WriteEnum(tile_mode); }
  void Write(SkFilterMode filter_mode) { WriteEnum(filter_mode); }
  void Write(SkMipmapMode mipmap_mode) { WriteEnum(mipmap_mode); }

  void Write(bool data) { Write(static_cast<uint8_t>(data)); }

  // Aligns the memory to the given alignment.
  void AlignMemory(size_t alignment);

  void AssertAlignment(size_t alignment) {
#if DCHECK_IS_ON()
    uintptr_t memory = reinterpret_cast<uintptr_t>(memory_.get());
    DCHECK_EQ(base::bits::AlignUp(memory, alignment), memory);
#endif
  }

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

  // Serializes the given |skottie| vector graphic.
  void Write(scoped_refptr<SkottieWrapper> skottie);

 private:
  template <typename T>
  void WriteSimple(const T& val);

  void WriteFlattenable(const SkFlattenable* val);

  template <typename Enum>
  void WriteEnum(Enum value) {
    Write(base::checked_cast<uint8_t>(value));
  }

  // The main entry point is Write(const PaintFilter* filter) which casts the
  // filter and calls one of the following functions.
  void Write(const ColorFilterPaintFilter& filter, const SkM44& current_ctm);
  void Write(const BlurPaintFilter& filter, const SkM44& current_ctm);
  void Write(const DropShadowPaintFilter& filter, const SkM44& current_ctm);
  void Write(const MagnifierPaintFilter& filter, const SkM44& current_ctm);
  void Write(const ComposePaintFilter& filter, const SkM44& current_ctm);
  void Write(const AlphaThresholdPaintFilter& filter, const SkM44& current_ctm);
  void Write(const XfermodePaintFilter& filter, const SkM44& current_ctm);
  void Write(const ArithmeticPaintFilter& filter, const SkM44& current_ctm);
  void Write(const MatrixConvolutionPaintFilter& filter,
             const SkM44& current_ctm);
  void Write(const DisplacementMapEffectPaintFilter& filter,
             const SkM44& current_ctm);
  void Write(const ImagePaintFilter& filter, const SkM44& current_ctm);
  void Write(const RecordPaintFilter& filter, const SkM44& current_ctm);
  void Write(const MergePaintFilter& filter, const SkM44& current_ctm);
  void Write(const MorphologyPaintFilter& filter, const SkM44& current_ctm);
  void Write(const OffsetPaintFilter& filter, const SkM44& current_ctm);
  void Write(const TilePaintFilter& filter, const SkM44& current_ctm);
  void Write(const TurbulencePaintFilter& filter, const SkM44& current_ctm);
  void Write(const ShaderPaintFilter& filter, const SkM44& current_ctm);
  void Write(const MatrixPaintFilter& filter, const SkM44& current_ctm);
  void Write(const LightingDistantPaintFilter& filter,
             const SkM44& current_ctm);
  void Write(const LightingPointPaintFilter& filter, const SkM44& current_ctm);
  void Write(const LightingSpotPaintFilter& filter, const SkM44& current_ctm);

  void Write(const PaintRecord* record,
             const gfx::Rect& playback_rect,
             const gfx::SizeF& post_scale);
  void Write(const SkRegion& region);
  void WriteImage(const DecodedDrawImage& decoded_draw_image);
  void WriteImage(uint32_t transfer_cache_entry_id, bool needs_mips);
  void WriteImage(const gpu::Mailbox& mailbox);
  void DidWrite(size_t bytes_written);
  void EnsureBytes(size_t required_bytes);
  sk_sp<PaintShader> TransformShaderIfNecessary(
      const PaintShader* original,
      PaintFlags::FilterQuality quality,
      const SkM44& current_ctm,
      uint32_t* paint_image_transfer_cache_entry_id,
      gfx::SizeF* paint_record_post_scale,
      bool* paint_image_needs_mips,
      gpu::Mailbox* mailbox_out);

  raw_ptr<char> memory_ = nullptr;
  size_t size_ = 0u;
  size_t remaining_bytes_ = 0u;
  const raw_ref<const PaintOp::SerializeOptions> options_;
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
