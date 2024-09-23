// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_READER_H_
#define CC_PAINT_PAINT_OP_READER_H_

#include <optional>
#include <type_traits>
#include <vector>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "cc/paint/draw_looper.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/transfer_cache_deserialize_helper.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

struct SkGainmapInfo;
struct SkHighContrastConfig;
class SkColorSpace;
namespace sktext::gpu {
class Slug;
}

namespace gpu {
struct Mailbox;
}

namespace gfx {
struct HDRMetadata;
}

namespace cc {

class PaintShader;
class PathEffect;
class SkottieWrapper;

// PaintOpReader takes garbage |memory| and clobbers it with successive
// read functions.
class CC_PAINT_EXPORT PaintOpReader {
  STACK_ALLOCATED();

 public:
  // The DeserializeOptions passed to the reader must set all fields if it can
  // be used to for deserializing images, paint records or text blobs.
  // See PaintOpWriter for the alignment requirement for `memory`.
  PaintOpReader(const volatile void* memory,
                size_t size,
                const PaintOp::DeserializeOptions& options,
                bool enable_security_constraints = false);

  static void FixupMatrixPostSerialization(SkMatrix* matrix);

  // This should be called before before reading PaintOp data. This function
  // should not be called if this PaintOpReader is used to read specific data
  // instead of a whole PaintOp.
  bool ReadAndValidateOpHeader(uint8_t* op_type, size_t* serialized_size);

  size_t BufferAlignment() const {
    return PaintOpWriter::BufferAlignment(enable_security_constraints_);
  }

  bool valid() const { return valid_; }
  size_t remaining_bytes() const { return remaining_bytes_; }

  void ReadData(base::span<uint8_t> data);
  void ReadSize(size_t* size);

  void Read(SkScalar* data);
  void Read(uint8_t* data);
  void Read(uint32_t* data);
  void Read(uint64_t* data);
  void Read(int32_t* data);
  void Read(SkRect* rect);
  void Read(SkIRect* rect);
  void Read(SkRRect* rect);
  void Read(SkColor4f* color);

  void Read(SkPath* path);
  void Read(PaintFlags* flags);
  void Read(CorePaintFlags* flags);
  void Read(PaintImage* image,
            PaintFlags::DynamicRangeLimitMixture dynamic_range_limit);
  void Read(sk_sp<SkData>* data);
  void Read(sk_sp<sktext::gpu::Slug>* slug);
  void Read(sk_sp<DrawLooper>* looper);
  void Read(sk_sp<PaintFilter>* filter);
  void Read(sk_sp<PaintShader>* shader);
  void Read(SkMatrix* matrix);
  void Read(SkM44* matrix);
  void Read(SkImageInfo* info);
  void Read(SkSamplingOptions* sampling);
  void Read(sk_sp<SkColorSpace>* color_space);
  void Read(SkGainmapInfo* gainmap_info);
  void Read(SkYUVColorSpace* yuv_color_space);
  void Read(SkYUVAInfo::PlaneConfig* plane_config);
  void Read(SkYUVAInfo::Subsampling* subsampling);
  void Read(gpu::Mailbox* mailbox);
  void Read(SkHighContrastConfig* config);
  void Read(gfx::HDRMetadata* hdr_metadata);
  void Read(SkGradientShader::Interpolation* interpolation);

  void Read(scoped_refptr<SkottieWrapper>* skottie);

  void Read(SkClipOp* op) { ReadEnum<SkClipOp, SkClipOp::kMax_EnumValue>(op); }
  void Read(PaintCanvas::AnnotationType* type) {
    ReadEnum<PaintCanvas::AnnotationType,
             PaintCanvas::AnnotationType::kLinkToDestination>(type);
  }
  void Read(SkCanvas::SrcRectConstraint* constraint) {
    ReadEnum<SkCanvas::SrcRectConstraint, SkCanvas::kFast_SrcRectConstraint>(
        constraint);
  }
  void Read(SkColorType* color_type) {
    ReadEnum<SkColorType, kLastEnum_SkColorType>(color_type);
  }
  void Read(PaintFlags::FilterQuality* quality) {
    ReadEnum<PaintFlags::FilterQuality, PaintFlags::FilterQuality::kLast>(
        quality);
  }
  void Read(SkBlendMode* blend_mode) {
    ReadEnum<SkBlendMode, SkBlendMode::kLastMode>(blend_mode);
  }
  void Read(SkTileMode* tile_mode) {
    ReadEnum<SkTileMode, SkTileMode::kLastTileMode>(tile_mode);
  }
  void Read(SkFilterMode* filter_mode) {
    ReadEnum<SkFilterMode, SkFilterMode::kLast>(filter_mode);
  }
  void Read(SkMipmapMode* mipmap_mode) {
    ReadEnum<SkMipmapMode, SkMipmapMode::kLast>(mipmap_mode);
  }

  void Read(bool* data) {
    uint8_t value = 0u;
    Read(&value);
    *data = !!value;
  }

  // Returns true if there is enough data to read for the specified vector. If
  // there is not enough data, the PaintOpReader is marked invalid.
  template <typename T>
  bool CanReadVector(size_t size, const std::vector<T>& vec) {
    if (size > vec.max_size() || remaining_bytes_ < size * sizeof(T))
        [[unlikely]] {
      SetInvalid(DeserializationError::kInsufficientRemainingBytes_ReadData);
      return false;
    }
    return true;
  }

  template <typename T>
  void Read(std::vector<T>* vec) {
    size_t size = 0;
    ReadSize(&size);
    if (!CanReadVector(size, *vec)) [[unlikely]] {
      return;
    }
    ReadVectorContent(size, vec);
  }

  // Returns a pointer to the next block of memory of size |bytes|, and treats
  // this memory as read (advancing the reader). Returns nullptr if |bytes|
  // would exceed the available budfer.
  const volatile void* ExtractReadableMemory(size_t bytes);

  // Aligns the memory to the given `alignment` which must be within the range
  // of [PaintOpWriter::kDefaultAlignment, BufferAlignment()].
  void AlignMemory(size_t alignment);

  void AssertFieldAlignment() {
#if DCHECK_IS_ON()
    PaintOpWriter::AssertAlignment(memory_, PaintOpWriter::kDefaultAlignment);
#endif
  }

 private:
  enum class DeserializationError {
    // Enum values must remain synchronized with PaintOpDeserializationError
    // in tools/metrics/histograms/metadata/gpu/enums.xml.
    kDrawLooperForbidden = 0,
    kEnumValueOutOfRange = 1,
    kForbiddenSerializedImageType = 2,
    kInsufficientRemainingBytes_AlignMemory = 3,
    kInsufficientRemainingBytes_ExtractReadableMemory = 4,
    kInsufficientRemainingBytes_Read_PaintRecord = 5,
    kInsufficientRemainingBytes_Read_PaintShader_ColorBytes = 6,
    kInsufficientRemainingBytes_Read_PaintShader_ColorSize = 7,
    kInsufficientRemainingBytes_Read_PaintShader_Positions = 8,
    kInsufficientRemainingBytes_Read_SkData = 9,
    kInsufficientRemainingBytes_Read_SkPath = 10,
    kInsufficientRemainingBytes_Read_SkRegion = 11,
    kInsufficientRemainingBytes_Read_Slug = 12,
    kInsufficientRemainingBytes_ReadData = 13,
    kInsufficientRemainingBytes_ReadFlattenable = 14,
    kInsufficientRemainingBytes_ReadMatrixConvolutionPaintFilter = 15,
    kInsufficientRemainingBytes_ReadSimple = 16,
    kInvalidPaintShader = 17,
    kInvalidPaintShaderPositionsSize = 18,
    kInvalidPaintShaderScalingBehavior = 19,
    kInvalidPaintShaderType = 20,
    kInvalidPlaneConfig = 21,
    kInvalidRasterScale = 22,
    kInvalidRecordShaderId = 23,
    kInvalidSerializedImageType = 24,
    kInvalidSkYUVColorSpace = 25,
    kInvalidSubsampling = 26,
    kInvalidTypeface = 27,
    kMissingPaintCachePathEntry = 28,
    kMissingPaintCacheTextBlobEntry = 29,
    kMissingSharedImageProvider = 30,
    kPaintFilterHasTooManyInputs = 31,
    kPaintOpBufferMakeFromMemoryFailure = 32,
    kPaintRecordForbidden = 33,
    kReadImageFailure = 34,
    kSharedImageOpenFailure = 35,  // Obsolete
    kSkColorFilterUnflattenFailure = 36,
    kSkColorSpaceDeserializeFailure = 37,
    kSkDrawLooperUnflattenFailure = 38,  // Obsolete
    kSkMaskFilterUnflattenFailure = 39,
    kSkPathEffectUnflattenFailure = 40,
    kSkPathReadFromMemoryFailure = 41,
    kSkRegionReadFromMemoryFailure = 42,
    kSlugDeserializeFailure = 43,
    kUnexpectedPaintShaderType = 44,
    kUnexpectedSerializedImageType = 45,
    kZeroMailbox = 46,
    kZeroRegionBytes = 47,
    kZeroSkPathBytes = 48,
    kSharedImageProviderUnknownMailbox = 49,
    kSharedImageProviderNoAccess = 50,
    kSharedImageProviderSkImageCreationFailed = 51,
    kZeroSkColorFilterBytes = 52,
    kInsufficientPixelData = 53,
    kSkGainmapInfoDeserializationFailure = 54,
    kHdrMetadataDeserializeFailure = 55,
    kNonFiniteSkColor4f = 56,
    kInvalidSkColor4fAlpha = 57,

    kMaxValue = kInvalidSkColor4fAlpha
  };

  template <typename T>
  void ReadSimple(T* val);

  template <typename T>
  using Factory = sk_sp<T> (*)(const void* data,
                               size_t size,
                               const SkDeserialProcs* procs);

  template <typename Enum, Enum kMaxValue = Enum::kMaxValue>
  void ReadEnum(Enum* enum_value) {
    static_assert(static_cast<unsigned>(kMaxValue) <= 255,
                  "Max value must fit in uint8_t");
    uint8_t value = 0u;
    Read(&value);
    if (value > static_cast<uint8_t>(kMaxValue)) {
      SetInvalid(DeserializationError::kEnumValueOutOfRange);
      return;
    }
    *enum_value = static_cast<Enum>(value);
  }

  void SetInvalid(DeserializationError error);

  void Read(sk_sp<ColorFilter>* filter);
  void Read(sk_sp<PathEffect>* effect);

  // The main entry point is Read(sk_sp<PaintFilter>* filter) which calls one of
  // the following functions depending on read type.
  void ReadColorFilterPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadBlurPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadDropShadowPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadMagnifierPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadComposePaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadAlphaThresholdPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadXfermodePaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadArithmeticPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadMatrixConvolutionPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadDisplacementMapEffectPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadImagePaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadRecordPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadMergePaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadMorphologyPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadOffsetPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadTilePaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadTurbulencePaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadShaderPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadMatrixPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadLightingDistantPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadLightingPointPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);
  void ReadLightingSpotPaintFilter(
      sk_sp<PaintFilter>* filter,
      const std::optional<PaintFilter::CropRect>& crop_rect);

  // Returns the size of the read record, 0 if error.
  size_t Read(std::optional<PaintRecord>* record);

  void Read(SkRegion* region);
  uint8_t* CopyScratchSpace(size_t bytes);
  void DidRead(size_t bytes_read);

  template <typename T>
    requires(std::is_trivially_copyable_v<T>)
  void ReadVectorContent(size_t size, std::vector<T>* vec) {
    vec->resize(size);
    ReadData(base::as_writable_byte_span(*vec));
  }

  template <typename T>
    requires(!std::is_trivially_copyable_v<T>)
  void ReadVectorContent(size_t size, std::vector<T>* vec) {
    vec->resize(size);
    for (size_t i = 0; i < size; ++i) {
      Read(&(*vec)[i]);
    }
  }

  const volatile uint8_t* memory_ = nullptr;
  size_t remaining_bytes_ = 0u;
  bool valid_ = true;
  const PaintOp::DeserializeOptions& options_;

  // Indicates that the data was serialized with the following constraints:
  // 1) PaintRecords and SkDrawLoopers are ignored.
  // 2) Images are decoded and only the bitmap is serialized.
  // If set to true, the above constraints are validated during deserialization
  // and the data types specified above are ignored.
  const bool enable_security_constraints_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_READER_H_
