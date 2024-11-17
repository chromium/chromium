// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_shader.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/types/optional_util.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace cc {
namespace {
base::AtomicSequenceNumber g_next_shader_id;

sk_sp<SkPicture> ToSkPicture(const PaintRecord& record,
                             const SkRect& bounds,
                             const gfx::SizeF* raster_scale,
                             ImageProvider* image_provider) {
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(SkRect::MakeWH(bounds.width(), bounds.height()));
  canvas->translate(-bounds.fLeft, -bounds.fTop);
  if (raster_scale)
    canvas->scale(raster_scale->width(), raster_scale->height());
  record.Playback(canvas, PlaybackParams(image_provider));
  return recorder.finishRecordingAsPicture();
}

bool CompareMatricesForTesting(const SkMatrix& a,  // IN-TEST
                               const SkMatrix& b,
                               bool ignore_scaling_differences) {
  if (!ignore_scaling_differences) {
    return a == b;
  }

  SkSize scale;
  SkMatrix a_without_scale;
  SkMatrix b_without_scale;

  const bool decomposes = a.decomposeScale(&scale, &a_without_scale);
  if (decomposes != b.decomposeScale(&scale, &b_without_scale)) {
    return false;
  }
  if (!decomposes) {
    return true;
  }
  return a_without_scale == b_without_scale;
}

}  // namespace

const PaintShader::RecordShaderId PaintShader::kInvalidRecordShaderId = -1;

sk_sp<PaintShader> PaintShader::MakeEmpty() {
  sk_sp<PaintShader> shader(new PaintShader(Type::kEmpty));

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeColor(SkColor4f color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kColor));

  // Just one color. Store it in the fallback color. Easy.
  shader->fallback_color_ = color;

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeLinearGradient(
    const SkPoint points[],
    const SkColor4f colors[],
    const SkScalar pos[],
    int count,
    SkTileMode mode,
    SkGradientShader::Interpolation interpolation,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor4f fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kLinearGradient));

  // There are always two points, the start and the end.
  shader->start_point_ = points[0];
  shader->end_point_ = points[1];
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);
  shader->SetGradientInterpolation(interpolation);

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeRadialGradient(
    const SkPoint& center,
    SkScalar radius,
    const SkColor4f colors[],
    const SkScalar pos[],
    int count,
    SkTileMode mode,
    SkGradientShader::Interpolation interpolation,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor4f fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kRadialGradient));

  shader->center_ = center;
  shader->start_radius_ = shader->end_radius_ = radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);
  shader->SetGradientInterpolation(interpolation);

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeTwoPointConicalGradient(
    const SkPoint& start,
    SkScalar start_radius,
    const SkPoint& end,
    SkScalar end_radius,
    const SkColor4f colors[],
    const SkScalar pos[],
    int count,
    SkTileMode mode,
    SkGradientShader::Interpolation interpolation,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor4f fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kTwoPointConicalGradient));

  shader->start_point_ = start;
  shader->end_point_ = end;
  shader->start_radius_ = start_radius;
  shader->end_radius_ = end_radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);
  shader->SetGradientInterpolation(interpolation);

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeSweepGradient(
    SkScalar cx,
    SkScalar cy,
    const SkColor4f colors[],
    const SkScalar pos[],
    int color_count,
    SkTileMode mode,
    SkScalar start_degrees,
    SkScalar end_degrees,
    SkGradientShader::Interpolation interpolation,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor4f fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kSweepGradient));

  shader->center_ = SkPoint::Make(cx, cy);
  shader->start_degrees_ = start_degrees;
  shader->end_degrees_ = end_degrees;
  shader->SetColorsAndPositions(colors, pos, color_count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);
  shader->SetGradientInterpolation(interpolation);

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeImage(const PaintImage& image,
                                          SkTileMode tx,
                                          SkTileMode ty,
                                          const SkMatrix* local_matrix,
                                          const SkRect* tile_rect) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kImage));

  shader->image_ = image;
  shader->SetMatrixAndTiling(local_matrix, tx, ty);
  if (tile_rect) {
    DCHECK(image.IsPaintWorklet());
    shader->tile_ = *tile_rect;
  }

  shader->ResolveSkObjects();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakePaintRecord(
    PaintRecord record,
    const SkRect& tile,
    SkTileMode tx,
    SkTileMode ty,
    const SkMatrix* local_matrix,
    ScalingBehavior scaling_behavior) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kPaintRecord));

  shader->record_ = std::move(record);
  shader->id_ = g_next_shader_id.GetNext();
  shader->tile_ = tile;
  shader->scaling_behavior_ = scaling_behavior;
  shader->SetMatrixAndTiling(local_matrix, tx, ty);

  shader->ResolveSkObjects();
  return shader;
}

// static
size_t PaintShader::GetSerializedSize(const PaintShader* shader) {
  if (!shader) {
    return PaintOpWriter::SerializedSize<bool>();
  }

  return (base::CheckedNumeric<size_t>(PaintOpWriter::SerializedSize<bool>()) +
          PaintOpWriter::SerializedSize(shader->shader_type_) +
          PaintOpWriter::SerializedSize(shader->flags_) +
          PaintOpWriter::SerializedSize(shader->end_radius_) +
          PaintOpWriter::SerializedSize(shader->start_radius_) +
          PaintOpWriter::SerializedSize(shader->tx_) +
          PaintOpWriter::SerializedSize(shader->ty_) +
          PaintOpWriter::SerializedSize(shader->fallback_color_) +
          PaintOpWriter::SerializedSize(shader->scaling_behavior_) +
          PaintOpWriter::SerializedSize(shader->local_matrix_) +
          PaintOpWriter::SerializedSize(shader->center_) +
          PaintOpWriter::SerializedSize(shader->tile_) +
          PaintOpWriter::SerializedSize(shader->start_point_) +
          PaintOpWriter::SerializedSize(shader->end_point_) +
          PaintOpWriter::SerializedSize(shader->start_degrees_) +
          PaintOpWriter::SerializedSize(shader->end_degrees_) +
          PaintOpWriter::SerializedSize(shader->gradient_interpolation_) +
          PaintOpWriter::SerializedSize(shader->image_) +
          PaintOpWriter::SerializedSize(shader->id_) +
          PaintOpWriter::SerializedSize(shader->record_) +
          PaintOpWriter::SerializedSizeOfElements(shader->colors_.data(),
                                                  shader->colors_.size()) +
          PaintOpWriter::SerializedSizeOfElements(shader->positions_.data(),
                                                  shader->positions_.size()))
      .ValueOrDie();
}

PaintShader::PaintShader(Type type) : shader_type_(type) {}
PaintShader::~PaintShader() = default;

bool PaintShader::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  switch (shader_type_) {
    case Type::kEmpty:
    case Type::kColor:
    case Type::kLinearGradient:
    case Type::kRadialGradient:
    case Type::kTwoPointConicalGradient:
    case Type::kSweepGradient:
      return false;
    case Type::kImage:
      if (image_ && !image_.IsTextureBacked()) {
        if (content_color_usage) {
          *content_color_usage =
              std::max(*content_color_usage, image_.GetContentColorUsage());
        }
        return true;
      }
      return false;
    case Type::kPaintRecord:
      if (record_ && record_->has_discardable_images()) {
        if (content_color_usage) {
          *content_color_usage =
              std::max(*content_color_usage, record_->content_color_usage());
        }
        return true;
      }
      return false;
    case Type::kShaderCount:
      NOTREACHED();
  }
}

bool PaintShader::GetClampedRasterizationTileRect(const SkMatrix& ctm,
                                                  int max_texture_size,
                                                  SkRect* tile_rect) const {
  DCHECK_EQ(shader_type_, Type::kPaintRecord);

  // If we are using a fixed scale, the record is rasterized with the original
  // tile size and scaling is applied to the generated output.
  if (scaling_behavior_ == ScalingBehavior::kFixedScale) {
    *tile_rect = tile_;
    return true;
  }

  SkMatrix matrix = ctm;
  if (local_matrix_.has_value())
    matrix.preConcat(local_matrix_.value());

  *tile_rect =
      PaintRecord::GetFixedScaleBounds(matrix, tile_, max_texture_size);
  if (tile_rect->isEmpty())
    return false;
  return true;
}

sk_sp<PaintShader> PaintShader::CreateScaledPaintRecord(
    const SkMatrix& ctm,
    int max_texture_size,
    gfx::SizeF* raster_scale) const {
  DCHECK_EQ(shader_type_, Type::kPaintRecord);

  // If this is already fixed scale, then this is already good to go.
  if (scaling_behavior_ == ScalingBehavior::kFixedScale) {
    *raster_scale = gfx::SizeF(1.f, 1.f);
    return sk_ref_sp<PaintShader>(this);
  }

  // For creating a decoded PaintRecord shader, we need to do the following:
  // 1) Figure out the scale at which the record should be rasterization given
  //    the ctm and local_matrix on the shader.
  // 2) Transform this record to an SkPicture with this scale and replace
  //    encoded images in this record with decodes from the ImageProvider. This
  //    is done by setting the raster_matrix_ for this shader to be used
  //    in GetSkShader.
  // 3) Since the SkShader will use a scaled SkPicture, we use a kFixedScale for
  //    the decoded shader which creates an SkPicture backed SkImage for
  //    creating the decoded SkShader.
  // Note that the scaling logic here is replicated from
  // SkPictureShader::refBitmapShader.
  SkRect tile_rect;
  if (!GetClampedRasterizationTileRect(ctm, max_texture_size, &tile_rect))
    return nullptr;

  *raster_scale =
      gfx::SizeF(SkIntToScalar(tile_rect.width()) / tile_.width(),
                 SkIntToScalar(tile_rect.height()) / tile_.height());
  SkMatrix local_matrix_with_inv_raster_scale = GetLocalMatrix();
  local_matrix_with_inv_raster_scale.preScale(1 / raster_scale->width(),
                                              1 / raster_scale->height());

  sk_sp<PaintShader> shader(new PaintShader(Type::kPaintRecord));
  shader->record_ = record_;
  shader->id_ = id_;
  shader->tile_ = tile_rect;
  // Use a fixed scale since we have already scaled the tile rect and fixed the
  // raster scale.
  shader->scaling_behavior_ = ScalingBehavior::kFixedScale;
  shader->SetMatrixAndTiling(&local_matrix_with_inv_raster_scale, tx_, ty_);
  return shader;
}

sk_sp<PaintShader> PaintShader::CreatePaintWorkletRecord(
    ImageProvider* image_provider) const {
  DCHECK_EQ(shader_type_, Type::kImage);
  DCHECK(image_ && image_.IsPaintWorklet());

  ImageProvider::ScopedResult result =
      image_provider->GetRasterContent(DrawImage(image_));
  if (!result || !result.has_paint_record()) {
    return nullptr;
  }
  SkMatrix local_matrix = GetLocalMatrix();
  return PaintShader::MakePaintRecord(result.ReleaseAsRecord(), tile_, tx_, ty_,
                                      &local_matrix);
}

sk_sp<PaintShader> PaintShader::CreateDecodedImage(
    const SkMatrix& ctm,
    PaintFlags::FilterQuality quality,
    ImageProvider* image_provider,
    uint32_t* transfer_cache_entry_id,
    PaintFlags::FilterQuality* raster_quality,
    bool* needs_mips,
    gpu::Mailbox* mailbox) const {
  DCHECK_EQ(shader_type_, Type::kImage);
  if (!image_)
    return nullptr;

  SkMatrix total_image_matrix = GetLocalMatrix();
  total_image_matrix.preConcat(ctm);
  SkRect src_rect = SkRect::MakeIWH(image_.width(), image_.height());
  SkIRect int_src_rect;
  src_rect.roundOut(&int_src_rect);
  DrawImage draw_image(image_, false, int_src_rect, quality,
                       SkM44(total_image_matrix));
  auto decoded_draw_image = image_provider->GetRasterContent(draw_image);
  if (!decoded_draw_image)
    return nullptr;

  auto decoded_image = decoded_draw_image.decoded_image();
  SkMatrix final_matrix = GetLocalMatrix();
  bool need_scale = !decoded_image.is_scale_adjustment_identity();
  if (need_scale) {
    final_matrix.preScale(1.f / decoded_image.scale_adjustment().width(),
                          1.f / decoded_image.scale_adjustment().height());
  }

  PaintImage decoded_paint_image;
  if (decoded_image.transfer_cache_entry_id()) {
    decoded_paint_image = image_;
    *transfer_cache_entry_id = *decoded_image.transfer_cache_entry_id();
  } else if (!decoded_image.mailbox().IsZero()) {
    decoded_paint_image = image_;
    *mailbox = decoded_image.mailbox();
  } else {
    DCHECK(decoded_image.image());

    sk_sp<SkImage> sk_image =
        sk_ref_sp<SkImage>(const_cast<SkImage*>(decoded_image.image().get()));
    decoded_paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(image_.stable_id())
            .set_texture_image(std::move(sk_image),
                               image_.GetContentIdForFrame(0u))
            .TakePaintImage();
  }

  // TODO(khushalsagar): Remove filter quality from DecodedDrawImage. All we
  // want to do is cap the filter quality used, but Gpu and Sw cache have
  // different behaviour. D:
  *raster_quality = decoded_image.filter_quality();
  *needs_mips = decoded_image.transfer_cache_entry_needs_mips();
  return PaintShader::MakeImage(decoded_paint_image, tx_, ty_, &final_matrix);
}

sk_sp<SkShader> PaintShader::GetSkShader(
    PaintFlags::FilterQuality quality) const {
  SkSamplingOptions sampling(PaintFlags::FilterQualityToSkSamplingOptions(
      quality, PaintFlags::ScalingOperation::kUnknown));

  switch (shader_type_) {
    case Type::kEmpty:
      return SkShaders::Empty();
    case Type::kColor:
      // This will be handled by the fallback check below.
      break;
    case Type::kLinearGradient: {
      SkPoint points[2] = {start_point_, end_point_};
      return SkGradientShader::MakeLinear(
          points, colors_.data(), nullptr /*sk_sp<SkColorSpace>*/,
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, gradient_interpolation_,
          base::OptionalToPtr(local_matrix_));
    }
    case Type::kRadialGradient:
      return SkGradientShader::MakeRadial(
          center_, start_radius_, colors_.data(),
          nullptr /*sk_sp<SkColorSpace>*/,
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, gradient_interpolation_,
          base::OptionalToPtr(local_matrix_));
    case Type::kTwoPointConicalGradient:
      return SkGradientShader::MakeTwoPointConical(
          start_point_, start_radius_, end_point_, end_radius_, colors_.data(),
          nullptr /*sk_sp<SkColorSpace>*/,
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, gradient_interpolation_,
          base::OptionalToPtr(local_matrix_));
    case Type::kSweepGradient:
      return SkGradientShader::MakeSweep(
          center_.x(), center_.y(), colors_.data(),
          nullptr /*sk_sp<SkColorSpace>*/,
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, start_degrees_, end_degrees_,
          gradient_interpolation_, base::OptionalToPtr(local_matrix_));
    case Type::kImage:
      if (sk_cached_image_) {
        return sk_cached_image_->makeShader(tx_, ty_, sampling,
                                            base::OptionalToPtr(local_matrix_));
      }
      break;
    case Type::kPaintRecord:
      if (sk_cached_picture_) {
        switch (scaling_behavior_) {
          // For raster scale, we create a picture shader directly.
          case ScalingBehavior::kRasterAtScale:
            return sk_cached_picture_->makeShader(
                tx_, ty_, sampling.filter, base::OptionalToPtr(local_matrix_),
                nullptr);
          // For fixed scale, we create an image shader with an image backed by
          // the picture.
          case ScalingBehavior::kFixedScale: {
            auto image = SkImages::DeferredFromPicture(
                sk_cached_picture_,
                SkISize::Make(tile_.width(), tile_.height()), nullptr, nullptr,
                SkImages::BitDepth::kU8, SkColorSpace::MakeSRGB());
            return image->makeShader(tx_, ty_, sampling,
                                     base::OptionalToPtr(local_matrix_));
          }
        }
        break;
      }
      break;
    case Type::kShaderCount:
      NOTREACHED();
  }

  // If we didn't create a shader for whatever reason, create a fallback
  // color one.
  return SkShaders::Color(fallback_color_.toSkColor());
}

void PaintShader::ResolveSkObjects(const gfx::SizeF* raster_scale,
                                   ImageProvider* image_provider) {
  switch (shader_type_) {
    case Type::kImage:
      if (image_ && !image_.IsPaintWorklet()) {
        sk_cached_image_ = image_.GetSkImage();
      }
      break;
    case Type::kPaintRecord: {
      // Create a recording at the desired scale if this record has images
      // which have been decoded before raster.
      sk_cached_picture_ =
          ToSkPicture(*record_, tile_, raster_scale, image_provider);
      break;
    }
    default:
      break;
  }
}

void PaintShader::SetColorsAndPositions(const SkColor4f* colors,
                                        const SkScalar* positions,
                                        int count) {
#if DCHECK_IS_ON()
  static const int kMaxShaderColorsSupported = 10000;
  DCHECK_GE(count, 2);
  DCHECK_LE(count, kMaxShaderColorsSupported);
#endif
  colors_.assign(colors, colors + count);
  if (positions)
    positions_.assign(positions, positions + count);
}

void PaintShader::SetMatrixAndTiling(const SkMatrix* matrix,
                                     SkTileMode tx,
                                     SkTileMode ty) {
  if (matrix) {
    local_matrix_ = *matrix;
    // The matrix type is mutable and set lazily. Force it to be computed here
    // to avoid a data race from the lazy computation happening on a worker
    // thread.
    local_matrix_->getType();
  }
  tx_ = tx;
  ty_ = ty;
}

void PaintShader::SetFlagsAndFallback(uint32_t flags,
                                      SkColor4f fallback_color) {
  flags_ = flags;
  fallback_color_ = fallback_color;
}

bool PaintShader::IsOpaque() const {
  switch (shader_type_) {
    case Type::kEmpty:
      return false;
    case Type::kColor:
      // This will be handled by the fallback check below.
      break;
    case Type::kLinearGradient:  // fall-through
    case Type::kRadialGradient:  // fall-through
    case Type::kSweepGradient:
      if (tx_ == SkTileMode::kDecal)
        return false;
      for (const auto& c : colors_)
        if (!c.isOpaque())
          return false;
      return true;
    case Type::kTwoPointConicalGradient:
      // Because two-point-conical can sometimes ignore its tiling and not draw
      // everywhere, we conservatively return false here. If we measure
      // performance reasons to be more aggressive here, we can ask Skia to
      // expose private functionality to compute this with having to actually
      // instantiate a sk_shader object.
      return false;
    case Type::kImage:
      if (tx_ == SkTileMode::kDecal || ty_ == SkTileMode::kDecal)
        return false;
      if (sk_cached_image_)
        return sk_cached_image_->isOpaque();
      return false;
    case Type::kPaintRecord:
      return false;
    case Type::kShaderCount:
      NOTREACHED();
  }
  return fallback_color_.isOpaque();
}

bool PaintShader::IsValid() const {
  switch (shader_type_) {
    case Type::kEmpty:
    case Type::kColor:
      return true;
    case Type::kSweepGradient:
      if (!std::isfinite(start_degrees_) || !std::isfinite(end_degrees_) ||
          start_degrees_ >= end_degrees_) {
        return false;
      }
      [[fallthrough]];
    case Type::kLinearGradient:
    case Type::kRadialGradient:
    case Type::kTwoPointConicalGradient:
      return colors_.size() >= 2 &&
             (positions_.empty() || positions_.size() == colors_.size());
    case Type::kImage:
      // We may not be able to decode the image, in which case it would be
      // false, but that would still make a valid shader.
      return true;
    case Type::kPaintRecord:
      return !!record_;
    case Type::kShaderCount:
      return false;
  }
  return false;
}

bool PaintShader::EqualsForTesting(const PaintShader& other) const {
  if (shader_type_ != other.shader_type_)
    return false;

  // Record and image shaders are scaled during serialization.
  const bool ignore_scaling_differences =
      shader_type_ == PaintShader::Type::kPaintRecord ||
      shader_type_ == PaintShader::Type::kImage;

  // Variables that all shaders use.
  const SkMatrix& local_matrix = local_matrix_ ? *local_matrix_ : SkMatrix::I();
  const SkMatrix& other_local_matrix =
      other.local_matrix_ ? *other.local_matrix_ : SkMatrix::I();
  if (!CompareMatricesForTesting(local_matrix, other_local_matrix,  // IN-TEST
                                 ignore_scaling_differences)) {
    return false;
  }

  if (fallback_color_ != other.fallback_color_ || flags_ != other.flags_ ||
      tx_ != other.tx_ || ty_ != other.ty_) {
    return false;
  }
  if (!ignore_scaling_differences &&
      scaling_behavior_ != other.scaling_behavior_) {
    return false;
  }

  // Variables that only some shaders use.
  switch (shader_type_) {
    case Type::kEmpty:
    case Type::kColor:
      break;
    case Type::kSweepGradient:
      if (start_degrees_ != other.start_degrees_ ||
          end_degrees_ != other.end_degrees_) {
        return false;
      }
      [[fallthrough]];
    case Type::kLinearGradient:
    case Type::kRadialGradient:
    case Type::kTwoPointConicalGradient:
      if (start_radius_ != other.start_radius_ ||
          end_radius_ != other.end_radius_ || center_ != other.center_ ||
          start_point_ != other.start_point_ ||
          end_point_ != other.end_point_ || colors_ != other.colors_ ||
          positions_ != other.positions_) {
        return false;
      }
      break;
    case Type::kImage:
      // TODO(enne): add comparison of images once those are serialized.
      break;
    case Type::kPaintRecord:
      // If we have a record but not other.record, or vice versa, then shaders
      // aren't the same.
      if (!record_ != !other.record_)
        return false;
      // tile_ and record_ intentionally omitted since they are modified on the
      // serialized shader based on the ctm.
      break;
    case Type::kShaderCount:
      break;
  }

  return true;
}

}  // namespace cc
