// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_writer.h"

#include "base/bits.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/src/core/SkRemoteGlyphCache.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
const size_t kSkiaAlignment = 4u;

size_t RoundDownToAlignment(size_t bytes, size_t alignment) {
  return base::bits::AlignDown(bytes, alignment);
}

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
      size_(size),
      remaining_bytes_(size - HeaderBytes()),
      options_(options),
      enable_security_constraints_(enable_security_constraints) {
  // Leave space for header of type/skip.
  DCHECK_GE(size, HeaderBytes());
}

PaintOpWriter::~PaintOpWriter() = default;

template <typename T>
void PaintOpWriter::WriteSimple(const T& val) {
  static_assert(base::is_trivially_copyable<T>::value, "");

  // Round up each write to 4 bytes.  This is not technically perfect alignment,
  // but it is about 30% faster to post-align each write to 4 bytes than it is
  // to pre-align memory to the correct alignment.
  // TODO(enne): maybe we should do this correctly and DCHECK alignment.
  static constexpr size_t kAlign = 4;
  size_t size = base::bits::Align(sizeof(T), kAlign);
  EnsureBytes(size);
  if (!valid_)
    return;

  reinterpret_cast<T*>(memory_)[0] = val;

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
      memory_, RoundDownToAlignment(remaining_bytes_, kSkiaAlignment));
  if (bytes_written == 0u) {
    valid_ = false;
    return;
  }
  *size_memory = bytes_written;
  memory_ += bytes_written;
  remaining_bytes_ -= bytes_written;
}

uint64_t* PaintOpWriter::WriteSize(size_t size) {
  AlignMemory(8);
  uint64_t* memory = reinterpret_cast<uint64_t*>(memory_);
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

void PaintOpWriter::Write(const SkPath& path) {
  auto id = path.getGenerationID();
  Write(id);

  if (options_.paint_cache->Get(PaintCacheDataType::kPath, id)) {
    Write(static_cast<uint32_t>(PaintCacheEntryState::kCached));
    return;
  }

  // The SkPath may fail to serialize if the bytes required would overflow.
  uint64_t bytes_required = path.writeToMemory(nullptr);
  if (bytes_required == 0u) {
    Write(static_cast<uint32_t>(PaintCacheEntryState::kEmpty));
    return;
  }

  Write(static_cast<uint32_t>(PaintCacheEntryState::kInlined));
  uint64_t* bytes_to_skip = WriteSize(0u);
  if (!valid_)
    return;

  if (bytes_required > remaining_bytes_) {
    valid_ = false;
    return;
  }
  size_t bytes_written = path.writeToMemory(memory_);
  DCHECK_EQ(bytes_written, bytes_required);
  options_.paint_cache->Put(PaintCacheDataType::kPath, id, bytes_written);
  *bytes_to_skip = bytes_written;
  memory_ += bytes_written;
  remaining_bytes_ -= bytes_written;
}

void PaintOpWriter::Write(const PaintFlags& flags) {
  WriteSimple(flags.color_);
  Write(flags.width_);
  Write(flags.miter_limit_);
  WriteSimple(flags.blend_mode_);
  WriteSimple(flags.bitfields_uint_);

  WriteFlattenable(flags.path_effect_.get());
  WriteFlattenable(flags.mask_filter_.get());
  WriteFlattenable(flags.color_filter_.get());

  if (enable_security_constraints_)
    WriteSize(static_cast<size_t>(0u));
  else
    WriteFlattenable(flags.draw_looper_.get());

  Write(flags.image_filter_.get());
  Write(flags.shader_.get(), flags.getFilterQuality());
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
    if (!draw_image.paint_image().GetSkImage()->asLegacyBitmap(&bm)) {
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
  auto decoded_image = options_.image_provider->GetRasterContent(draw_image);
  DCHECK(!decoded_image.decoded_image().image())
      << "Use transfer cache for image serialization";
  const DecodedDrawImage& decoded_draw_image = decoded_image.decoded_image();
  DCHECK(decoded_draw_image.src_rect_offset().isEmpty())
      << "We shouldn't ask for image subsets";

  base::Optional<uint32_t> id = decoded_draw_image.transfer_cache_entry_id();
  *scale_adjustment = decoded_draw_image.scale_adjustment();
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

  memory_ += written;
  remaining_bytes_ -= written;
}

void PaintOpWriter::Write(const sk_sp<SkTextBlob>& blob) {
  DCHECK(blob);
  if (!valid_)
    return;

  AlignMemory(4);
  uint32_t blob_id = blob->uniqueID();
  Write(blob_id);
  uint64_t* size_memory = WriteSize(0u);
  if (!valid_)
    return;

  if (options_.paint_cache->Get(PaintCacheDataType::kTextBlob, blob_id))
    return;

  auto encodeTypeface = [](SkTypeface* tf, void* ctx) -> sk_sp<SkData> {
    return static_cast<SkStrikeServer*>(ctx)->serializeTypeface(tf);
  };
  DCHECK(options_.strike_server);
  SkSerialProcs procs;
  procs.fTypefaceProc = encodeTypeface;
  procs.fTypefaceCtx = options_.strike_server;

  size_t bytes_written = blob->serialize(
      procs, memory_, RoundDownToAlignment(remaining_bytes_, kSkiaAlignment));
  if (bytes_written == 0u) {
    valid_ = false;
    return;
  }

  options_.paint_cache->Put(PaintCacheDataType::kTextBlob, blob_id,
                            bytes_written);
  *size_memory = bytes_written;
  memory_ += bytes_written;
  remaining_bytes_ -= bytes_written;
}

sk_sp<PaintShader> PaintOpWriter::TransformShaderIfNecessary(
    const PaintShader* original,
    SkFilterQuality quality,
    uint32_t* paint_image_transfer_cache_entry_id,
    gfx::SizeF* paint_record_post_scale,
    bool* paint_image_needs_mips) {
  DCHECK(!enable_security_constraints_);

  const auto type = original->shader_type();
  const auto& ctm = options_.canvas->getTotalMatrix();

  if (type == PaintShader::Type::kImage) {
    return original->CreateDecodedImage(ctm, quality, options_.image_provider,
                                        paint_image_transfer_cache_entry_id,
                                        &quality, paint_image_needs_mips);
  }

  if (type == PaintShader::Type::kPaintRecord) {
    return original->CreateScaledPaintRecord(ctm, options_.max_texture_size,
                                             paint_record_post_scale);
  }

  return sk_ref_sp<PaintShader>(original);
}

void PaintOpWriter::Write(SkMatrix matrix) {
  if (!matrix.isIdentity())
    matrix.dirtyMatrixTypeCache();
  WriteSimple(matrix);
}

void PaintOpWriter::Write(const PaintShader* shader, SkFilterQuality quality) {
  sk_sp<PaintShader> transformed_shader;
  uint32_t paint_image_transfer_cache_id = kInvalidImageTransferCacheEntryId;
  gfx::SizeF paint_record_post_scale(1.f, 1.f);
  bool paint_image_needs_mips = false;

  if (!enable_security_constraints_ && shader) {
    transformed_shader = TransformShaderIfNecessary(
        shader, quality, &paint_image_transfer_cache_id,
        &paint_record_post_scale, &paint_image_needs_mips);
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
  // SkTileMode does not have an explicitly defined backing type, so
  // write a consistently sized value.
  Write(static_cast<int32_t>(shader->tx_));
  Write(static_cast<int32_t>(shader->ty_));
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

  if (enable_security_constraints_) {
    DrawImage draw_image(shader->image_, MakeSrcRect(shader->image_), quality,
                         SkMatrix::I());
    SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
    Write(draw_image, &scale_adjustment);
    DCHECK_EQ(scale_adjustment.width(), 1.f);
    DCHECK_EQ(scale_adjustment.height(), 1.f);
  } else {
    WriteImage(paint_image_transfer_cache_id, paint_image_needs_mips);
  }

  if (shader->record_) {
    Write(true);
    DCHECK_NE(shader->id_, PaintShader::kInvalidRecordShaderId);
    Write(shader->id_);
    const gfx::Rect playback_rect(
        gfx::ToEnclosingRect(gfx::SkRectToRectF(shader->tile())));

    Write(shader->record_.get(), playback_rect, paint_record_post_scale,
          SkMatrix::I());
  } else {
    DCHECK_EQ(shader->id_, PaintShader::kInvalidRecordShaderId);
    Write(false);
  }

  WriteSize(shader->colors_.size());
  WriteData(shader->colors_.size() * sizeof(SkColor), shader->colors_.data());

  WriteSize(shader->positions_.size());
  WriteData(shader->positions_.size() * sizeof(SkScalar),
            shader->positions_.data());
  // Explicitly don't write the cached_shader_ because that can be regenerated
  // using other fields.
}

void PaintOpWriter::Write(SkColorType color_type) {
  WriteSimple(static_cast<uint32_t>(color_type));
}

void PaintOpWriter::Write(SkYUVColorSpace yuv_color_space) {
  WriteSimple(static_cast<uint32_t>(yuv_color_space));
}

void PaintOpWriter::WriteData(size_t bytes, const void* input) {
  EnsureBytes(bytes);
  if (!valid_)
    return;
  if (bytes == 0)
    return;

  memcpy(memory_, input, bytes);
  memory_ += bytes;
  remaining_bytes_ -= bytes;
}

void PaintOpWriter::AlignMemory(size_t alignment) {
  // Due to the math below, alignment must be a power of two.
  DCHECK_GT(alignment, 0u);
  DCHECK_EQ(alignment & (alignment - 1), 0u);

  uintptr_t memory = reinterpret_cast<uintptr_t>(memory_);
  // The following is equivalent to:
  //   padding = (alignment - memory % alignment) % alignment;
  // because alignment is a power of two. This doesn't use modulo operator
  // however, since it can be slow.
  size_t padding = ((memory + alignment - 1) & ~(alignment - 1)) - memory;
  EnsureBytes(padding);
  if (!valid_)
    return;

  memory_ += padding;
  remaining_bytes_ -= padding;
}

void PaintOpWriter::Write(const PaintFilter* filter) {
  if (!filter) {
    WriteSimple(static_cast<uint32_t>(PaintFilter::Type::kNullFilter));
    return;
  }
  WriteSimple(static_cast<uint32_t>(filter->type()));
  auto* crop_rect = filter->crop_rect();
  WriteSimple(static_cast<uint32_t>(!!crop_rect));
  if (crop_rect) {
    WriteSimple(crop_rect->flags());
    WriteSimple(crop_rect->rect());
  }

  if (!valid_)
    return;

  AlignMemory(kSkiaAlignment);
  switch (filter->type()) {
    case PaintFilter::Type::kNullFilter:
      NOTREACHED();
      break;
    case PaintFilter::Type::kColorFilter:
      Write(static_cast<const ColorFilterPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kBlur:
      Write(static_cast<const BlurPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kDropShadow:
      Write(static_cast<const DropShadowPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMagnifier:
      Write(static_cast<const MagnifierPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kCompose:
      Write(static_cast<const ComposePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kAlphaThreshold:
      Write(static_cast<const AlphaThresholdPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kXfermode:
      Write(static_cast<const XfermodePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kArithmetic:
      Write(static_cast<const ArithmeticPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMatrixConvolution:
      Write(static_cast<const MatrixConvolutionPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kDisplacementMapEffect:
      Write(static_cast<const DisplacementMapEffectPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kImage:
      Write(static_cast<const ImagePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kPaintRecord:
      Write(static_cast<const RecordPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMerge:
      Write(static_cast<const MergePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMorphology:
      Write(static_cast<const MorphologyPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kOffset:
      Write(static_cast<const OffsetPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kTile:
      Write(static_cast<const TilePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kTurbulence:
      Write(static_cast<const TurbulencePaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kPaintFlags:
      Write(static_cast<const PaintFlagsPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kMatrix:
      Write(static_cast<const MatrixPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kLightingDistant:
      Write(static_cast<const LightingDistantPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kLightingPoint:
      Write(static_cast<const LightingPointPaintFilter&>(*filter));
      break;
    case PaintFilter::Type::kLightingSpot:
      Write(static_cast<const LightingSpotPaintFilter&>(*filter));
      break;
  }
}

void PaintOpWriter::Write(const ColorFilterPaintFilter& filter) {
  WriteFlattenable(filter.color_filter().get());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const BlurPaintFilter& filter) {
  WriteSimple(filter.sigma_x());
  WriteSimple(filter.sigma_y());
  WriteSimple(filter.tile_mode());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const DropShadowPaintFilter& filter) {
  WriteSimple(filter.dx());
  WriteSimple(filter.dy());
  WriteSimple(filter.sigma_x());
  WriteSimple(filter.sigma_y());
  WriteSimple(filter.color());
  WriteSimple(filter.shadow_mode());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const MagnifierPaintFilter& filter) {
  WriteSimple(filter.src_rect());
  WriteSimple(filter.inset());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const ComposePaintFilter& filter) {
  Write(filter.outer().get());
  Write(filter.inner().get());
}

void PaintOpWriter::Write(const AlphaThresholdPaintFilter& filter) {
  Write(filter.region());
  WriteSimple(filter.inner_min());
  WriteSimple(filter.outer_max());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const XfermodePaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.blend_mode()));
  Write(filter.background().get());
  Write(filter.foreground().get());
}

void PaintOpWriter::Write(const ArithmeticPaintFilter& filter) {
  WriteSimple(filter.k1());
  WriteSimple(filter.k2());
  WriteSimple(filter.k3());
  WriteSimple(filter.k4());
  WriteSimple(filter.enforce_pm_color());
  Write(filter.background().get());
  Write(filter.foreground().get());
}

void PaintOpWriter::Write(const MatrixConvolutionPaintFilter& filter) {
  WriteSimple(filter.kernel_size());
  auto size = static_cast<size_t>(
      sk_64_mul(filter.kernel_size().width(), filter.kernel_size().height()));
  for (size_t i = 0; i < size; ++i)
    WriteSimple(filter.kernel_at(i));
  WriteSimple(filter.gain());
  WriteSimple(filter.bias());
  WriteSimple(filter.kernel_offset());
  WriteSimple(static_cast<uint32_t>(filter.tile_mode()));
  WriteSimple(filter.convolve_alpha());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const DisplacementMapEffectPaintFilter& filter) {
  WriteSimple(static_cast<uint32_t>(filter.channel_x()));
  WriteSimple(static_cast<uint32_t>(filter.channel_y()));
  WriteSimple(filter.scale());
  Write(filter.displacement().get());
  Write(filter.color().get());
}

void PaintOpWriter::Write(const ImagePaintFilter& filter) {
  DrawImage draw_image(
      filter.image(),
      SkIRect::MakeWH(filter.image().width(), filter.image().height()),
      filter.filter_quality(), SkMatrix::I());
  SkSize scale_adjustment = SkSize::Make(1.f, 1.f);
  Write(draw_image, &scale_adjustment);
  DCHECK_EQ(scale_adjustment.width(), 1.f);
  DCHECK_EQ(scale_adjustment.height(), 1.f);

  Write(filter.src_rect());
  Write(filter.dst_rect());
  Write(filter.filter_quality());
}

void PaintOpWriter::Write(const RecordPaintFilter& filter) {
  WriteSimple(filter.record_bounds());
  if (!options_.canvas) {
    Write(filter.record().get(), gfx::Rect(), gfx::SizeF(1.f, 1.f),
          SkMatrix::I());
    return;
  }

  // The logic here to only use the scale component of the matrix during
  // analysis is for consistency with the rasterization of the filter later in
  // pipeline in skia. For every draw with a filter, SkCanvas creates a layer
  // for the draw and modifies the scale for these filters.
  // See SkCanvas::internalSaveLayer.
  SkMatrix mat = options_.canvas->getTotalMatrix();
  SkSize scale;
  if (!mat.isScaleTranslate() && mat.decomposeScale(&scale))
    mat = SkMatrix::MakeScale(scale.width(), scale.height());
  Write(filter.record().get(), gfx::Rect(), gfx::SizeF(1.f, 1.f), mat);
}

void PaintOpWriter::Write(const MergePaintFilter& filter) {
  WriteSize(filter.input_count());
  for (size_t i = 0; i < filter.input_count(); ++i)
    Write(filter.input_at(i));
}

void PaintOpWriter::Write(const MorphologyPaintFilter& filter) {
  WriteSimple(filter.morph_type());
  WriteSimple(filter.radius_x());
  WriteSimple(filter.radius_y());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const OffsetPaintFilter& filter) {
  WriteSimple(filter.dx());
  WriteSimple(filter.dy());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const TilePaintFilter& filter) {
  WriteSimple(filter.src());
  WriteSimple(filter.dst());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const TurbulencePaintFilter& filter) {
  WriteSimple(filter.turbulence_type());
  WriteSimple(filter.base_frequency_x());
  WriteSimple(filter.base_frequency_y());
  WriteSimple(filter.num_octaves());
  WriteSimple(filter.seed());
  WriteSimple(filter.tile_size());
}

void PaintOpWriter::Write(const PaintFlagsPaintFilter& filter) {
  Write(filter.flags());
}

void PaintOpWriter::Write(const MatrixPaintFilter& filter) {
  Write(filter.matrix());
  WriteSimple(filter.filter_quality());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const LightingDistantPaintFilter& filter) {
  WriteSimple(filter.lighting_type());
  WriteSimple(filter.direction());
  WriteSimple(filter.light_color());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const LightingPointPaintFilter& filter) {
  WriteSimple(filter.lighting_type());
  WriteSimple(filter.location());
  WriteSimple(filter.light_color());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const LightingSpotPaintFilter& filter) {
  WriteSimple(filter.lighting_type());
  WriteSimple(filter.location());
  WriteSimple(filter.target());
  WriteSimple(filter.specular_exponent());
  WriteSimple(filter.cutoff_angle());
  WriteSimple(filter.light_color());
  WriteSimple(filter.surface_scale());
  WriteSimple(filter.kconstant());
  WriteSimple(filter.shininess());
  Write(filter.input().get());
}

void PaintOpWriter::Write(const PaintRecord* record,
                          const gfx::Rect& playback_rect,
                          const gfx::SizeF& post_scale,
                          const SkMatrix& post_matrix_for_analysis) {
  AlignMemory(PaintOpBuffer::PaintOpAlign);

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

  // Nested records are used for picture shaders and filters which don't support
  // using lcd text. Make sure we disable it here to match this in the text
  // analysis canvas.
  const bool can_use_lcd_text = false;
  SimpleBufferSerializer serializer(
      memory_, remaining_bytes_, options_.image_provider,
      options_.transfer_cache, options_.paint_cache, options_.strike_server,
      options_.color_space, can_use_lcd_text,
      options_.context_supports_distance_field_text, options_.max_texture_size,
      options_.max_texture_bytes);
  serializer.Serialize(record, playback_rect, post_scale,
                       post_matrix_for_analysis);

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
  memory_ += serializer.written();
  remaining_bytes_ -= serializer.written();
}

void PaintOpWriter::Write(const SkRegion& region) {
  size_t bytes_required = region.writeToMemory(nullptr);
  std::unique_ptr<char[]> data(new char[bytes_required]);
  size_t bytes_written = region.writeToMemory(data.get());
  DCHECK_EQ(bytes_required, bytes_written);

  WriteSize(bytes_written);
  WriteData(bytes_written, data.get());
}

inline void PaintOpWriter::EnsureBytes(size_t required_bytes) {
  if (remaining_bytes_ < required_bytes)
    valid_ = false;
}

}  // namespace cc
