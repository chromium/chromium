// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CC_PAINT_PAINT_OP_WRITER_H_
#define CC_PAINT_PAINT_OP_WRITER_H_

#include <memory>
#include <type_traits>
#include <vector>

#include "base/bits.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/numerics/checked_math.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

struct SkGainmapInfo;
struct SkHighContrastConfig;
struct SkRect;
struct SkIRect;
class SkM44;
class SkRRect;
namespace sktext::gpu {
class Slug;
}

namespace gfx {
struct HDRMetadata;
}

namespace gpu {
struct Mailbox;
}

namespace cc {

class ColorFilter;
class DecodedDrawImage;
class DrawImage;
class DrawLooper;
class PaintShader;
class PathEffect;

class CC_PAINT_EXPORT PaintOpWriter {
  STACK_ALLOCATED();

 public:
  // The SerializeOptions passed to the writer must set the required fields
  // if it can be used for serializing images, paint records or text blobs.
  // If `enable_security_constraints` is false, `memory` must be aligned to
  // kMaxAlignment, and AllocateAlignedBuffer() is the preferred way to
  // allocate `memory`. Otherwise `memory` can be allocated in any way that can
  // ensure kDefaultAlignment. See BufferAlignment() for more details.
  // If `size` is not enough to contain serialized data, the buffer won't
  // overflow, but Write() will be silent no-ops.
  PaintOpWriter(void* memory,
                size_t size,
                const PaintOp::SerializeOptions& options,
                bool enable_security_constraints = false)
      : memory_(static_cast<uint8_t*>(memory)),
        size_(base::bits::AlignDown(size, kDefaultAlignment)),
        options_(options),
        enable_security_constraints_(enable_security_constraints) {
    memory_end_ = memory_ + size_;
    AssertAlignment(memory_, BufferAlignment());
  }

  ~PaintOpWriter();

  template <typename T = char>
  static std::unique_ptr<T, base::AlignedFreeDeleter> AllocateAlignedBuffer(
      size_t size) {
    return std::unique_ptr<T, base::AlignedFreeDeleter>(
        static_cast<T*>(base::AlignedAlloc(size, kMaxAlignment)));
  }

  const PaintOp::SerializeOptions& options() const { return options_; }

  // Type and serialized_size fit in kHeaderBytes, using 1 byte and 3 bytes,
  // respectively. Note that serialized_size in the header is different from
  // PaintOp::aligned_size because serialized data may have different byte
  // format and serialization of reference data fields may be make
  // serialized_size much bigger than PaintOp::aligned_size.
  static constexpr size_t kHeaderBytes = sizeof(uint32_t);
  static constexpr size_t kMaxSerializedSize = (1u << 24) - 1;

  // The start/end of the buffer for a serialized PaintOp must be aligned to
  // BufferAlignment() which is the maximum alignment of all serialized fields,
  // to ensure the alignment padding of any field to be constant.
  //
  // When enable_security_constraints is true, we won't serialize PaintRecords
  // or images that require alignments greater than kDefaultAlignment. We can't
  // require larger alignment because the buffer may be a part of another
  // buffer (e.g. mojom data) for which the caller can't control the alignment.
  //
  // When enable_security_constraints is false, the alignment is 16 which is
  // the maximum alignment requirement of particular types of pixmaps (see
  // image_transfer_data_cache.cc).
  static constexpr size_t BufferAlignment(bool enable_security_constraints) {
    return enable_security_constraints ? kDefaultAlignment : kMaxAlignment;
  }
  static constexpr size_t kMaxAlignment = 16;
  size_t BufferAlignment() const {
    return BufferAlignment(enable_security_constraints_);
  }

  // Round up each field to 4 bytes by default. This is not technically perfect
  // alignment, but it is about 30% faster to post-align each write to 4 bytes
  // than it is to pre-align memory to the correct alignment.
  // A field can also use a larger alignment by calling AlignMemory().
  static constexpr size_t kDefaultAlignment = alignof(uint32_t);

 private:
  template <typename T>
  static constexpr size_t SerializedSizeSimple();

 public:
  // SerializedSize() returns the maximum serialized size of the given type or
  // the given parameter. For a buffer to contain serialization of multiple
  // data, the size can be the accumulated results of SerializedSize() of each
  // data. When possible, the parameterized version should be used to make it
  // easier to keep serialized size calculation in sync with serialization and
  // deserialization, and make it possible to allow dynamic sizing for some
  // data types (see the specialized/overloaded functions).
  template <typename T>
  static constexpr size_t SerializedSize();
  template <typename T>
  static constexpr size_t SerializedSize(const T& data);
  static size_t SerializedSize(const PaintImage& image);
  static size_t SerializedSize(const PaintRecord& record);
  static size_t SerializedSize(const SkHighContrastConfig& config);

  // Serialization of raw/smart pointers is not supported by default.
  template <typename T>
  static inline size_t SerializedSize(const T* p);
  template <typename T>
  static inline size_t SerializedSize(const std::unique_ptr<T>& p);
  template <typename T>
  static inline size_t SerializedSize(const scoped_refptr<T>& p);
  template <typename T>
  static inline size_t SerializedSize(const raw_ptr<T>& p);

  template <typename T>
  static inline size_t SerializedSize(T* p) {
    return SerializedSize(static_cast<const T*>(p));
  }
  static size_t SerializedSize(const SkColorSpace* color_space);
  static size_t SerializedSize(const gfx::HDRMetadata& hdr_metadata);
  static size_t SerializedSize(const SkGainmapInfo& gainmap_info);
  static size_t SerializedSize(const ColorFilter* filter);
  static size_t SerializedSize(const DrawLooper* looper);
  static size_t SerializedSize(const PaintFilter* filter);
  static size_t SerializedSize(const PathEffect* effect);

  template <typename T>
  static size_t SerializedSize(const std::optional<T>& o) {
    if (o) {
      return (base::CheckedNumeric<size_t>(SerializedSize<bool>()) +
              SerializedSize<T>(*o))
          .ValueOrDie();
    }
    return SerializedSize<bool>();
  }

  // Size of serialized (size_t, bytes).
  static size_t SerializedSizeOfBytes(size_t num_bytes) {
    return (base::CheckedNumeric<size_t>(SerializedSize<size_t>()) +
            base::bits::AlignUp(num_bytes, kDefaultAlignment))
        .ValueOrDie();
  }
  // Size of serialized (size_t, elements>
  template <typename T>
  static size_t SerializedSizeOfElements(const T* elements, size_t count) {
    return (SerializedSize<size_t>() +
            base::CheckedNumeric<size_t>(count) * SerializedSize(*elements))
        .ValueOrDie();
  }

  // These two functions should be called before and after (respectively)
  // serializing the data of a PaintOp. These functions should not be called
  // if this PaintOpWriter is used to write specific data instead of a whole
  // PaintOp.
  void ReserveOpHeader() {
    // Pretend we have written the header to leave a space for the header.
    DCHECK_GE(size_, kHeaderBytes);
    DidWrite(kHeaderBytes);
  }

  // Returns the serialized size (aligned to BufferAlignment()) of the PaintOp,
  // or 0 on any errors.
  size_t FinishOp(uint8_t type);

  static void WriteHeaderForTesting(void* memory,
                                    uint8_t type,
                                    size_t serialized_size);

  // Write a sequence of arbitrary bytes.
  void WriteData(size_t bytes, const void* input);

  // Returns the size of successfully written data, including paddings for
  // alignment.
  size_t size() const { return valid_ ? size_ - remaining_bytes() : 0u; }

  // Writes a size_t.
  // Note that size_t is always serialized as two uint32_ts to make the
  // serialized result portable between 32bit and 64bit processes.
  void WriteSize(size_t size);
  void Write(SkScalar data) { WriteSimple(data); }
  void Write(SkMatrix matrix);
  void Write(const SkM44& matrix);
  void Write(uint8_t data) { WriteSimple(data); }
  void Write(uint32_t data) { WriteSimple(data); }
  void Write(int32_t data) { WriteSimple(data); }
  void Write(const SkRect& rect) { WriteSimple(rect); }
  void Write(const SkIRect& rect) { WriteSimple(rect); }
  void Write(const SkRRect& rect) { WriteSimple(rect); }
  void Write(const SkColor4f& color) { WriteSimple(color); }
  void Write(const SkPath& path, UsePaintCache);
  void Write(const sk_sp<SkData>& data);
  void Write(const SkColorSpace* data);
  void Write(const SkGainmapInfo& gainmap_info);
  void Write(const SkSamplingOptions&);
  void Write(const sk_sp<sktext::gpu::Slug>& slug);
  void Write(SkYUVColorSpace yuv_color_space);
  void Write(SkYUVAInfo::PlaneConfig plane_config);
  void Write(SkYUVAInfo::Subsampling subsampling);
  void Write(const gpu::Mailbox& mailbox);
  void Write(const SkHighContrastConfig& config);
  void Write(const SkGradientShader::Interpolation& interpolation);

  // Shaders and filters need to know the current transform in order to lock in
  // the scale factor they will be evaluated at after deserialization. This is
  // critical to ensure that nested PaintRecords are analyzed and rasterized
  // identically when text is involved.
  void Write(const PaintFlags& flags, const SkM44& current_ctm);
  void Write(const PaintShader* shader,
             PaintFlags::FilterQuality quality,
             const SkM44& current_ctm);
  void Write(const ColorFilter* filter);
  void Write(const DrawLooper* looper);
  void Write(const PaintFilter* filter, const SkM44& current_ctm);
  void Write(const sk_sp<PaintFilter> filter, const SkM44& current_ctm) {
    Write(filter.get(), current_ctm);
  }
  void Write(const PathEffect* effect);
  void Write(const gfx::HDRMetadata& hdr_metadata);

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

  // Aligns the memory to the given `alignment` which must be within the range
  // of [kDefaultAlignment, BufferAlignment()].
  void AlignMemory(size_t alignment);

  static void AssertAlignment(const volatile uint8_t* memory,
                              size_t alignment) {
#if DCHECK_IS_ON()
    DCHECK_EQ(memory, base::bits::AlignUp(memory, alignment));
#endif
  }
  void AssertFieldAlignment() {
#if DCHECK_IS_ON()
    AssertAlignment(memory_, kDefaultAlignment);
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

  // Faster than a series of WriteSimple() calls, because it can amortize
  // the size checks and pointer maintenance over many values written.
  // Note that the types must really be simple; prefer Write() over
  // WriteSimpleMultiple() if you are not performance-constrained and/or
  // not sure if the types are simple. In particular, size_t has its own
  // convention through WriteSize(), and if you want to write a size_t
  // using WriteSimpleMultiple(), you'll need to implement that convention
  // yourself.
  template <typename... Types>
  ALWAYS_INLINE void WriteSimpleMultiple(Types... vals) {
    AssertFieldAlignment();
    static constexpr size_t total_size =
        (base::bits::AlignUp(sizeof(vals), kDefaultAlignment) + ...);

    EnsureBytes(total_size);
    if (!valid_) {
      return;
    }

    // The pattern ([&]{ code(vals) } (), ...) evaluates code() over
    // each element of `vals` in turn. It is similar to the use of + ...
    // above (the comma followed by ... generates a fold expression).
    // Note that `vals` on the inside of the fold expression refers to
    // one specific value.
    uint8_t* ptr = memory_;
    (
        [&] {
          static_assert(std::is_trivially_copyable_v<decltype(vals)>);
          reinterpret_cast<decltype(vals)*>(ptr)[0] = vals;
          ptr += base::bits::AlignUp(sizeof(vals), kDefaultAlignment);
        }(),
        ...);

    DidWrite(total_size);
  }

  template <typename T>
    requires(std::is_trivially_copyable_v<T>)
  void Write(const std::vector<T>& vec) {
    WriteSize(vec.size());
    WriteData(vec.size() * sizeof(T), vec.data());
  }

  template <typename T, typename... Args>
    requires(!std::is_trivially_copyable_v<T>)
  void Write(const std::vector<T>& vec, const Args&... args) {
    WriteSize(vec.size());
    for (const T& t : vec) {
      Write(t, args...);
    }
  }

 private:
  template <typename T>
  void WriteSimple(const T& val) {
    static_assert(std::is_trivially_copyable_v<T>);

    AssertFieldAlignment();
    static constexpr size_t size =
        base::bits::AlignUp(sizeof(T), kDefaultAlignment);
    EnsureBytes(size);
    if (!valid_) {
      return;
    }

    reinterpret_cast<T*>(memory_)[0] = val;

    memory_ += size;
    AssertFieldAlignment();
  }

  template <typename Enum>
  void WriteEnum(Enum value) {
    Write(base::checked_cast<uint8_t>(value));
  }

  // The following sequence is used when the size is unknown before writing
  // some data:
  //   void* memory = SkipSize();
  //   size_t data_size = WriteSomeData();
  //   WriteSizeAt(memory, data_size);
  void* SkipSize();
  void WriteSizeAt(void* memory, size_t size);

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

  void Write(const PaintRecord& record,
             const gfx::Rect& playback_rect,
             const gfx::SizeF& post_scale);
  void Write(const SkRegion& region);
  void WriteImage(const DecodedDrawImage& decoded_draw_image,
                  bool reinterpret_as_srgb);
  void WriteImage(uint32_t transfer_cache_entry_id, bool needs_mips);
  void WriteImage(const gpu::Mailbox& mailbox, bool reinterpret_as_srgb);
  void DidWrite(size_t bytes_written) {
    // All data are aligned with kDefaultAlignment at least.
    size_t aligned_bytes =
        base::bits::AlignUp(bytes_written, kDefaultAlignment);
    DCHECK_LE(aligned_bytes, remaining_bytes());
    memory_ += aligned_bytes;
  }
  void EnsureBytes(size_t required_bytes) {
    if (remaining_bytes() < required_bytes) {
      valid_ = false;
    }
  }
  size_t remaining_bytes() const {
    DCHECK_LE(memory_, memory_end_);
    return memory_end_ - memory_;
  }
  sk_sp<PaintShader> TransformShaderIfNecessary(
      const PaintShader* original,
      PaintFlags::FilterQuality quality,
      const SkM44& current_ctm,
      uint32_t* paint_image_transfer_cache_entry_id,
      gfx::SizeF* paint_record_post_scale,
      bool* paint_image_needs_mips,
      gpu::Mailbox* mailbox_out);

  uint8_t* memory_ = nullptr;
  const uint8_t* memory_end_ = nullptr;
  size_t size_ = 0u;
  const PaintOp::SerializeOptions& options_;
  bool valid_ = true;

  // Indicates that the following security constraints must be applied during
  // serialization:
  // 1) PaintRecords and DrawLoopers must be ignored.
  // 2) Codec backed images must be decoded and only the bitmap should be
  // serialized.
  const bool enable_security_constraints_;
};

template <typename T>
constexpr size_t PaintOpWriter::SerializedSizeSimple() {
  static_assert(!std::is_pointer_v<T>);
  return base::bits::AlignUp(sizeof(T), kDefaultAlignment);
}

// size_t is always serialized as two uint32_ts to make the serialized result
// portable between 32bit and 64bit processes.
template <>
constexpr size_t PaintOpWriter::SerializedSizeSimple<size_t>() {
  return base::bits::AlignUp(2 * sizeof(uint32_t), kDefaultAlignment);
}

template <typename T>
constexpr size_t PaintOpWriter::SerializedSize() {
  static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
  return SerializedSizeSimple<T>();
}
template <typename T>
constexpr size_t PaintOpWriter::SerializedSize(const T& data) {
  return SerializedSizeSimple<T>();
}

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_WRITER_H_
