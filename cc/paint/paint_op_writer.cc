// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_writer.h"

#include <memory>
#include <type_traits>

#include "base/bits.h"
#include "base/notreached.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/skottie_transfer_cache_entry.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/private/chromium/GrSlug.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {

SkIRect MakeSrcRect(const PaintImage& image) {
  if (!image)
    return SkIRect::MakeEmpty();
  return SkIRect::MakeWH(image.width(), image.height());
}

}  // namespace

// static
size_t PaintOpWriter::GetFlattenableSize(const SkFlattenable* flattenable) {
  // The first bit is always written to indicate the serialized size of the
  // flattenable, or zero if it doesn't exist.
  size_t total_size = sizeof(uint64_t) + sizeof(uint64_t) /* alignment */;
  if (!flattenable)
    return total_size;

  // There is no method to know the serialized size of a flattenable without
  // serializing it.
  sk_sp<SkData> data = flattenable->serialize();
  total_size += data->isEmpty() ? 0u : data->size();
  return total_size;
}

// static
size_t PaintOpWriter::GetImageSize(const PaintImage& image) {
  // Image Serialization type.
  size_t image_size = sizeof(PaintOp::SerializedImageType);
  if (image) {
    auto info = SkImageInfo::Make(image.width(), image.height(),
                                  kN32_SkColorType, kPremul_SkAlphaType);
    image_size += sizeof(info.colorType());
    image_size += sizeof(info.width());
    image_size += sizeof(info.height());
    image_size += sizeof(uint64_t) + sizeof(uint64_t) /* alignment */;
    image_size += info.computeMinByteSize();
  }
  return image_size;
}

// static
size_t PaintOpWriter::GetRecordSize(const PaintRecord* record) {
  // Zero size indicates no record.
  // TODO(khushalsagar): Querying the size of a PaintRecord is not supported.
  // This works only for security constrained serialization which ignores
  // records.
  return sizeof(uint64_t);
}

PaintOpWriter::PaintOpWriter(void* memory,
                             size_t size,
                             const PaintOp::SerializeOptions& options,
                             bool enable_security_constraints)
    : memory_(static_cast<char*>(memory) + HeaderBytes()),
      size_(base::bits::AlignDown(size, Alignment())),
      remaining_bytes_(size_ - HeaderBytes()),
      options_(options),
      enable_security_constraints_(enable_security_constraints) {
  // Leave space for header of type/skip.
  DCHECK_GE(size, HeaderBytes());
  DCHECK_EQ(memory_.get(), base::bits::AlignUp(memory_.get(), Alignment()));
}

PaintOpWriter::~PaintOpWriter() = default;

template <typename T>
void PaintOpWriter::WriteSimple(const T& val) {
  static_assert(std::is_trivially_copyable_v<T>);

  // Round up each write to 4 bytes.  This is not technically perfect alignment,
  // but it is about 30% faster to post-align each write to 4 bytes than it is
  // to pre-align memory to the correct alignment.
  DCHECK_EQ(memory_.get(), base::bits::AlignUp(memory_.get(), Alignment()));
  static constexpr size_t size = base::bits::AlignUp(sizeof(T), Alignment());
  EnsureBytes(size);
  if (!valid_)
    return;

  reinterpret_cast<T*>(memory_.get())[0] = val;

  memory_ += size;
  remaining_bytes_ -= size;
}
void PaintOpWriter::WriteFlattenable(const SkFlattenable* val) {
  if (!val) {
    WriteSize(static_cast<size_t>(0u));
    return;
  }

  uint64_t* size_memory = WriteSize(0u);
  if (!valid_)
    return;

  size_t bytes_written = val->serialize(
      memory_, base::bits::AlignDown(remaining_bytes_, Alignment()));
  if (bytes_written == 0u) {
    valid_ = false;
    return;
  }
  *size_memory = bytes_written;
  DidWrite(bytes_written);
}

uint64_t* PaintOpWriter::WriteSize(size_t size) {
  AlignMemory(8);
  uint64_t* memory = reinterpret_cast<uint64_t*>(memory_.get());
  WriteSimple<uint64_t>(size);
  return memory;
}

void PaintOpWriter::Write(SkScalar data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint8_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint32_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(uint64_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(int32_t data) {
  WriteSimple(data);
}

void PaintOpWriter::Write(const SkRect& rect) {
  WriteSimple(rect);
}

void PaintOpWriter::Write(const SkIRect& rect) {
  WriteSimple(rect);
}

void PaintOpWriter::Write(const SkRRect& rect) {
  WriteSimple(rect);
}

void PaintOpWriter::Write(const SkColor4f& color) {
  WriteSimple(color);
}

void PaintOpWriter::Write(const SkPath& path, UsePaintCache use_paint_cache) {
  auto id = path.getGenerationID();
  if (!options_->for_identifiability_study)
    Write(id);

  DCHECK(use_paint_cache == UsePaintCache::kEnabled ||
         !options_->paint_cache->Get(PaintCacheDataType::kPath, id));
  if (use_paint_cache == UsePaintCache::kEnabled &&
      options_->paint_cache->Get(PaintCacheDataType::kPath, id)) {
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
  uint64_t* bytes_to_skip = WriteSize(0u);
  if (!valid_)
    return;

  if (bytes_required > remaining_bytes_) {
    valid_ = false;
    return;
  }
  size_t bytes_written = path.writeToMemory(memory_);
  DCHECK_EQ(bytes_written, bytes_required);
  if (use_paint_cache == UsePaintCache::kEnabled) {
    options_->paint_cache->Put(PaintCacheDataType::kPath, id, bytes_written);
  }
  *bytes_to_skip = bytes_written;
  DidWrite(bytes_written);
}

void PaintOpWriter::Write(const PaintFlags& flags, const SkM44& current_ctm) {
  WriteSimple(flags.color_);
  Write(flags.width_);
  Write(flags.miter_limit_);
  Write(flags.blend_mode_);
  WriteSimple(flags.bitfields_uint_);

  WriteFlattenable(flags.path_effect_.get());
  WriteFlattenable(flags.mask_filter_.get());
  WriteFlattenable(flags.color_filter_.get());

  if (enable_security_constraints_)
    WriteSize(static_cast<size_t>(0u));
  else
    WriteFlattenable(flags.draw_looper_.get());

  Write(flags.image_filter_.get(), current_ctm);
  Write(flags.shader_.get(), flags.getFilterQuality(), current_ctm);
}

void PaintOpWriter::Write(const DrawImage& draw_image,
                          SkSize* scale_adjustment) {
  const PaintImage& paint_image = draw_image.paint_image();

  // Empty image.
  if (!draw_image.paint_image()) {
    Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kNoImage));
    return;
  }

  // We never ask for subsets during serialization.
  DCHECK_EQ(paint_image.width(), draw_image.src_rect().width());
  DCHECK_EQ(paint_image.height(), draw_image.src_rect().height());

  // Security constrained serialization inlines the image bitmap.
  if (enable_security_constraints_) {
    SkBitmap bm;
    if (!draw_image.paint_image().GetSwSkImage()->asLegacyBitmap(&bm)) {
      Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kNoImage));
      return;
    }

    Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kImageData));
    const auto& pixmap = bm.pixmap();
    Write(pixmap.colorType());
    Write(pixmap.width());
    Write(pixmap.height());
    size_t pixmap_size = pixmap.computeByteSize();
    WriteSize(pixmap_size);
    WriteData(pixmap_size, pixmap.addr());
    return;
  }

  // Default mode uses the transfer cache.
  auto decoded_image = options_->image_provider->GetRasterContent(draw_image);
  DCHECK(!decoded_image.decoded_image().image())
      << "Use transfer cache for image serialization";
  const DecodedDrawImage& decoded_draw_image = decoded_image.decoded_image();
  DCHECK(decoded_draw_image.src_rect_offset().isEmpty())
      << "We shouldn't ask for image subsets";

  *scale_adjustment = decoded_draw_image.scale_adjustment();

  WriteImage(decoded_draw_image);
}

void PaintOpWriter::Write(scoped_refptr<SkottieWrapper> skottie) {
  uint32_t id = skottie->id();
  Write(id);

  uint64_t* bytes_to_skip = WriteSize(0u);
  if (!valid_)
    return;

  bool locked =
      options_->transfer_cache->LockEntry(TransferCacheEntryType::kSkottie, id);

  // Add a cache entry for the skottie animation.
  uint64_t bytes_written = 0u;
  if (!locked) {
    bytes_written = options_->transfer_cache->CreateEntry(
        ClientSkottieTransferCacheEntry(skottie), memory_);
    options_->transfer_cache->AssertLocked(TransferCacheEntryType::kSkottie,
                                           id);
  }

  DCHECK_LE(bytes_written, remaining_bytes_);
  *bytes_to_skip = bytes_written;
  DidWrite(bytes_written);
}

void PaintOpWriter::WriteImage(const DecodedDrawImage& decoded_draw_image) {
  if (!decoded_draw_image.mailbox().IsZero()) {
    WriteImage(decoded_draw_image.mailbox());
    return;
  }

  absl::optional<uint32_t> id = decoded_draw_image.transfer_cache_entry_id();
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

void PaintOpWriter::WriteImage(const gpu::Mailbox& mailbox) {
  DCHECK(!mailbox.IsZero());

  Write(static_cast<uint8_t>(PaintOp::SerializedImageType::kMailbox));

  EnsureBytes(sizeof(mailbox.name));
  if (!valid_)
    return;

  memcpy(memory_, mailbox.name, sizeof(mailbox.name));
  DidWrite(sizeof(mailbox.name));
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

void PaintOpWriter::Write(const SkColorSpace* color_space) {
  if (!color_space) {
    WriteSize(static_cast<size_t>(0));
    return;
  }
  size_t size = color_space->writeToMemory(nullptr);
  WriteSize(size);

  EnsureBytes(size);
  if (!valid_)
    return;

  size_t written = color_space->writeToMemory(memory_);
  CHECK_EQ(written, size);
  DidWrite(written);
}

void PaintOpWriter::Write(const sk_sp<GrSlug>& slug) {
  if (!valid_)
    return;

  AssertAlignment(Alignment());
  uint64_t* size_memory = WriteSize(0u);
  if (!valid_)
    return;

  size_t bytes_written = 0;
  if (slug) {
    // TODO(penghuang): should we use a unique id to avoid sending the same
    // slug?
    bytes_written = slug->serialize(
        memory_, base::bits::AlignDown(remaining_bytes_, Alignment()));
    if (bytes_written == 0u) {
      valid_ = false;
      return;
    }
  }

  *size_memory = bytes_written;
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
      return original->CreateDecodedImage(
          ctm, quality, options_->image_provider,
          paint_image_transfer_cache_entry_id, &quality, paint_image_needs_mips,
          mailbox_out);
    }
    sk_sp<PaintShader> record_shader =
        original->CreatePaintWorkletRecord(options_->image_provider);
    if (!record_shader)
      return nullptr;
    return record_shader->CreateScaledPaintRecord(
        ctm, options_->max_texture_size, paint_record_post_scale);
  }

  if (type == PaintShader::Type::kPaintRecord) {
    return original->CreateScaledPaintRecord(ctm, options_->max_texture_size,
                                             paint_record_post_scale);
  }

  return sk_ref_sp<PaintShader>(original);
}

void PaintOpWriter::Write(SkMatrix matrix) {
  if (!matrix.isIdentity())
    matrix.dirtyMatrixTypeCache();
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
  WriteSimple(shader->gradient_interpolation_);

  if (enable_security_constraints_) {
    DrawImage draw_image(shader->image_, false, MakeSrcRect(shader->image_),
                         quality, SkM44());
    SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
    Write(draw_image, &scale_adjustment);
    DCHECK_EQ(scale_adjustment.width(), 1.f);
    DCHECK_EQ(scale_adjustment.height(), 1.f);
  } else {
    if (!mailbox.IsZero())
      WriteImage(mailbox);
    else
      WriteImage(paint_image_transfer_cache_id, paint_image_needs_mips);
  }

  if (shader->record_) {
    Write(true);
    DCHECK_NE(shader->id_, PaintShader::kInvalidRecordShaderId);
    if (!options_->for_identifiability_study)
      Write(shader->id_);
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
  DCHECK_EQ(memory_.get(),
            base::bits::AlignUp(memory_.get(), PaintOpWriter::Alignment()));
  if (bytes == 0)
    return;

  EnsureBytes(bytes);

  if (!valid_)
    return;

  memcpy(memory_, input, bytes);
  DidWrite(bytes);
}

void PaintOpWriter::AlignMemory(size_t alignment) {
  // Due to the math below, alignment must be a power of two.
  DCHECK_GT(alignment, 0u);
  DCHECK_EQ(alignment & (alignment - 1), 0u);

  uintptr_t memory = reinterpret_cast<uintptr_t>(memory_.get());
  // The following is equivalent to:
  //   padding = (alignment - memory % alignment) % alignment;
  // because alignment is a power of two. This doesn't use modulo operator
  // however, since it can be slow.
  size_t padding = base::bits::AlignUp(memory, alignment) - memory;
  EnsureBytes(padding);
  if (!valid_)
    return;

  memory_ += padding;
  remaining_bytes_ -= padding;
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

  if (!valid_)
    return;

  AssertAlignment(Alignment());
  switch (filter->type()) {
    case PaintFilter::Type::kNullFilter:
      NOTREACHED();
      break;
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

void PaintOpWriter::Write(const ColorFilterPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteFlattenable(filter.color_filter().get());
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
  // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
  WriteSimple(filter.color().toSkColor());
  WriteEnum(filter.shadow_mode());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const MagnifierPaintFilter& filter,
                          const SkM44& current_ctm) {
  WriteSimple(filter.src_rect());
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
  WriteSimple(filter.inner_min());
  WriteSimple(filter.outer_max());
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
      current_ctm.asM33(), options_->max_texture_size);
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
  for (size_t i = 0; i < filter.input_count(); ++i)
    Write(filter.input_at(i), current_ctm);
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
  // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
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
  // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
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
  // TODO(crbug/1308932): Remove toSkColor and make all SkColor4f.
  WriteSimple(filter.light_color().toSkColor());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get(), current_ctm);
}

void PaintOpWriter::Write(const PaintRecord& record,
                          const gfx::Rect& playback_rect,
                          const gfx::SizeF& post_scale) {
  AlignMemory(PaintOpBuffer::kPaintOpAlign);

  // We need to record how many bytes we will serialize, but we don't know this
  // information until we do the serialization. So, skip the amount needed
  // before writing.
  size_t size_offset = sizeof(uint64_t);
  EnsureBytes(size_offset);
  if (!valid_)
    return;

  uint64_t* size_memory = WriteSize(0u);
  if (!valid_)
    return;

  if (enable_security_constraints_) {
    // We don't serialize PaintRecords when security constraints are enabled.
    return;
  }

  // Nested records are used for picture shaders and filters. These are always
  // converted to a fixed scale mode (hence |post_scale|), which means they are
  // first rendered offscreen via SkImage::MakeFromPicture. This inherently does
  // not support lcd text, so reflect that in the serialization options.
  PaintOp::SerializeOptions lcd_disabled_options = *options_;
  lcd_disabled_options.can_use_lcd_text = false;
  SimpleBufferSerializer serializer(memory_, remaining_bytes_,
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
  *size_memory = serializer.written();

  // The serializer should have failed if it ran out of space. DCHECK to verify
  // that it wrote at most as many bytes as we had left.
  DCHECK_LE(serializer.written(), remaining_bytes_);
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

inline void PaintOpWriter::DidWrite(size_t bytes_written) {
  // All data are aligned with PaintOpWriter::Alignment() at least.
  size_t aligned_bytes = base::bits::AlignUp(bytes_written, Alignment());
  memory_ += aligned_bytes;
  DCHECK_LE(aligned_bytes, remaining_bytes_);
  remaining_bytes_ -= aligned_bytes;
}

inline void PaintOpWriter::EnsureBytes(size_t required_bytes) {
  if (remaining_bytes_ < required_bytes)
    valid_ = false;
}

}  // namespace cc
