// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_op_writer.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "base/bits.h"
#include "base/notreached.h"
#include "cc/paint/color_filter.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/draw_looper.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/path_effect.h"
#include "cc/paint/skottie_transfer_cache_entry.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "third_party/skia/include/private/chromium/SkImageChromium.h"
#include "third_party/skia/include/private/chromium/Slug.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace cc {
namespace {

SkIRect MakeSrcRect(const PaintImage& image) {
  if (!image) {
    return SkIRect::MakeEmpty();
  }
  return SkIRect::MakeWH(image.width(), image.height());
}

void WriteHeader(void* memory, uint8_t type, size_t serialized_size) {
  DCHECK_LT(serialized_size, PaintOpWriter::kMaxSerializedSize);
  static_cast<uint32_t*>(memory)[0] = type | serialized_size << 8;
}

}  // namespace

// static
size_t PaintOpWriter::SerializedSize(const PaintImage& image) {
  // Image Serialization type.
  base::CheckedNumeric<size_t> image_size =
      SerializedSize<PaintOp::SerializedImageType>();
  if (image) {
    auto info = SkImageInfo::Make(image.width(), image.height(),
                                  kN32_SkColorType, kPremul_SkAlphaType);
    image_size += SerializedSizeSimple<bool>();
    image_size += SerializedSize(info.colorType());
    image_size += SerializedSize(info.width());
    image_size += SerializedSize(info.height());
    image_size += SerializedSizeOfBytes(info.computeMinByteSize());
  }
  return image_size.ValueOrDie();
}

// static
size_t PaintOpWriter::SerializedSize(const SkColorSpace* color_space) {
  return SerializedSizeOfBytes(color_space ? color_space->writeToMemory(nullptr)
                                           : 0u);
}

// static
size_t PaintOpWriter::SerializedSize(const gfx::HDRMetadata& hdr_metadata) {
  return SerializedSizeOfBytes(
      gfx::mojom::HDRMetadata::Serialize(&hdr_metadata).size());
}

// static
size_t PaintOpWriter::SerializedSize(const SkGainmapInfo& gainmap_info) {
  return SerializedSizeSimple<SkColor4f>() +  // fGainmapRatioMin
         SerializedSizeSimple<SkColor4f>() +  // fGainmapRatioMax
         SerializedSizeSimple<SkColor4f>() +  // fGainmapGamma
         SerializedSizeSimple<SkColor4f>() +  // fEpsilonSdr
         SerializedSizeSimple<SkColor4f>() +  // fEpsilonHdr
         SerializedSizeSimple<SkScalar>() +   // fDisplayRatioSdr
         SerializedSizeSimple<SkScalar>() +   // fDisplayRatioHdr
         SerializedSizeSimple<uint32_t>() +   // fBaseImageType
         SerializedSize(gainmap_info.fGainmapMathColorSpace.get());
}

// static
size_t PaintOpWriter::SerializedSize(const PaintRecord& record) {
  // TODO(khushalsagar): Querying the size of a PaintRecord is not supported.
  // This works only for security constrained serialization which ignores
  // records and writes only a size_t(0).
  return SerializedSize<size_t>();
}

// static
size_t PaintOpWriter::SerializedSize(const SkHighContrastConfig& config) {
  return SerializedSize(config.fGrayscale) +
         SerializedSize(config.fInvertStyle) + SerializedSize(config.fContrast);
}

// static
size_t PaintOpWriter::SerializedSize(const ColorFilter* filter) {
  if (!filter) {
    return SerializedSize(ColorFilter::Type::kNull);
  }
  base::CheckedNumeric<size_t> size = SerializedSize(filter->type_);
  size += filter->SerializedDataSize();
  return size.ValueOrDie();
}

// static
size_t PaintOpWriter::SerializedSize(const DrawLooper* looper) {
  if (!looper) {
    return SerializedSizeSimple<bool>();
  }
  size_t count = looper->layers_.size();
  base::CheckedNumeric<size_t> size =
      SerializedSizeSimple<bool>() + SerializedSize(count);
  if (count > 0) {
    const DrawLooper::Layer& layer = looper->layers_.front();
    base::CheckedNumeric<size_t> layer_size =
        SerializedSize(layer.offset) + SerializedSize(layer.blur_sigma) +
        SerializedSize(layer.color) + SerializedSize(layer.flags);
    size += layer_size * count;
  }
  return size.ValueOrDie();
}

// static
size_t PaintOpWriter::SerializedSize(const PaintFilter* filter) {
  if (!filter) {
    return SerializedSize(PaintFilter::Type::kNullFilter);
  }
  return filter->SerializedSize();
}

// static
size_t PaintOpWriter::SerializedSize(const PathEffect* effect) {
  if (!effect) {
    return SerializedSize(PathEffect::Type::kNull);
  }
  base::CheckedNumeric<size_t> size = SerializedSize(effect->type_);
  size += effect->SerializedDataSize();
  return size.ValueOrDie();
}

PaintOpWriter::~PaintOpWriter() = default;

size_t PaintOpWriter::FinishOp(uint8_t type) {
  if (!valid_) {
    return 0u;
  }

  size_t written = size();
  DCHECK_GE(written, kHeaderBytes);

  size_t aligned_written = base::bits::AlignUp(written, BufferAlignment());
  size_t padding = aligned_written - written;
  if (aligned_written > kMaxSerializedSize || padding > remaining_bytes()) {
    valid_ = false;
    return 0u;
  }

  // Write type and skip into the header bytes.
  WriteHeader(memory_ - written, type, aligned_written);

  memory_ += padding;
  return aligned_written;
}

void PaintOpWriter::WriteHeaderForTesting(void* memory,
                                          uint8_t type,
                                          size_t serialized_size) {
  WriteHeader(memory, type, serialized_size);
}

void PaintOpWriter::WriteSize(size_t size) {
  EnsureBytes(SerializedSize<size_t>());
  if (!valid_) {
    return;
  }
  WriteSizeAt(memory_, size);
  DidWrite(SerializedSize<size_t>());
}

void* PaintOpWriter::SkipSize() {
  auto* memory = memory_;
  WriteSize(0u);
  return memory;
}

void PaintOpWriter::WriteSizeAt(void* memory, size_t size) {
  // size_t is always serialized as uint32_ts to make the serialized result
  // portable between 32bit and 64bit processes, and to meet the 4-byte
  // minimum alignment requirement of PaintOpWriter (https://crbug.com/1429994
  // and https://crbug.com/1440013).
  uint32_t* memory_32 = static_cast<uint32_t*>(memory);
  memory_32[0] = static_cast<uint32_t>(size);
  memory_32[1] = static_cast<uint32_t>(static_cast<uint64_t>(size) >> 32);
}

void PaintOpWriter::Write(const SkPath& path, UsePaintCache use_paint_cache) {
  auto id = path.getGenerationID();
  if (!options_.for_identifiability_study) {
    Write(id);
  }

  DCHECK(use_paint_cache == UsePaintCache::kEnabled ||
         !options_.paint_cache->Get(PaintCacheDataType::kPath, id));
  if (use_paint_cache == UsePaintCache::kEnabled &&
      options_.paint_cache->Get(PaintCacheDataType::kPath, id)) {
    Write(static_cast<uint32_t>(PaintCacheEntryState::kCached));
    return;
  }

  // The SkPath may fail to serialize if the bytes required would overflow.
  uint64_t bytes_required = path.writeToMemory(nullptr);
  if (bytes_required == 0u) {
    Write(static_cast<uint32_t>(PaintCacheEntryState::kEmpty));
    return;
  }

  if (use_paint_cache == UsePaintCache::kEnabled) {
    Write(static_cast<uint32_t>(PaintCacheEntryState::kInlined));
  } else {
    Write(static_cast<uint32_t>(PaintCacheEntryState::kInlinedDoNotCache));
  }
  void* bytes_to_skip = SkipSize();
  if (!valid_) {
    return;
  }

  if (bytes_required > remaining_bytes()) {
    valid_ = false;
    return;
  }
  size_t bytes_written = path.writeToMemory(memory_);
  DCHECK_EQ(bytes_written, bytes_required);
  if (use_paint_cache == UsePaintCache::kEnabled) {
    options_.paint_cache->Put(PaintCacheDataType::kPath, id, bytes_written);
  }
  WriteSizeAt(bytes_to_skip, bytes_written);
  DidWrite(bytes_written);
}

void PaintOpWriter::Write(const PaintFlags& flags, const SkM44& current_ctm) {
  if (flags.path_effect_ == nullptr && flags.color_filter_ == nullptr &&
      flags.draw_looper_ == nullptr && flags.image_filter_ == nullptr &&
      flags.shader_ == nullptr) {
    // Fast path for when there is nothing complicated to write.
    // NOTE: size_t is written as two 32-bit zeros (see WriteSize()).
    WriteSimpleMultiple(
        flags.color_, flags.width_, flags.miter_limit_, flags.bitfields_uint_,
        // flags.path_effect_.
        base::checked_cast<uint8_t>(PathEffect::Type::kNull),
        // flags.color_filter_.
        base::checked_cast<uint8_t>(ColorFilter::Type::kNull),
        // flags.draw_looper_.
        false,
        // flags.image_filter_.
        base::checked_cast<uint8_t>(PaintFilter::Type::kNullFilter),
        // flags.shader_.
        false);
    return;
  }

  WriteSimple(flags.color_);
  Write(flags.width_);
  Write(flags.miter_limit_);
  WriteSimple(flags.bitfields_uint_);

  Write(flags.path_effect_.get());
  Write(flags.color_filter_.get());

  if (enable_security_constraints_) {
    const bool has_looper = false;
    WriteSimple(has_looper);
  } else {
    Write(flags.draw_looper_.get());
  }

  Write(flags.image_filter_.get(), current_ctm);
  Write(flags.shader_.get(), flags.getFilterQuality(), current_ctm);
}

void PaintOpWriter::Write(const DrawImage& draw_image,
                          SkSize* scale_adjustment) {
  const PaintImage& paint_image = draw_image.paint_image();

  // Empty image.
  if (!paint_image) {
    Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kNoImage));
    return;
  }

  // We never ask for subsets during serialization.
  DCHECK_EQ(paint_image.width(), draw_image.src_rect().width());
  DCHECK_EQ(paint_image.height(), draw_image.src_rect().height());

  // Security constrained serialization inlines the image bitmap.
  if (enable_security_constraints_) {
    SkBitmap bm;
    if (!paint_image.GetSwSkImage()->asLegacyBitmap(&bm)) {
      Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kNoImage));
      return;
    }

    Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kImageData));
    const auto& pixmap = bm.pixmap();
    // Pixmaps requiring alignments larger than kDefaultAlignment [1] are not
    // supported because the buffer is only guaranteed to align to
    // kDefaultAlignment when enable_security_constraits_ is true.
    // [1] See https://crbug.com/1300188 and https://crrev.com/c/3485859.
    DCHECK_LE(static_cast<size_t>(SkColorTypeBytesPerPixel(pixmap.colorType())),
              kDefaultAlignment);
    Write(pixmap.colorType());
    Write(pixmap.width());
    Write(pixmap.height());
    size_t pixmap_size = pixmap.computeByteSize();
    WriteSize(pixmap_size);
    WriteData(pixmap_size, pixmap.addr());
    return;
  }

  // Default mode uses the transfer cache.
  auto decoded_image = options_.image_provider->GetRasterContent(draw_image);
  DCHECK(!decoded_image.decoded_image().image())
      << "Use transfer cache for image serialization";
  const DecodedDrawImage& decoded_draw_image = decoded_image.decoded_image();
  DCHECK(decoded_draw_image.src_rect_offset().isEmpty())
      << "We shouldn't ask for image subsets";

  *scale_adjustment = decoded_draw_image.scale_adjustment();

  WriteImage(decoded_draw_image, paint_image.GetReinterpretAsSRGB());
}

void PaintOpWriter::Write(scoped_refptr<SkottieWrapper> skottie) {
  uint32_t id = skottie->id();
  Write(id);

  void* bytes_to_skip = SkipSize();
  if (!valid_) {
    return;
  }

  bool locked =
      options_.transfer_cache->LockEntry(TransferCacheEntryType::kSkottie, id);

  // Add a cache entry for the skottie animation.
  size_t bytes_written = 0u;
  if (!locked) {
    bytes_written = options_.transfer_cache->CreateEntry(
        ClientSkottieTransferCacheEntry(skottie), memory_);
    options_.transfer_cache->AssertLocked(TransferCacheEntryType::kSkottie, id);
  }

  DCHECK_LE(bytes_written, remaining_bytes());
  WriteSizeAt(bytes_to_skip, bytes_written);
  DidWrite(bytes_written);
}

void PaintOpWriter::WriteImage(const DecodedDrawImage& decoded_draw_image,
                               bool reinterpret_as_srgb) {
  if (!decoded_draw_image.mailbox().IsZero()) {
    WriteImage(decoded_draw_image.mailbox(), reinterpret_as_srgb);
    return;
  }

  std::optional<uint32_t> id = decoded_draw_image.transfer_cache_entry_id();
  // In the case of a decode failure, id may not be set. Send an invalid ID.
  WriteImage(id.value_or(kInvalidImageTransferCacheEntryId),
             decoded_draw_image.transfer_cache_entry_needs_mips());
}

void PaintOpWriter::WriteImage(uint32_t transfer_cache_entry_id,
                               bool needs_mips) {
  if (transfer_cache_entry_id == kInvalidImageTransferCacheEntryId) {
    Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kNoImage));
    return;
  }

  Write(
      static_cast<uint8_t>(PaintOp::SerializedImageType::kTransferCacheEntry));
  Write(transfer_cache_entry_id);
  Write(needs_mips);
}

void PaintOpWriter::WriteImage(const gpu::Mailbox& mailbox,
                               bool reinterpret_as_srgb) {
  DCHECK(!mailbox.IsZero());

  Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kMailbox));

  EnsureBytes(sizeof(mailbox.name));
  if (!valid_) {
    return;
  }

  memcpy(memory_, mailbox.name, sizeof(mailbox.name));
  DidWrite(sizeof(mailbox.name));
  Write(reinterpret_as_srgb);
}

void PaintOpWriter::Write(const SkHighContrastConfig& config) {
  WriteSimple(config.fGrayscale);
  WriteEnum(config.fInvertStyle);
  WriteSimple(config.fContrast);
}

void PaintOpWriter::Write(const sk_sp<SkData>& data) {
  if (data.get() && data->size()) {
    WriteSize(data->size());
    WriteData(data->size(), data->data());
  } else {
    // Differentiate between nullptr and valid but zero size.  It's not clear
    // that this happens in practice, but seems better to be consistent.
    WriteSize(static_cast<size_t>(0));
    Write(!!data.get());
  }
}

void PaintOpWriter::Write(const SkSamplingOptions& sampling) {
  Write(sampling.useCubic);
  if (sampling.useCubic) {
    Write(sampling.cubic.B);
    Write(sampling.cubic.C);
  } else {
    Write(sampling.filter);
    Write(sampling.mipmap);
  }
}

void PaintOpWriter::Write(
    const SkGradientShader::Interpolation& interpolation) {
  WriteEnum(interpolation.fInPremul);
  WriteEnum(interpolation.fColorSpace);
  WriteEnum(interpolation.fHueMethod);
}

void PaintOpWriter::Write(const SkColorSpace* color_space) {
  if (!color_space) {
    WriteSize(static_cast<size_t>(0));
    return;
  }
  size_t size = color_space->writeToMemory(nullptr);
  WriteSize(size);

  EnsureBytes(size);
  if (!valid_) {
    return;
  }

  size_t written = color_space->writeToMemory(memory_);
  CHECK_EQ(written, size);
  DidWrite(written);
}

void PaintOpWriter::Write(const gfx::HDRMetadata& hdr_metadata) {
  std::vector<uint8_t> bytes =
      gfx::mojom::HDRMetadata::Serialize(&hdr_metadata);
  WriteSize(bytes.size());
  WriteData(bytes.size(), bytes.data());
}

void PaintOpWriter::Write(const SkGainmapInfo& gainmap_info) {
  Write(gainmap_info.fGainmapRatioMin);
  Write(gainmap_info.fGainmapRatioMax);
  Write(gainmap_info.fGainmapGamma);
  Write(gainmap_info.fEpsilonSdr);
  Write(gainmap_info.fEpsilonHdr);
  Write(gainmap_info.fDisplayRatioSdr);
  Write(gainmap_info.fDisplayRatioHdr);
  Write(gainmap_info.fDisplayRatioHdr);
  uint32_t base_image_type = 0;
  switch (gainmap_info.fBaseImageType) {
    case SkGainmapInfo::BaseImageType::kSDR:
      base_image_type = 0;
      break;
    case SkGainmapInfo::BaseImageType::kHDR:
      base_image_type = 1;
      break;
  }
  Write(base_image_type);
  Write(gainmap_info.fGainmapMathColorSpace.get());
}

void PaintOpWriter::Write(const sk_sp<sktext::gpu::Slug>& slug) {
  if (!valid_) {
    return;
  }

  AssertFieldAlignment();
  void* size_memory = SkipSize();
  if (!valid_) {
    return;
  }

  size_t bytes_written = 0;
  if (slug) {
    // TODO(penghuang): should we use a unique id to avoid sending the same
    // slug?
    bytes_written = slug->serialize(
        memory_, base::bits::AlignDown(remaining_bytes(), kDefaultAlignment));
    if (bytes_written == 0u) {
      valid_ = false;
      return;
    }
  }

  WriteSizeAt(size_memory, bytes_written);
  DidWrite(bytes_written);
}

sk_sp<PaintShader> PaintOpWriter::TransformShaderIfNecessary(
    const PaintShader* original,
    PaintFlags::FilterQuality quality,
    const SkM44& current_ctm,
    uint32_t* paint_image_transfer_cache_entry_id,
    gfx::SizeF* paint_record_post_scale,
    bool* paint_image_needs_mips,
    gpu::Mailbox* mailbox_out) {
  DCHECK(!enable_security_constraints_);

  const auto type = original->shader_type();
  const auto& ctm = current_ctm.asM33();

  if (type == PaintShader::Type::kImage) {
    if (!original->paint_image().IsPaintWorklet()) {
      return original->CreateDecodedImage(ctm, quality, options_.image_provider,
                                          paint_image_transfer_cache_entry_id,
                                          &quality, paint_image_needs_mips,
                                          mailbox_out);
    }
    sk_sp<PaintShader> record_shader =
        original->CreatePaintWorkletRecord(options_.image_provider);
    if (!record_shader) {
      return nullptr;
    }
    return record_shader->CreateScaledPaintRecord(
        ctm, options_.max_texture_size, paint_record_post_scale);
  }

  if (type == PaintShader::Type::kPaintRecord) {
    return original->CreateScaledPaintRecord(ctm, options_.max_texture_size,
                                             paint_record_post_scale);
  }

  return sk_ref_sp<PaintShader>(original);
}

void PaintOpWriter::Write(SkMatrix matrix) {
  if (!matrix.isIdentity()) {
    matrix.dirtyMatrixTypeCache();
  }
  WriteSimple(matrix);
}

void PaintOpWriter::Write(const SkM44& matrix) {
  WriteSimple(matrix);
}

void PaintOpWriter::Write(const PaintShader* shader,
                          PaintFlags::FilterQuality quality,
                          const SkM44& current_ctm) {
  sk_sp<PaintShader> transformed_shader;
  uint32_t paint_image_transfer_cache_id = kInvalidImageTransferCacheEntryId;
  gfx::SizeF paint_record_post_scale(1.f, 1.f);
  bool paint_image_needs_mips = false;
  gpu::Mailbox mailbox;

  if (!enable_security_constraints_ && shader) {
    transformed_shader = TransformShaderIfNecessary(
        shader, quality, current_ctm, &paint_image_transfer_cache_id,
        &paint_record_post_scale, &paint_image_needs_mips, &mailbox);
    shader = transformed_shader.get();
  }

  if (!shader) {
    WriteSimple(false);
    return;
  }

  // TODO(vmpstr): This could be optimized to only serialize fields relevant to
  // the specific shader type. If done, then corresponding reading and tests
  // would have to also be updated.
  WriteSimple(true);
  WriteSimple(shader->shader_type_);
  WriteSimple(shader->flags_);
  WriteSimple(shader->end_radius_);
  WriteSimple(shader->start_radius_);
  Write(shader->tx_);
  Write(shader->ty_);
  WriteSimple(shader->fallback_color_);
  WriteSimple(shader->scaling_behavior_);
  if (shader->local_matrix_) {
    Write(true);
    Write(*shader->local_matrix_);
  } else {
    Write(false);
  }
  WriteSimple(shader->center_);
  WriteSimple(shader->tile_);
  WriteSimple(shader->start_point_);
  WriteSimple(shader->end_point_);
  WriteSimple(shader->start_degrees_);
  WriteSimple(shader->end_degrees_);
  Write(shader->gradient_interpolation_);

  if (enable_security_constraints_) {
    DrawImage draw_image(shader->image_, false, MakeSrcRect(shader->image_),
                         quality, SkM44());
    SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
    Write(draw_image, &scale_adjustment);
    DCHECK_EQ(scale_adjustment.width(), 1.f);
    DCHECK_EQ(scale_adjustment.height(), 1.f);
  } else {
    if (!mailbox.IsZero()) {
      WriteImage(mailbox, shader->image_.GetReinterpretAsSRGB());
    } else {
      WriteImage(paint_image_transfer_cache_id, paint_image_needs_mips);
    }
  }

  if (shader->record_) {
    Write(true);
    DCHECK_NE(shader->id_, PaintShader::kInvalidRecordShaderId);
    if (!options_.for_identifiability_study) {
      Write(shader->id_);
    }
    const gfx::Rect playback_rect(
        gfx::ToEnclosingRect(gfx::SkRectToRectF(shader->tile())));

    Write(*shader->record_, playback_rect, paint_record_post_scale);
  } else {
    DCHECK_EQ(shader->id_, PaintShader::kInvalidRecordShaderId);
    Write(false);
  }

  WriteSize(shader->colors_.size());
  WriteData(shader->colors_.size() *
                (shader->colors_.size() > 0 ? sizeof(shader->colors_[0]) : 0u),
            shader->colors_.data());

  WriteSize(shader->positions_.size());
  WriteData(shader->positions_.size() * sizeof(SkScalar),
            shader->positions_.data());
  // Explicitly don't write the cached_shader_ because that can be regenerated
  // using other fields.
}

void PaintOpWriter::Write(SkYUVColorSpace yuv_color_space) {
  WriteSimple(static_cast<uint32_t>(yuv_color_space));
}

void PaintOpWriter::Write(SkYUVAInfo::PlaneConfig plane_config) {
  WriteSimple(static_cast<uint32_t>(plane_config));
}

void PaintOpWriter::Write(SkYUVAInfo::Subsampling subsampling) {
  WriteSimple(static_cast<uint32_t>(subsampling));
}

void PaintOpWriter::WriteData(size_t bytes, const void* input) {
  AssertFieldAlignment();

  if (bytes == 0) {
    return;
  }

  EnsureBytes(bytes);

  if (!valid_) {
    return;
  }

  memcpy(memory_, input, bytes);
  DidWrite(bytes);
}

void PaintOpWriter::AlignMemory(size_t alignment) {
  DCHECK_GE(alignment, kDefaultAlignment);
  DCHECK_LE(alignment, BufferAlignment());
  // base::bits::AlignUp() below will check if alignment is a power of two.

  uintptr_t memory = reinterpret_cast<uintptr_t>(memory_);
  size_t padding = base::bits::AlignUp(memory, alignment) - memory;
  EnsureBytes(padding);
  if (!valid_) {
    return;
  }

  memory_ += padding;
}

void PaintOpWriter::Write(const ColorFilter* filter) {
  if (!filter) {
    WriteEnum(ColorFilter::Type::kNull);
    return;
  }
  WriteEnum(filter->type_);
  filter->SerializeData(*this);
}

void PaintOpWriter::Write(const DrawLooper* looper) {
  if (!looper) {
    WriteSimple(false);
    return;
  }

  WriteSimple(true);
  WriteSize(looper->layers_.size());

  for (auto const& layer : looper->layers_) {
    WriteSimple(layer.offset);
    WriteSimple(layer.blur_sigma);
    WriteSimple(layer.color);
    WriteSimple(layer.flags);
  }
}

void PaintOpWriter::Write(const PaintFilter* filter, const SkM44& current_ctm) {
  if (!filter) {
    WriteEnum(PaintFilter::Type::kNullFilter);
    return;
  }
  WriteEnum(filter->type());
  auto* crop_rect = filter->GetCropRect();
  WriteSimple(static_cast<uint32_t>(!!crop_rect));
  if (crop_rect) {
    WriteSimple(*crop_rect);
  }

  if (!valid_) {
    return;
  }

  AssertFieldAlignment();
  switch (filter->type()) {
    case PaintFilter::Type::kNullFilter:
      NOTREACHED();
    case PaintFilter::Type::kColorFilter:
      Write(static_cast<const ColorFilterPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kBlur:
      Write(static_cast<const BlurPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kDropShadow:
      Write(static_cast<const DropShadowPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kMagnifier:
      Write(static_cast<const MagnifierPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kCompose:
      Write(static_cast<const ComposePaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kAlphaThreshold:
      Write(static_cast<const AlphaThresholdPaintFilter&>(*filter),
            current_ctm);
      break;
    case PaintFilter::Type::kXfermode:
      Write(static_cast<const XfermodePaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kArithmetic:
      Write(static_cast<const ArithmeticPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kMatrixConvolution:
      Write(static_cast<const MatrixConvolutionPaintFilter&>(*filter),
            current_ctm);
      break;
    case PaintFilter::Type::kDisplacementMapEffect:
      Write(static_cast<const DisplacementMapEffectPaintFilter&>(*filter),
            current_ctm);
      break;
    case PaintFilter::Type::kImage:
      Write(static_cast<const ImagePaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kPaintRecord:
      Write(static_cast<const RecordPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kMerge:
      Write(static_cast<const MergePaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kMorphology:
      Write(static_cast<const MorphologyPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kOffset:
      Write(static_cast<const OffsetPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kTile:
      Write(static_cast<const TilePaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kTurbulence:
      Write(static_cast<const TurbulencePaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kShader:
      Write(static_cast<const ShaderPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kMatrix:
      Write(static_cast<const MatrixPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kLightingDistant:
      Write(static_cast<const LightingDistantPaintFilter&>(*filter),
            current_ctm);
      break;
    case PaintFilter::Type::kLightingPoint:
      Write(static_cast<const LightingPointPaintFilter&>(*filter), current_ctm);
      break;
    case PaintFilter::Type::kLightingSpot:
      Write(static_cast<const LightingSpotPaintFilter&>(*filter), current_ctm);
      break;
  }
}

void PaintOpWriter::Write(const PathEffect* effect) {
  if (!effect) {
    WriteEnum(PathEffect::Type::kNull);
    return;
  }
  WriteEnum(effect->type_);
  effect->SerializeData(*this);
}

void PaintOpWriter::Write(const ColorFilterPaintFilter& filter,
                          const SkM44& current_ctm) {
  Write(filter.color_filter().get());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const BlurPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.sigma_x());
  WriteSimple(filter.sigma_y());
  Write(filter.tile_mode());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const DropShadowPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.dx());
  WriteSimple(filter.dy());
  WriteSimple(filter.sigma_x());
  WriteSimple(filter.sigma_y());
  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  WriteSimple(filter.color().toSkColor());
  WriteEnum(filter.shadow_mode());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const MagnifierPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.lens_bounds());
  WriteSimple(filter.zoom_amount());
  WriteSimple(filter.inset());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const ComposePaintFilter& filter,
                          const SkM44& current_ctm) {
  Write(filter.outer().get(), current_ctm);
  Write(filter.inner().get(), current_ctm);
}

void PaintOpWriter::Write(const AlphaThresholdPaintFilter& filter,
                          const SkM44& current_ctm) {
  Write(filter.region());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const XfermodePaintFilter& filter,
                          const SkM44& current_ctm) {
  Write(filter.blend_mode());
  Write(filter.background().get(), current_ctm);
  Write(filter.foreground().get(), current_ctm);
}

void PaintOpWriter::Write(const ArithmeticPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.k1());
  WriteSimple(filter.k2());
  WriteSimple(filter.k3());
  WriteSimple(filter.k4());
  WriteSimple(filter.enforce_pm_color());
  Write(filter.background().get(), current_ctm);
  Write(filter.foreground().get(), current_ctm);
}

void PaintOpWriter::Write(const MatrixConvolutionPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.kernel_size());
  auto kernel_size = filter.kernel_size();
  DCHECK(!kernel_size.isEmpty());
  auto size = static_cast<size_t>(kernel_size.width()) *
              static_cast<size_t>(kernel_size.height());
  for (size_t i = 0; i < size; ++i) {
    WriteSimple(filter.kernel_at(i));
  }
  WriteSimple(filter.gain());
  WriteSimple(filter.bias());
  WriteSimple(filter.kernel_offset());
  Write(filter.tile_mode());
  WriteSimple(filter.convolve_alpha());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const DisplacementMapEffectPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteEnum(filter.channel_x());
  WriteEnum(filter.channel_y());
  WriteSimple(filter.scale());
  Write(filter.displacement().get(), current_ctm);
  Write(filter.color().get(), current_ctm);
}

void PaintOpWriter::Write(const ImagePaintFilter& filter,
                          const SkM44& current_ctm) {
  DrawImage draw_image(
      filter.image(), false,
      SkIRect::MakeWH(filter.image().width(), filter.image().height()),
      filter.filter_quality(), SkM44());
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
  Write(draw_image, &scale_adjustment);
  DCHECK_EQ(scale_adjustment.width(), 1.f);
  DCHECK_EQ(scale_adjustment.height(), 1.f);

  Write(filter.src_rect());
  Write(filter.dst_rect());
  Write(filter.filter_quality());
}

void PaintOpWriter::Write(const RecordPaintFilter& filter,
                          const SkM44& current_ctm) {
  // Convert to a fixed scale filter so that any content contained within
  // the filter's PaintRecord is rasterized at the scale we use here for
  // analysis (e.g. this ensures any contained text blobs will not be missing
  // from the cache).
  auto scaled_filter = filter.CreateScaledPaintRecord(
      current_ctm.asM33(), options_.max_texture_size);
  if (!scaled_filter) {
    WriteSimple(false);
    return;
  }

  WriteSimple(true);
  WriteSimple(scaled_filter->record_bounds());
  WriteSimple(scaled_filter->raster_scale());
  WriteSimple(scaled_filter->scaling_behavior());

  Write(scaled_filter->record(), gfx::Rect(), scaled_filter->raster_scale());
}

void PaintOpWriter::Write(const MergePaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSize(filter.input_count());
  for (size_t i = 0; i < filter.input_count(); ++i) {
    Write(filter.input_at(i), current_ctm);
  }
}

void PaintOpWriter::Write(const MorphologyPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteEnum(filter.morph_type());
  WriteSimple(filter.radius_x());
  WriteSimple(filter.radius_y());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const OffsetPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.dx());
  WriteSimple(filter.dy());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const TilePaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.src());
  WriteSimple(filter.dst());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const TurbulencePaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteEnum(filter.turbulence_type());
  WriteSimple(filter.base_frequency_x());
  WriteSimple(filter.base_frequency_y());
  WriteSimple(filter.num_octaves());
  WriteSimple(filter.seed());
  WriteSimple(filter.tile_size());
}

void PaintOpWriter::Write(const ShaderPaintFilter& filter,
                          const SkM44& current_ctm) {
  Write(&filter.shader(), filter.filter_quality(), current_ctm);
  Write(filter.alpha());
  Write(filter.filter_quality());
  WriteEnum(filter.dither());
}

void PaintOpWriter::Write(const MatrixPaintFilter& filter,
                          const SkM44& current_ctm) {
  Write(filter.matrix());
  Write(filter.filter_quality());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const LightingDistantPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteEnum(filter.lighting_type());
  WriteSimple(filter.direction());
  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  WriteSimple(filter.light_color().toSkColor());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const LightingPointPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteEnum(filter.lighting_type());
  WriteSimple(filter.location());
  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  WriteSimple(filter.light_color().toSkColor());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const LightingSpotPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteEnum(filter.lighting_type());
  WriteSimple(filter.location());
  WriteSimple(filter.target());
  WriteSimple(filter.specular_exponent());
  WriteSimple(filter.cutoff_angle());
  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  WriteSimple(filter.light_color().toSkColor());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const PaintRecord& record,
                          const gfx::Rect& playback_rect,
                          const gfx::SizeF& post_scale) {
  // We need to record how many bytes we will serialize, but we don't know this
  // information until we do the serialization. So, write 0 as the size first,
  // and amend it after writing.
  void* size_memory = SkipSize();
  if (!valid_) {
    return;
  }

  if (enable_security_constraints_) {
    // We don't serialize PaintRecords when security constraints are enabled.
    return;
  }

  AlignMemory(BufferAlignment());

  // Nested records are used for picture shaders and filters. These are always
  // converted to a fixed scale mode (hence |post_scale|), which means they are
  // first rendered offscreen via SkImages::DeferredFromPicture. This inherently
  // does not support lcd text, so reflect that in the serialization options.
  PaintOp::SerializeOptions lcd_disabled_options = options_;
  lcd_disabled_options.can_use_lcd_text = false;
  SimpleBufferSerializer serializer(memory_, remaining_bytes(),
                                    lcd_disabled_options);
  serializer.Serialize(record.buffer(), playback_rect, post_scale);

  if (!serializer.valid()) {
    valid_ = false;
    return;
  }
  // Now we can write the number of bytes we used. Ensure this amount is size_t,
  // since that's what we allocated for it.
  static_assert(sizeof(serializer.written()) == sizeof(size_t),
                "written() return type size is different from sizeof(size_t)");

  // Write the size to the size memory, which preceeds the memory for the
  // record.
  WriteSizeAt(size_memory, serializer.written());

  // The serializer should have failed if it ran out of space. DCHECK to verify
  // that it wrote at most as many bytes as we had left.
  DCHECK_LE(serializer.written(), remaining_bytes());
  DidWrite(serializer.written());
}

void PaintOpWriter::Write(const SkRegion& region) {
  size_t bytes_required = region.writeToMemory(nullptr);
  std::unique_ptr<char[]> data(new char[bytes_required]);
  size_t bytes_written = region.writeToMemory(data.get());
  DCHECK_EQ(bytes_required, bytes_written);

  WriteSize(bytes_written);
  WriteData(bytes_written, data.get());
}

}  // namespace cc
