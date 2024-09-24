// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_op_reader.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "cc/base/features.h"
#include "cc/paint/color_filter.h"
#include "cc/paint/draw_looper.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/path_effect.h"
#include "cc/paint/shader_transfer_cache_entry.h"
#include "cc/paint/skottie_transfer_cache_entry.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/paint/transfer_cache_deserialize_helper.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "third_party/skia/include/private/chromium/Slug.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace cc {
namespace {

bool IsValidPaintShaderType(PaintShader::Type type) {
  return static_cast<uint8_t>(type) <
         static_cast<uint8_t>(PaintShader::Type::kShaderCount);
}

bool IsValidPaintShaderScalingBehavior(PaintShader::ScalingBehavior behavior) {
  return behavior == PaintShader::ScalingBehavior::kRasterAtScale ||
         behavior == PaintShader::ScalingBehavior::kFixedScale;
}

float ComputeHdrHeadroom(
    const PaintFlags::DynamicRangeLimitMixture& dynamic_range_limit,
    float target_hdr_headroom) {
  const float dynamic_range_high_mix =
      1.f - dynamic_range_limit.constrained_high_mix -
      dynamic_range_limit.standard_mix;
  float hdr_headroom = 1.f;
  if (dynamic_range_limit.constrained_high_mix > 0) {
    hdr_headroom *= std::pow(std::min(2.f, target_hdr_headroom),
                             dynamic_range_limit.constrained_high_mix);
  }
  if (dynamic_range_high_mix > 0) {
    hdr_headroom *= std::pow(target_hdr_headroom, dynamic_range_high_mix);
  }
  return hdr_headroom;
}

}  // namespace

PaintOpReader::PaintOpReader(const volatile void* memory,
                             size_t size,
                             const PaintOp::DeserializeOptions& options,
                             bool enable_security_constraints)
    : memory_(static_cast<const volatile uint8_t*>(memory)),
      remaining_bytes_(
          base::bits::AlignDown(size, PaintOpWriter::kDefaultAlignment)),
      options_(options),
      enable_security_constraints_(enable_security_constraints) {
  PaintOpWriter::AssertAlignment(memory_, BufferAlignment());
}

// static
void PaintOpReader::FixupMatrixPostSerialization(SkMatrix* matrix) {
  // Can't trust malicious clients to provide the correct derived matrix type.
  // However, if a matrix thinks that it's identity, then make it so.
  if (matrix->isIdentity())
    matrix->setIdentity();
  else
    matrix->dirtyMatrixTypeCache();
}

bool PaintOpReader::ReadAndValidateOpHeader(uint8_t* type,
                                            size_t* serialized_size) {
  static_assert(PaintOpWriter::kHeaderBytes == sizeof(uint32_t));
  uint32_t header;
  Read(&header);
  if (!valid_) {
    return false;
  }
  *type = static_cast<uint8_t>(header & 0xFF);
  *serialized_size = header >> 8;

  size_t remaining_op_bytes = *serialized_size - PaintOpWriter::kHeaderBytes;
  if (remaining_bytes_ < remaining_op_bytes) {
    return false;
  }
  remaining_bytes_ = remaining_op_bytes;

  if (*serialized_size % BufferAlignment() != 0) {
    return false;
  }
  if (*type > static_cast<uint8_t>(PaintOpType::kLastPaintOpType)) {
    return false;
  }
  return true;
}

template <typename T>
void PaintOpReader::ReadSimple(T* val) {
  static_assert(std::is_trivially_copyable_v<T>);
  // Serialized values other than 0 or 1 can cause different behavior in debug
  // and release, making debugging harder. Please use Read(bool*).
  static_assert(!std::is_same_v<T, bool>);

  AssertFieldAlignment();
  // Align everything to 4 bytes, as the writer does.
  static constexpr size_t size =
      base::bits::AlignUp(sizeof(T), PaintOpWriter::kDefaultAlignment);

  if (remaining_bytes_ < size)
    SetInvalid(DeserializationError::kInsufficientRemainingBytes_ReadSimple);

  if (!valid_)
    return;

  // Most of the time this is used for primitives, but this function is also
  // used for SkRect/SkIRect/SkMatrix whose implicit operator= can't use a
  // volatile.  TOCTOU violations don't matter for these simple types so
  // use assignment.
  *val = *reinterpret_cast<const T*>(const_cast<const uint8_t*>(memory_));

  memory_ += size;
  remaining_bytes_ -= size;
  AssertFieldAlignment();
}

uint8_t* PaintOpReader::CopyScratchSpace(size_t bytes) {
  DCHECK(SkIsAlign4(reinterpret_cast<uintptr_t>(memory_)));

  if (options_.scratch_buffer.size() < bytes) {
    options_.scratch_buffer.resize(bytes);
  }
  memcpy(options_.scratch_buffer.data(), const_cast<const uint8_t*>(memory_),
         bytes);
  return options_.scratch_buffer.data();
}

void PaintOpReader::ReadData(base::span<uint8_t> data) {
  AssertFieldAlignment();
  if (data.size() == 0) {
    return;
  }

  if (remaining_bytes_ < data.size()) {
    SetInvalid(DeserializationError::kInsufficientRemainingBytes_ReadData);
    return;
  }

  memcpy(data.data(), const_cast<const uint8_t*>(memory_), data.size());
  DidRead(data.size());
}

void PaintOpReader::ReadSize(size_t* size) {
  // size_t is always serialized as uint64_t to make the serialized result
  // portable between 32bit and 64bit processes.
  uint32_t lo = 0u;
  uint32_t hi = 0u;
  ReadSimple(&lo);
  ReadSimple(&hi);

  // size_t is always aligned to only 4 bytes. Avoid undefined behavior by
  // reading as two uint32_ts and combining the result.
  // https://crbug.com/1429994
  uint64_t size64 = static_cast<uint64_t>(hi) << 32 | lo;
  if (!base::IsValueInRangeForNumericType<size_t>(size64)) {
    valid_ = false;
    return;
  }
  *size = static_cast<size_t>(size64);
}

void PaintOpReader::Read(SkScalar* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(uint8_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(uint32_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(uint64_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(int32_t* data) {
  ReadSimple(data);
}

void PaintOpReader::Read(SkRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkIRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkRRect* rect) {
  ReadSimple(rect);
}

void PaintOpReader::Read(SkColor4f* color) {
  ReadSimple(color);
  if (!valid_) {
    return;
  }

  // Colors are generally [0, 1], sometimes with a wider gamut, but
  // infinite and NaN colors don't make sense and shouldn't be produced by a
  // renderer, so encountering a non-finite color implies the paint op buffer
  // is invalid.
  if (!std::isfinite(color->fR) || !std::isfinite(color->fG) ||
      !std::isfinite(color->fB) || !std::isfinite(color->fA)) {
    SetInvalid(DeserializationError::kNonFiniteSkColor4f);
    return;
  }

  // Alpha outside [0, 1] is considered invalid.
  if (color->fA < 0.0f || 1.0f < color->fA) {
    SetInvalid(DeserializationError::kInvalidSkColor4fAlpha);
    return;
  }
}

void PaintOpReader::Read(SkPath* path) {
  uint32_t path_id;
  ReadSimple(&path_id);
  if (!valid_)
    return;

  uint32_t entry_state_int = 0u;
  ReadSimple(&entry_state_int);
  if (entry_state_int > static_cast<uint32_t>(PaintCacheEntryState::kLast)) {
    valid_ = false;
    return;
  }

  auto entry_state = static_cast<PaintCacheEntryState>(entry_state_int);
  switch (entry_state) {
    case PaintCacheEntryState::kEmpty:
      return;
    case PaintCacheEntryState::kCached:
      if (!options_.paint_cache->GetPath(path_id, path)) {
        SetInvalid(DeserializationError::kMissingPaintCachePathEntry);
      }
      return;
    case PaintCacheEntryState::kInlined:
    case PaintCacheEntryState::kInlinedDoNotCache: {
      size_t path_bytes = 0u;
      ReadSize(&path_bytes);
      if (path_bytes > remaining_bytes_)
        SetInvalid(
            DeserializationError::kInsufficientRemainingBytes_Read_SkPath);
      if (path_bytes == 0u)
        SetInvalid(DeserializationError::kZeroSkPathBytes);
      if (!valid_)
        return;

      auto* scratch = CopyScratchSpace(path_bytes);
      size_t bytes_read = path->readFromMemory(scratch, path_bytes);
      if (bytes_read == 0u) {
        SetInvalid(DeserializationError::kSkPathReadFromMemoryFailure);
        return;
      }
      if (entry_state == PaintCacheEntryState::kInlined) {
        options_.paint_cache->PutPath(path_id, *path);
      } else {
        // If we know that this path will only be drawn once, which is
        // implied by kInlinedDoNotCache, we signal to skia that it should not
        // do any caching either.
        path->setIsVolatile(true);
      }
      DidRead(path_bytes);
      return;
    }
  }
}

void PaintOpReader::Read(PaintFlags* flags) {
  Read(&flags->color_);
  Read(&flags->width_);
  Read(&flags->miter_limit_);

  ReadSimple(&flags->bitfields_uint_);

  Read(&flags->path_effect_);
  Read(&flags->color_filter_);

  if (enable_security_constraints_) {
    bool has_looper = false;
    Read(&has_looper);
    if (has_looper) {
      SetInvalid(DeserializationError::kDrawLooperForbidden);
      return;
    }
  } else {
    Read(&flags->draw_looper_);
  }

  Read(&flags->image_filter_);
  Read(&flags->shader_);
}

void PaintOpReader::Read(CorePaintFlags* flags) {
  Read(&flags->color_);
  Read(&flags->width_);
  Read(&flags->miter_limit_);
  ReadSimple(&flags->bitfields_uint_);
}

void PaintOpReader::Read(
    PaintImage* image,
    PaintFlags::DynamicRangeLimitMixture dynamic_range_limit) {
  uint8_t serialized_type_int = 0u;
  Read(&serialized_type_int);
  if (serialized_type_int >
      static_cast<uint8_t>(PaintOp::SerializedImageType::kLastType)) {
    SetInvalid(DeserializationError::kInvalidSerializedImageType);
    return;
  }

  auto serialized_type =
      static_cast<PaintOp::SerializedImageType>(serialized_type_int);
  if (serialized_type == PaintOp::SerializedImageType::kNoImage)
    return;

  // Compute the HDR headroom for tone mapping.
  const float hdr_headroom =
      ComputeHdrHeadroom(dynamic_range_limit, options_.hdr_headroom);

  if (enable_security_constraints_) {
    switch (serialized_type) {
      case PaintOp::SerializedImageType::kNoImage:
        NOTREACHED();
      case PaintOp::SerializedImageType::kImageData: {
        SkColorType color_type;
        Read(&color_type);
        if (!valid_) {
          return;
        }
        // Color types requiring alignment larger than kDefaultAlignment is not
        // supported.
        if (static_cast<size_t>(SkColorTypeBytesPerPixel(color_type)) >
            PaintOpWriter::kDefaultAlignment) {
          SetInvalid(DeserializationError::kReadImageFailure);
          return;
        }
        uint32_t width;
        Read(&width);
        uint32_t height;
        Read(&height);
        size_t pixel_size;
        ReadSize(&pixel_size);
        if (!valid_)
          return;

        SkImageInfo image_info =
            SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
        if (pixel_size < image_info.computeMinByteSize()) {
          SetInvalid(DeserializationError::kInsufficientPixelData);
          return;
        }
        const volatile void* pixel_data = ExtractReadableMemory(pixel_size);
        if (!valid_)
          return;

        SkPixmap pixmap(image_info, const_cast<const void*>(pixel_data),
                        image_info.minRowBytes());

        *image = PaintImageBuilder::WithDefault()
                     .set_id(PaintImage::GetNextId())
                     .set_texture_image(SkImages::RasterFromPixmapCopy(pixmap),
                                        PaintImage::kNonLazyStableId)
                     .set_target_hdr_headroom(hdr_headroom)
                     .TakePaintImage();
      }
        return;
      case PaintOp::SerializedImageType::kTransferCacheEntry:
      case PaintOp::SerializedImageType::kMailbox:
        SetInvalid(DeserializationError::kForbiddenSerializedImageType);
        return;
    }

    NOTREACHED();
  }

  if (serialized_type == PaintOp::SerializedImageType::kMailbox) {
    if (!options_.shared_image_provider) {
      SetInvalid(DeserializationError::kMissingSharedImageProvider);
      return;
    }

    gpu::Mailbox mailbox;
    Read(&mailbox);
    if (mailbox.IsZero()) {
      SetInvalid(DeserializationError::kZeroMailbox);
      return;
    }

    bool reinterpret_as_srgb = 0;
    Read(&reinterpret_as_srgb);

    SharedImageProvider::Error error;
    sk_sp<SkImage> sk_image =
        options_.shared_image_provider->OpenSharedImageForRead(mailbox, error);
    if (error != SharedImageProvider::Error::kNoError) {
      switch (error) {
        case SharedImageProvider::Error::kNoAccess:
          SetInvalid(DeserializationError::kSharedImageProviderNoAccess);
          break;
        case SharedImageProvider::Error::kSkImageCreationFailed:
          SetInvalid(
              DeserializationError::kSharedImageProviderSkImageCreationFailed);
          break;
        case SharedImageProvider::Error::kUnknownMailbox:
          SetInvalid(DeserializationError::kSharedImageProviderUnknownMailbox);
          break;
        default:
          NOTREACHED();
      }
      SetInvalid(DeserializationError::kSharedImageOpenFailure);
      return;
    }
    DCHECK(sk_image);

    *image = PaintImageBuilder::WithDefault()
                 .set_id(PaintImage::GetNextId())
                 .set_texture_image(std::move(sk_image),
                                    PaintImage::kNonLazyStableId)
                 .set_target_hdr_headroom(hdr_headroom)
                 .set_reinterpret_as_srgb(reinterpret_as_srgb)
                 .TakePaintImage();
    return;
  }

  if (serialized_type != PaintOp::SerializedImageType::kTransferCacheEntry) {
    SetInvalid(DeserializationError::kUnexpectedSerializedImageType);
    return;
  }

  uint32_t transfer_cache_entry_id;
  ReadSimple(&transfer_cache_entry_id);
  if (!valid_)
    return;

  bool needs_mips;
  Read(&needs_mips);
  if (!valid_)
    return;

  // If we encountered a decode failure, we may write an invalid id for the
  // image. In these cases, just return, leaving the image as nullptr.
  if (transfer_cache_entry_id == kInvalidImageTransferCacheEntryId)
    return;

  // The transfer cache entry for an image may not exist if the upload fails.
  if (auto* entry =
          options_.transfer_cache->GetEntryAs<ServiceImageTransferCacheEntry>(
              transfer_cache_entry_id)) {
    if (needs_mips) {
      entry->EnsureMips();
    }

    PaintImageBuilder builder =
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_texture_image(entry->image(), PaintImage::kNonLazyStableId)
            .set_target_hdr_headroom(hdr_headroom);
    if (entry->HasGainmap()) {
      builder = std::move(builder).set_gainmap_texture_image(
          entry->gainmap_image(), entry->gainmap_info());
    }
    if (entry->hdr_metadata().has_value()) {
      builder = std::move(builder).set_hdr_metadata(entry->hdr_metadata());
    }
    *image = builder.TakePaintImage();
  }
}

void PaintOpReader::Read(sk_sp<SkData>* data) {
  size_t bytes = 0;
  ReadSize(&bytes);
  if (remaining_bytes_ < bytes)
    SetInvalid(DeserializationError::kInsufficientRemainingBytes_Read_SkData);
  if (!valid_)
    return;

  // Separate out empty vs not valid cases.
  if (bytes == 0) {
    bool has_data = false;
    Read(&has_data);
    if (has_data)
      *data = SkData::MakeEmpty();
    return;
  }

  // This is safe to cast away the volatile as it is just a memcpy internally.
  *data = SkData::MakeWithCopy(const_cast<const uint8_t*>(memory_), bytes);
  DidRead(bytes);
}

void PaintOpReader::Read(sk_sp<SkColorSpace>* color_space) {
  size_t size = 0;
  ReadSize(&size);
  if (remaining_bytes_ < size)
    valid_ = false;
  if (!valid_ || size == 0)
    return;

  auto* scratch = CopyScratchSpace(size);
  *color_space = SkColorSpace::Deserialize(scratch, size);
  // If this had non-zero bytes, it should be a valid color space.
  if (!color_space)
    SetInvalid(DeserializationError::kSkColorSpaceDeserializeFailure);

  DidRead(size);
}

void PaintOpReader::Read(SkGainmapInfo* gainmap_info) {
  Read(&gainmap_info->fGainmapRatioMin);
  Read(&gainmap_info->fGainmapRatioMax);
  Read(&gainmap_info->fGainmapGamma);
  Read(&gainmap_info->fEpsilonSdr);
  Read(&gainmap_info->fEpsilonHdr);
  Read(&gainmap_info->fDisplayRatioSdr);
  Read(&gainmap_info->fDisplayRatioHdr);
  Read(&gainmap_info->fDisplayRatioHdr);
  uint32_t base_image_type = 0;
  Read(&base_image_type);
  switch (base_image_type) {
    case 0:
      gainmap_info->fBaseImageType = SkGainmapInfo::BaseImageType::kSDR;
      break;
    case 1:
      gainmap_info->fBaseImageType = SkGainmapInfo::BaseImageType::kHDR;
      break;
    default:
      SetInvalid(DeserializationError::kSkGainmapInfoDeserializationFailure);
      break;
  }
  Read(&gainmap_info->fGainmapMathColorSpace);
}

void PaintOpReader::Read(sk_sp<sktext::gpu::Slug>* slug) {
  AssertFieldAlignment();

  size_t data_bytes = 0u;
  ReadSize(&data_bytes);
  if (data_bytes == 0) {
    *slug = nullptr;
    return;
  }

  if (remaining_bytes_ < data_bytes) {
    SetInvalid(DeserializationError::kInsufficientRemainingBytes_Read_Slug);
    return;
  }

  *slug = sktext::gpu::Slug::Deserialize(const_cast<const uint8_t*>(memory_),
                                         data_bytes, options_.strike_client);
  DidRead(data_bytes);

  if (!*slug) {
    SetInvalid(DeserializationError::kSlugDeserializeFailure);
    return;
  }
}

void PaintOpReader::Read(sk_sp<DrawLooper>* looper) {
  bool has_looper = false;
  Read(&has_looper);
  if (!has_looper) {
    *looper = nullptr;
    return;
  }

  DrawLooperBuilder builder;
  size_t count;
  ReadSize(&count);
  if (!valid_) {
    *looper = nullptr;
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    SkPoint offset;
    float blur_sigma;
    SkColor4f color;
    uint32_t flags;

    ReadSimple(&offset);
    ReadSimple(&blur_sigma);
    Read(&color);
    ReadSimple(&flags);
    if (!valid_) {
      *looper = nullptr;
      return;
    }
    builder.AddShadow(offset, blur_sigma, color, flags);
  }
  *looper = builder.Detach();
}

void PaintOpReader::Read(sk_sp<PaintShader>* shader) {
  bool has_shader = false;
  Read(&has_shader);
  if (!has_shader) {
    *shader = nullptr;
    return;
  }
  PaintShader::Type shader_type;
  ReadSimple(&shader_type);
  // Avoid creating a shader if something is invalid.
  if (!valid_ || !IsValidPaintShaderType(shader_type)) {
    SetInvalid(DeserializationError::kInvalidPaintShaderType);
    return;
  }
  if (enable_security_constraints_ &&
      shader_type == PaintShader::Type::kPaintRecord) {
    SetInvalid(DeserializationError::kPaintRecordForbidden);
    return;
  }

  *shader = sk_sp<PaintShader>(new PaintShader(shader_type));
  PaintShader& ref = **shader;
  ReadSimple(&ref.flags_);
  ReadSimple(&ref.end_radius_);
  ReadSimple(&ref.start_radius_);
  Read(&ref.tx_);
  Read(&ref.ty_);
  ReadSimple(&ref.fallback_color_);
  ReadSimple(&ref.scaling_behavior_);
  if (!IsValidPaintShaderScalingBehavior(ref.scaling_behavior_))
    SetInvalid(DeserializationError::kInvalidPaintShaderScalingBehavior);
  bool has_local_matrix = false;
  Read(&has_local_matrix);
  if (has_local_matrix) {
    ref.local_matrix_.emplace();
    Read(&*ref.local_matrix_);
  }
  ReadSimple(&ref.center_);
  ReadSimple(&ref.tile_);
  ReadSimple(&ref.start_point_);
  ReadSimple(&ref.end_point_);
  ReadSimple(&ref.start_degrees_);
  ReadSimple(&ref.end_degrees_);
  Read(&ref.gradient_interpolation_);
  Read(&ref.image_, PaintFlags::DynamicRangeLimitMixture(
                        PaintFlags::DynamicRangeLimit::kHigh));
  bool has_record = false;
  Read(&has_record);
  uint32_t shader_id = PaintShader::kInvalidRecordShaderId;
  size_t shader_size = 0;
  if (has_record) {
    if (shader_type != PaintShader::Type::kPaintRecord) {
      SetInvalid(DeserializationError::kUnexpectedPaintShaderType);
      return;
    }
    Read(&shader_id);
    if (shader_id == PaintShader::kInvalidRecordShaderId) {
      SetInvalid(DeserializationError::kInvalidRecordShaderId);
      return;
    }

    // Track dependent transfer cache entries to make cached shader size
    // more realistic.
    size_t pre_size = options_.transfer_cache->GetTotalEntrySizes();
    size_t record_size = Read(&ref.record_);
    size_t post_size = options_.transfer_cache->GetTotalEntrySizes();
    shader_size = post_size - pre_size + record_size;

    ref.id_ = shader_id;
  }
  decltype(ref.colors_)::size_type colors_size = 0;
  ReadSize(&colors_size);

  // If there are too many colors, abort.
  if (colors_size > remaining_bytes_) {
    SetInvalid(DeserializationError::
                   kInsufficientRemainingBytes_Read_PaintShader_ColorSize);
    return;
  }
  size_t colors_bytes =
      colors_size * (colors_size > 0 ? sizeof(ref.colors_[0]) : 0u);
  if (colors_bytes > remaining_bytes_) {
    SetInvalid(DeserializationError::
                   kInsufficientRemainingBytes_Read_PaintShader_ColorBytes);
    return;
  }
  ref.colors_.resize(colors_size);
  ReadData(base::as_writable_byte_span(ref.colors_));

  decltype(ref.positions_)::size_type positions_size = 0;
  ReadSize(&positions_size);
  // Positions are optional. If they exist, they have the same count as colors.
  if (positions_size > 0 && positions_size != colors_size) {
    SetInvalid(DeserializationError::kInvalidPaintShaderPositionsSize);
    return;
  }
  size_t positions_bytes = positions_size * sizeof(SkScalar);
  if (positions_bytes > remaining_bytes_) {
    SetInvalid(DeserializationError::
                   kInsufficientRemainingBytes_Read_PaintShader_Positions);
    return;
  }
  ref.positions_.resize(positions_size);
  ReadData(base::as_writable_byte_span(ref.positions_));

  // We don't write the cached shader, so don't attempt to read it either.

  if (!(*shader)->IsValid()) {
    SetInvalid(DeserializationError::kInvalidPaintShader);
    return;
  }

  // All shader types but records are done.
  if (shader_type != PaintShader::Type::kPaintRecord) {
    (*shader)->ResolveSkObjects();
    return;
  }

  // Record shaders have shader ids.  Attempt to use cached versions of
  // these so that Skia can cache based on SkPictureShader::fUniqueId.
  // These shaders are always serialized (and assumed to not be large
  // records).  Handling this edge case in this roundabout way prevents
  // transfer cache entries from needing to depend on other transfer cache
  // entries.
  auto* entry =
      options_.transfer_cache->GetEntryAs<ServiceShaderTransferCacheEntry>(
          shader_id);
  // Only consider entries that use the same scale.  This limits the service
  // side transfer cache to only having one entry per shader but this will hit
  // the common case of enabling Skia reuse.
  if (entry && entry->shader()->tile_ == ref.tile_) {
    DCHECK(!ref.sk_cached_picture_);
    ref.sk_cached_picture_ = entry->shader()->sk_cached_picture_;
  } else {
    ref.ResolveSkObjects();
    DCHECK(ref.sk_cached_picture_);
    options_.transfer_cache->CreateLocalEntry(
        shader_id, std::make_unique<ServiceShaderTransferCacheEntry>(
                       *shader, shader_size));
  }
}

void PaintOpReader::Read(SkMatrix* matrix) {
  ReadSimple(matrix);
  FixupMatrixPostSerialization(matrix);
}

void PaintOpReader::Read(SkM44* matrix) {
  ReadSimple(matrix);
}

void PaintOpReader::Read(SkSamplingOptions* sampling) {
  bool useCubic;
  Read(&useCubic);
  if (useCubic) {
    SkCubicResampler cubic;
    Read(&cubic.B);
    Read(&cubic.C);
    *sampling = SkSamplingOptions(cubic);
  } else {
    SkFilterMode filter;
    SkMipmapMode mipmap;
    Read(&filter);
    Read(&mipmap);
    *sampling = SkSamplingOptions(filter, mipmap);
  }
}

void PaintOpReader::Read(SkYUVColorSpace* yuv_color_space) {
  uint32_t raw_yuv_color_space = kIdentity_SkYUVColorSpace;
  ReadSimple(&raw_yuv_color_space);

  if (raw_yuv_color_space > kLastEnum_SkYUVColorSpace) {
    SetInvalid(DeserializationError::kInvalidSkYUVColorSpace);
    return;
  }

  *yuv_color_space = static_cast<SkYUVColorSpace>(raw_yuv_color_space);
}

void PaintOpReader::Read(SkYUVAInfo::PlaneConfig* plane_config) {
  uint32_t raw_plane_config =
      static_cast<uint32_t>(SkYUVAInfo::PlaneConfig::kUnknown);
  ReadSimple(&raw_plane_config);

  if (raw_plane_config >
      static_cast<uint32_t>(SkYUVAInfo::PlaneConfig::kLast)) {
    SetInvalid(DeserializationError::kInvalidPlaneConfig);
    return;
  }

  *plane_config = static_cast<SkYUVAInfo::PlaneConfig>(raw_plane_config);
}

void PaintOpReader::Read(SkYUVAInfo::Subsampling* subsampling) {
  uint32_t raw_subsampling =
      static_cast<uint32_t>(SkYUVAInfo::Subsampling::kUnknown);
  ReadSimple(&raw_subsampling);

  if (raw_subsampling > static_cast<uint32_t>(SkYUVAInfo::Subsampling::kLast)) {
    SetInvalid(DeserializationError::kInvalidSubsampling);
    return;
  }

  *subsampling = static_cast<SkYUVAInfo::Subsampling>(raw_subsampling);
}

void PaintOpReader::Read(gpu::Mailbox* mailbox) {
  ReadData(base::as_writable_byte_span(mailbox->name));
}

void PaintOpReader::Read(SkHighContrastConfig* config) {
  Read(&config->fGrayscale);
  ReadEnum<SkHighContrastConfig::InvertStyle,
           SkHighContrastConfig::InvertStyle::kLast>(&config->fInvertStyle);
  ReadSimple(&config->fContrast);
}

void PaintOpReader::Read(gfx::HDRMetadata* hdr_metadata) {
  size_t size = 0;
  ReadSize(&size);
  if (remaining_bytes_ < size) {
    valid_ = false;
  }
  if (!valid_ || size == 0) {
    return;
  }
  uint8_t* scratch = CopyScratchSpace(size);
  if (!gfx::mojom::HDRMetadata::Deserialize(scratch, size, hdr_metadata)) {
    SetInvalid(DeserializationError::kHdrMetadataDeserializeFailure);
  }
  DidRead(size);
}

void PaintOpReader::Read(SkGradientShader::Interpolation* interpolation) {
  ReadEnum<SkGradientShader::Interpolation::InPremul,
           SkGradientShader::Interpolation::InPremul::kYes>(
      &interpolation->fInPremul);
  ReadEnum<SkGradientShader::Interpolation::ColorSpace,
           SkGradientShader::Interpolation::ColorSpace::kLastColorSpace>(
      &interpolation->fColorSpace);
  ReadEnum<SkGradientShader::Interpolation::HueMethod,
           SkGradientShader::Interpolation::HueMethod::kLastHueMethod>(
      &interpolation->fHueMethod);
}

void PaintOpReader::Read(scoped_refptr<SkottieWrapper>* skottie) {
  if (!options_.is_privileged) {
    valid_ = false;
    return;
  }

  uint32_t transfer_cache_entry_id;
  ReadSimple(&transfer_cache_entry_id);
  if (!valid_)
    return;
  auto* entry =
      options_.transfer_cache->GetEntryAs<ServiceSkottieTransferCacheEntry>(
          transfer_cache_entry_id);
  if (entry) {
    *skottie = entry->skottie();
  } else {
    valid_ = false;
  }

  size_t bytes_to_skip = 0u;
  ReadSize(&bytes_to_skip);
  if (!valid_)
    return;
  if (bytes_to_skip > remaining_bytes_) {
    valid_ = false;
    return;
  }
  DidRead(bytes_to_skip);
}

void PaintOpReader::AlignMemory(size_t alignment) {
  DCHECK_GE(alignment, PaintOpWriter::kDefaultAlignment);
  DCHECK_LE(alignment, BufferAlignment());
  // base::bits::AlignUp() below will check if alignment is a power of two.

  size_t padding = base::bits::AlignUp(memory_, alignment) - memory_;
  if (padding > remaining_bytes_)
    SetInvalid(DeserializationError::kInsufficientRemainingBytes_AlignMemory);

  memory_ += padding;
  remaining_bytes_ -= padding;
}

// Don't inline this function so that crash reports can show the caller.
NOINLINE void PaintOpReader::SetInvalid(DeserializationError error) {
  static crash_reporter::CrashKeyString<4> deserialization_error_crash_key(
      "PaintOpReader deserialization error");
  base::UmaHistogramEnumeration("GPU.PaintOpReader.DeserializationError",
                                error);
  if (valid_ && options_.crash_dump_on_failure && base::RandInt(1, 10) == 1) {
    crash_reporter::ScopedCrashKeyString crash_key_scope(
        &deserialization_error_crash_key,
        base::NumberToString(static_cast<int>(error)));
    base::debug::DumpWithoutCrashing();
  }
  valid_ = false;
}

const volatile void* PaintOpReader::ExtractReadableMemory(size_t bytes) {
  if (remaining_bytes_ < bytes)
    SetInvalid(DeserializationError::
                   kInsufficientRemainingBytes_ExtractReadableMemory);
  if (!valid_)
    return nullptr;
  if (bytes == 0)
    return nullptr;

  const volatile void* extracted_memory = memory_;
  DidRead(bytes);
  return extracted_memory;
}

void PaintOpReader::Read(sk_sp<ColorFilter>* filter) {
  ColorFilter::Type type;
  ReadEnum(&type);
  if (!valid_) {
    return;
  }
  if (type == ColorFilter::Type::kNull) {
    *filter = nullptr;
    return;
  }
  *filter = ColorFilter::Deserialize(*this, type);
}

void PaintOpReader::Read(sk_sp<PathEffect>* effect) {
  PathEffect::Type type;
  ReadEnum(&type);
  if (!valid_) {
    return;
  }
  if (type == PathEffect::Type::kNull) {
    *effect = nullptr;
    return;
  }
  *effect = PathEffect::Deserialize(*this, type);
}

void PaintOpReader::Read(sk_sp<PaintFilter>* filter) {
  PaintFilter::Type type;
  ReadEnum(&type);
  if (!valid_)
    return;

  if (type == PaintFilter::Type::kNullFilter) {
    *filter = nullptr;
    return;
  }

  uint32_t has_crop_rect = 0;
  std::optional<PaintFilter::CropRect> crop_rect;
  ReadSimple(&has_crop_rect);
  if (has_crop_rect) {
    SkRect rect = SkRect::MakeEmpty();
    ReadSimple(&rect);
    crop_rect.emplace(rect);
  }

  AssertFieldAlignment();
  switch (type) {
    case PaintFilter::Type::kNullFilter:
      NOTREACHED();
    case PaintFilter::Type::kColorFilter:
      ReadColorFilterPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kBlur:
      ReadBlurPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kDropShadow:
      ReadDropShadowPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kMagnifier:
      ReadMagnifierPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kCompose:
      ReadComposePaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kAlphaThreshold:
      ReadAlphaThresholdPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kXfermode:
      ReadXfermodePaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kArithmetic:
      ReadArithmeticPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kMatrixConvolution:
      ReadMatrixConvolutionPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kDisplacementMapEffect:
      ReadDisplacementMapEffectPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kImage:
      ReadImagePaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kPaintRecord:
      ReadRecordPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kMerge:
      ReadMergePaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kMorphology:
      ReadMorphologyPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kOffset:
      ReadOffsetPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kTile:
      ReadTilePaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kTurbulence:
      ReadTurbulencePaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kShader:
      ReadShaderPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kMatrix:
      ReadMatrixPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kLightingDistant:
      ReadLightingDistantPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kLightingPoint:
      ReadLightingPointPaintFilter(filter, crop_rect);
      break;
    case PaintFilter::Type::kLightingSpot:
      ReadLightingSpotPaintFilter(filter, crop_rect);
      break;
  }
}

void PaintOpReader::ReadColorFilterPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  sk_sp<ColorFilter> color_filter;
  sk_sp<PaintFilter> input;

  Read(&color_filter);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new ColorFilterPaintFilter(std::move(color_filter),
                                           std::move(input),
                                           base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadBlurPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkScalar sigma_x = 0.f;
  SkScalar sigma_y = 0.f;
  SkTileMode tile_mode;
  sk_sp<PaintFilter> input;

  Read(&sigma_x);
  Read(&sigma_y);
  Read(&tile_mode);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new BlurPaintFilter(sigma_x, sigma_y, tile_mode,
                                    std::move(input),
                                    base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadDropShadowPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkScalar dx = 0.f;
  SkScalar dy = 0.f;
  SkScalar sigma_x = 0.f;
  SkScalar sigma_y = 0.f;
  SkColor color = SK_ColorBLACK;
  DropShadowPaintFilter::ShadowMode shadow_mode;
  sk_sp<PaintFilter> input;

  Read(&dx);
  Read(&dy);
  Read(&sigma_x);
  Read(&sigma_y);
  Read(&color);
  ReadEnum(&shadow_mode);
  Read(&input);
  if (!valid_)
    return;
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  filter->reset(new DropShadowPaintFilter(
      dx, dy, sigma_x, sigma_y, SkColor4f::FromColor(color), shadow_mode,
      std::move(input), base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadMagnifierPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkRect lens_bounds = SkRect::MakeEmpty();
  SkScalar zoom_amount = 1.f;
  SkScalar inset = 0.f;
  sk_sp<PaintFilter> input;

  Read(&lens_bounds);
  Read(&zoom_amount);
  Read(&inset);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new MagnifierPaintFilter(lens_bounds, zoom_amount, inset,
                                         std::move(input),
                                         base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadComposePaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  sk_sp<PaintFilter> outer;
  sk_sp<PaintFilter> inner;

  Read(&outer);
  Read(&inner);
  if (!valid_)
    return;
  filter->reset(new ComposePaintFilter(std::move(outer), std::move(inner)));
}

void PaintOpReader::ReadAlphaThresholdPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkRegion region;
  sk_sp<PaintFilter> input;

  Read(&region);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new AlphaThresholdPaintFilter(region, std::move(input),
                                              base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadXfermodePaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkBlendMode blend_mode;
  sk_sp<PaintFilter> background;
  sk_sp<PaintFilter> foreground;

  Read(&blend_mode);
  Read(&background);
  Read(&foreground);
  if (!valid_)
    return;

  filter->reset(new XfermodePaintFilter(blend_mode, std::move(background),
                                        std::move(foreground),
                                        base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadArithmeticPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  float k1 = 0.f;
  float k2 = 0.f;
  float k3 = 0.f;
  float k4 = 0.f;
  bool enforce_pm_color = false;
  sk_sp<PaintFilter> background;
  sk_sp<PaintFilter> foreground;
  Read(&k1);
  Read(&k2);
  Read(&k3);
  Read(&k4);
  Read(&enforce_pm_color);
  Read(&background);
  Read(&foreground);
  if (!valid_)
    return;
  filter->reset(new ArithmeticPaintFilter(
      k1, k2, k3, k4, enforce_pm_color, std::move(background),
      std::move(foreground), base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadMatrixConvolutionPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkISize kernel_size = SkISize::MakeEmpty();
  SkScalar gain = 0.f;
  SkScalar bias = 0.f;
  SkIPoint kernel_offset = SkIPoint::Make(0, 0);
  SkTileMode tile_mode;
  bool convolve_alpha = false;
  sk_sp<PaintFilter> input;

  ReadSimple(&kernel_size);
  if (!valid_) {
    return;
  }
  if (kernel_size.isEmpty()) {
    SetInvalid(
        DeserializationError::
            kInsufficientRemainingBytes_ReadMatrixConvolutionPaintFilter);
    return;
  }
  auto size = static_cast<size_t>(kernel_size.width()) *
              static_cast<size_t>(kernel_size.height());
  if (size > remaining_bytes_) {
    SetInvalid(
        DeserializationError::
            kInsufficientRemainingBytes_ReadMatrixConvolutionPaintFilter);
    return;
  }
  std::vector<SkScalar> kernel(size);
  for (size_t i = 0; i < size; ++i) {
    Read(&kernel[i]);
  }
  Read(&gain);
  Read(&bias);
  ReadSimple(&kernel_offset);
  Read(&tile_mode);
  Read(&convolve_alpha);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new MatrixConvolutionPaintFilter(
      kernel_size, kernel.data(), gain, bias, kernel_offset, tile_mode,
      convolve_alpha, std::move(input), base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadDisplacementMapEffectPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkColorChannel channel_x;
  SkColorChannel channel_y;
  SkScalar scale = 0.f;
  sk_sp<PaintFilter> displacement;
  sk_sp<PaintFilter> color;

  ReadEnum<SkColorChannel, SkColorChannel::kA>(&channel_x);
  ReadEnum<SkColorChannel, SkColorChannel::kA>(&channel_y);
  Read(&scale);
  Read(&displacement);
  Read(&color);

  if (!valid_)
    return;
  filter->reset(new DisplacementMapEffectPaintFilter(
      channel_x, channel_y, scale, std::move(displacement), std::move(color),
      base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadImagePaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  PaintImage image;
  Read(&image, PaintFlags::DynamicRangeLimitMixture(
                   PaintFlags::DynamicRangeLimit::kHigh));
  if (!image) {
    SetInvalid(DeserializationError::kReadImageFailure);
    return;
  }

  SkRect src_rect;
  Read(&src_rect);
  SkRect dst_rect;
  Read(&dst_rect);
  PaintFlags::FilterQuality quality;
  Read(&quality);

  if (!valid_)
    return;
  filter->reset(
      new ImagePaintFilter(std::move(image), src_rect, dst_rect, quality));
}

void PaintOpReader::ReadRecordPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  bool has_filter = false;
  Read(&has_filter);
  if (!has_filter) {
    *filter = nullptr;
    return;
  }

  SkRect record_bounds = SkRect::MakeEmpty();
  gfx::SizeF raster_scale = {0.f, 0.f};
  PaintShader::ScalingBehavior scaling_behavior =
      PaintShader::ScalingBehavior::kRasterAtScale;

  ReadSimple(&record_bounds);
  ReadSimple(&raster_scale);
  if (raster_scale.width() <= 0.f || raster_scale.height() <= 0.f) {
    SetInvalid(DeserializationError::kInvalidRasterScale);
    return;
  }

  ReadSimple(&scaling_behavior);
  if (!IsValidPaintShaderScalingBehavior(scaling_behavior)) {
    SetInvalid(DeserializationError::kInvalidPaintShaderScalingBehavior);
    return;
  }

  // RecordPaintFilter also requires kRasterAtScale to have {1.f, 1.f} as the
  // raster_scale, since that is intended for kFixedScale
  if (scaling_behavior == PaintShader::ScalingBehavior::kRasterAtScale &&
      (raster_scale.width() != 1.f || raster_scale.height() != 1.f)) {
    SetInvalid(DeserializationError::kInvalidRasterScale);
    return;
  }

  std::optional<PaintRecord> record;
  Read(&record);
  if (!valid_) {
    return;
  }
  filter->reset(new RecordPaintFilter(std::move(*record), record_bounds,
                                      raster_scale, scaling_behavior));
}

void PaintOpReader::ReadMergePaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  size_t input_count = 0;
  ReadSize(&input_count);

  // The minimum size for a serialized filter is 4 bytes (a zero uint32_t to
  // indicate a null filter). Make sure the |input_count| doesn't exceed the
  // maximum number of filters possible for the remaining data.
  const size_t max_filters = remaining_bytes_ / 4u;
  if (input_count > max_filters)
    SetInvalid(DeserializationError::kPaintFilterHasTooManyInputs);
  if (!valid_)
    return;
  std::vector<sk_sp<PaintFilter>> inputs(input_count);
  for (auto& input : inputs)
    Read(&input);
  if (!valid_)
    return;
  filter->reset(new MergePaintFilter(inputs.data(),
                                     static_cast<int>(input_count),
                                     base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadMorphologyPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  MorphologyPaintFilter::MorphType morph_type;
  float radius_x = 0;
  float radius_y = 0;
  sk_sp<PaintFilter> input;
  ReadEnum(&morph_type);
  Read(&radius_x);
  Read(&radius_y);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new MorphologyPaintFilter(morph_type, radius_x, radius_y,
                                          std::move(input),
                                          base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadOffsetPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkScalar dx = 0.f;
  SkScalar dy = 0.f;
  sk_sp<PaintFilter> input;

  Read(&dx);
  Read(&dy);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new OffsetPaintFilter(dx, dy, std::move(input),
                                      base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadTilePaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkRect src = SkRect::MakeEmpty();
  SkRect dst = SkRect::MakeEmpty();
  sk_sp<PaintFilter> input;

  Read(&src);
  Read(&dst);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(new TilePaintFilter(src, dst, std::move(input)));
}

void PaintOpReader::ReadTurbulencePaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  TurbulencePaintFilter::TurbulenceType turbulence_type;
  SkScalar base_frequency_x = 0.f;
  SkScalar base_frequency_y = 0.f;
  int num_octaves = 0;
  SkScalar seed = 0.f;
  SkISize tile_size = SkISize::MakeEmpty();

  ReadEnum(&turbulence_type);
  Read(&base_frequency_x);
  Read(&base_frequency_y);
  Read(&num_octaves);
  Read(&seed);
  ReadSimple(&tile_size);
  if (!valid_)
    return;
  filter->reset(new TurbulencePaintFilter(
      turbulence_type, base_frequency_x, base_frequency_y, num_octaves, seed,
      &tile_size, base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadShaderPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  using Dither = SkImageFilters::Dither;

  sk_sp<PaintShader> shader;
  float alpha = 1.0f;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kNone;
  Dither dither = Dither::kNo;

  Read(&shader);
  Read(&alpha);
  Read(&quality);
  ReadEnum<Dither, Dither::kYes>(&dither);

  if (!shader || !valid_)
    return;

  filter->reset(new ShaderPaintFilter(std::move(shader), alpha, quality, dither,
                                      base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadMatrixPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  SkMatrix matrix = SkMatrix::I();
  PaintFlags::FilterQuality filter_quality = PaintFlags::FilterQuality::kNone;
  sk_sp<PaintFilter> input;

  Read(&matrix);
  Read(&filter_quality);
  Read(&input);
  if (!valid_)
    return;
  filter->reset(
      new MatrixPaintFilter(matrix, filter_quality, std::move(input)));
}

void PaintOpReader::ReadLightingDistantPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  PaintFilter::LightingType lighting_type;
  SkPoint3 direction = SkPoint3::Make(0.f, 0.f, 0.f);
  SkColor light_color = SK_ColorBLACK;
  SkScalar surface_scale = 0.f;
  SkScalar kconstant = 0.f;
  SkScalar shininess = 0.f;
  sk_sp<PaintFilter> input;

  ReadEnum(&lighting_type);
  ReadSimple(&direction);
  Read(&light_color);
  Read(&surface_scale);
  Read(&kconstant);
  Read(&shininess);
  Read(&input);
  if (!valid_)
    return;
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  filter->reset(new LightingDistantPaintFilter(
      lighting_type, direction, SkColor4f::FromColor(light_color),
      surface_scale, kconstant, shininess, std::move(input),
      base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadLightingPointPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  PaintFilter::LightingType lighting_type;
  SkPoint3 location = SkPoint3::Make(0.f, 0.f, 0.f);
  SkColor light_color = SK_ColorBLACK;
  SkScalar surface_scale = 0.f;
  SkScalar kconstant = 0.f;
  SkScalar shininess = 0.f;
  sk_sp<PaintFilter> input;

  ReadEnum(&lighting_type);
  ReadSimple(&location);
  Read(&light_color);
  Read(&surface_scale);
  Read(&kconstant);
  Read(&shininess);
  Read(&input);
  if (!valid_)
    return;
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  filter->reset(new LightingPointPaintFilter(
      lighting_type, location, SkColor4f::FromColor(light_color), surface_scale,
      kconstant, shininess, std::move(input), base::OptionalToPtr(crop_rect)));
}

void PaintOpReader::ReadLightingSpotPaintFilter(
    sk_sp<PaintFilter>* filter,
    const std::optional<PaintFilter::CropRect>& crop_rect) {
  PaintFilter::LightingType lighting_type;
  SkPoint3 location = SkPoint3::Make(0.f, 0.f, 0.f);
  SkPoint3 target = SkPoint3::Make(0.f, 0.f, 0.f);
  SkScalar specular_exponent = 0.f;
  SkScalar cutoff_angle = 0.f;
  SkColor light_color = SK_ColorBLACK;
  SkScalar surface_scale = 0.f;
  SkScalar kconstant = 0.f;
  SkScalar shininess = 0.f;
  sk_sp<PaintFilter> input;

  ReadEnum(&lighting_type);
  ReadSimple(&location);
  ReadSimple(&target);
  Read(&specular_exponent);
  Read(&cutoff_angle);
  Read(&light_color);
  Read(&surface_scale);
  Read(&kconstant);
  Read(&shininess);
  Read(&input);

  if (!valid_)
    return;
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  filter->reset(new LightingSpotPaintFilter(
      lighting_type, location, target, specular_exponent, cutoff_angle,
      SkColor4f::FromColor(light_color), surface_scale, kconstant, shininess,
      std::move(input), base::OptionalToPtr(crop_rect)));
}

size_t PaintOpReader::Read(std::optional<PaintRecord>* record) {
  size_t size_bytes = 0;
  ReadSize(&size_bytes);

  if (enable_security_constraints_) {
    // Validate that the record was not serialized if security constraints are
    // enabled.
    if (size_bytes != 0) {
      SetInvalid(DeserializationError::kPaintRecordForbidden);
      return 0;
    }
    *record = PaintRecord();
    return 0;
  }

  AlignMemory(BufferAlignment());

  if (size_bytes > remaining_bytes_)
    SetInvalid(
        DeserializationError::kInsufficientRemainingBytes_Read_PaintRecord);
  if (!valid_)
    return 0;

  sk_sp<PaintOpBuffer> buffer =
      PaintOpBuffer::MakeFromMemory(memory_, size_bytes, options_);
  if (!buffer) {
    SetInvalid(DeserializationError::kPaintOpBufferMakeFromMemoryFailure);
    return 0;
  }
  *record = buffer->ReleaseAsRecord();
  DidRead(size_bytes);
  return size_bytes;
}

void PaintOpReader::Read(SkRegion* region) {
  size_t region_bytes = 0;
  ReadSize(&region_bytes);
  if (region_bytes == 0)
    SetInvalid(DeserializationError::kZeroRegionBytes);
  if (region_bytes > remaining_bytes_)
    SetInvalid(DeserializationError::kInsufficientRemainingBytes_Read_SkRegion);
  if (!valid_)
    return;
  auto data = base::HeapArray<char>::Uninit(region_bytes);
  ReadData(base::as_writable_byte_span(data));
  if (!valid_)
    return;
  size_t result = region->readFromMemory(data.data(), data.size());
  if (!result)
    SetInvalid(DeserializationError::kSkRegionReadFromMemoryFailure);
}

inline void PaintOpReader::DidRead(size_t bytes_read) {
  // All data are aligned with PaintOpWriter::kDefaultAlignment at least.
  size_t aligned_bytes =
      base::bits::AlignUp(bytes_read, PaintOpWriter::kDefaultAlignment);
  DCHECK_LE(aligned_bytes, remaining_bytes_);
  bytes_read = std::min(aligned_bytes, remaining_bytes_);
  memory_ += bytes_read;
  remaining_bytes_ -= bytes_read;
}

}  // namespace cc
