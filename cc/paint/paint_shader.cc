// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_shader.h"

#include "base/atomic_sequence_num.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace cc {
namespace {
base::AtomicSequenceNumber g_next_shader_id;

sk_sp<SkPicture> ToSkPicture(sk_sp<PaintRecord> record,
                             const SkRect& bounds,
                             const gfx::SizeF* raster_scale,
                             ImageProvider* image_provider) {
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(SkRect::MakeWH(bounds.width(), bounds.height()));
  canvas->translate(-bounds.fLeft, -bounds.fTop);
  if (raster_scale)
    canvas->scale(raster_scale->width(), raster_scale->height());
  record->Playback(canvas, PlaybackParams(image_provider));
  return recorder.finishRecordingAsPicture();
}

bool CompareMatrices(const SkMatrix& a,
                     const SkMatrix& b,
                     bool ignore_scaling_differences) {
  if (!ignore_scaling_differences)
    return PaintOp::AreSkMatricesEqual(a, b);

  SkSize scale;
  SkMatrix a_without_scale;
  SkMatrix b_without_scale;

  const bool decomposes = a.decomposeScale(&scale, &a_without_scale);
  if (decomposes != b.decomposeScale(&scale, &b_without_scale))
    return false;

  if (!decomposes)
    return true;
  return PaintOp::AreSkMatricesEqual(a_without_scale, b_without_scale);
}

SkRect AdjustForMaxTextureSize(SkRect tile, int max_texture_size) {
  if (max_texture_size == 0)
    return tile;

  if (tile.width() < max_texture_size && tile.height() < max_texture_size)
    return tile;

  float down_scale = max_texture_size / std::max(tile.width(), tile.height());
  tile = SkRect::MakeXYWH(tile.x(), tile.y(),
                          SkScalarFloorToScalar(tile.width() * down_scale),
                          SkScalarFloorToScalar(tile.height() * down_scale));
  return tile;
}

}  // namespace

const PaintShader::RecordShaderId PaintShader::kInvalidRecordShaderId = -1;

sk_sp<PaintShader> PaintShader::MakeEmpty() {
  sk_sp<PaintShader> shader(new PaintShader(Type::kEmpty));

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeColor(SkColor color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kColor));

  // Just one color. Store it in the fallback color. Easy.
  shader->fallback_color_ = color;

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeLinearGradient(const SkPoint points[],
                                                   const SkColor colors[],
                                                   const SkScalar pos[],
                                                   int count,
                                                   SkTileMode mode,
                                                   uint32_t flags,
                                                   const SkMatrix* local_matrix,
                                                   SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kLinearGradient));

  // There are always two points, the start and the end.
  shader->start_point_ = points[0];
  shader->end_point_ = points[1];
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeRadialGradient(const SkPoint& center,
                                                   SkScalar radius,
                                                   const SkColor colors[],
                                                   const SkScalar pos[],
                                                   int count,
                                                   SkTileMode mode,
                                                   uint32_t flags,
                                                   const SkMatrix* local_matrix,
                                                   SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kRadialGradient));

  shader->center_ = center;
  shader->start_radius_ = shader->end_radius_ = radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeTwoPointConicalGradient(
    const SkPoint& start,
    SkScalar start_radius,
    const SkPoint& end,
    SkScalar end_radius,
    const SkColor colors[],
    const SkScalar pos[],
    int count,
    SkTileMode mode,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kTwoPointConicalGradient));

  shader->start_point_ = start;
  shader->end_point_ = end;
  shader->start_radius_ = start_radius;
  shader->end_radius_ = end_radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeSweepGradient(SkScalar cx,
                                                  SkScalar cy,
                                                  const SkColor colors[],
                                                  const SkScalar pos[],
                                                  int color_count,
                                                  SkTileMode mode,
                                                  SkScalar start_degrees,
                                                  SkScalar end_degrees,
                                                  uint32_t flags,
                                                  const SkMatrix* local_matrix,
                                                  SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kSweepGradient));

  shader->center_ = SkPoint::Make(cx, cy);
  shader->start_degrees_ = start_degrees;
  shader->end_degrees_ = end_degrees;
  shader->SetColorsAndPositions(colors, pos, color_count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
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

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakePaintRecord(
    sk_sp<PaintRecord> record,
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

  shader->CreateSkShader();
  return shader;
}

// static
size_t PaintShader::GetSerializedSize(const PaintShader* shader) {
  size_t bool_size = sizeof(bool);
  if (!shader)
    return bool_size;

  return bool_size + sizeof(shader->shader_type_) + sizeof(shader->flags_) +
         sizeof(shader->end_radius_) + sizeof(shader->start_radius_) +
         sizeof(shader->tx_) + sizeof(shader->ty_) +
         sizeof(shader->fallback_color_) + sizeof(shader->scaling_behavior_) +
         bool_size + sizeof(*shader->local_matrix_) + sizeof(shader->center_) +
         sizeof(shader->tile_) + sizeof(shader->start_point_) +
         sizeof(shader->end_point_) + sizeof(shader->start_degrees_) +
         sizeof(shader->end_degrees_) +
         PaintOpWriter::GetImageSize(shader->image_) +
         PaintOpWriter::GetImageSize(shader->image_) + bool_size +
         sizeof(shader->id_) +
         PaintOpWriter::GetRecordSize(shader->record_.get()) +
         sizeof(shader->colors_.size()) +
         shader->colors_.size() * sizeof(SkColor) +
         sizeof(shader->positions_.size()) +
         shader->positions_.size() * sizeof(SkScalar);
}

PaintShader::PaintShader(Type type) : shader_type_(type) {}
PaintShader::~PaintShader() = default;

bool PaintShader::has_discardable_images() const {
  return (image_ && !image_.IsTextureBacked()) ||
         (record_ && record_->HasDiscardableImages());
}

bool PaintShader::GetRasterizationTileRect(const SkMatrix& ctm,
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

  SkSize scale;
  if (!matrix.decomposeScale(&scale)) {
    // Decomposition failed, use an approximation.
    scale.set(SkScalarSqrt(matrix.getScaleX() * matrix.getScaleX() +
                           matrix.getSkewX() * matrix.getSkewX()),
              SkScalarSqrt(matrix.getScaleY() * matrix.getScaleY() +
                           matrix.getSkewY() * matrix.getSkewY()));
  }

  SkScalar tile_area =
      tile_.width() * tile_.height() * scale.width() * scale.height();

  // Clamp the tile size to about 4M pixels.
  // TODO(khushalsagar): We need to consider the max texture size as well.
  static const SkScalar kMaxTileArea = 2048 * 2048;
  if (tile_area > kMaxTileArea) {
    SkScalar clamp_scale = SkScalarSqrt(kMaxTileArea / tile_area);
    scale.set(clamp_scale, clamp_scale);
  }

  *tile_rect = SkRect::MakeXYWH(
      tile_.fLeft * scale.width(), tile_.fTop * scale.height(),
      SkScalarCeilToInt(SkScalarAbs(scale.width() * tile_.width())),
      SkScalarCeilToInt(SkScalarAbs(scale.height() * tile_.height())));

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
  if (!GetRasterizationTileRect(ctm, &tile_rect))
    return nullptr;
  tile_rect = AdjustForMaxTextureSize(tile_rect, max_texture_size);

  sk_sp<PaintShader> shader(new PaintShader(Type::kPaintRecord));
  shader->record_ = record_;
  shader->id_ = id_;
  shader->tile_ = tile_rect;
  // Use a fixed scale since we have already scaled the tile rect and fixed the
  // raster scale.
  shader->scaling_behavior_ = ScalingBehavior::kFixedScale;
  shader->tx_ = tx_;
  shader->ty_ = ty_;

  *raster_scale =
      gfx::SizeF(SkIntToScalar(tile_rect.width()) / tile_.width(),
                 SkIntToScalar(tile_rect.height()) / tile_.height());
  shader->local_matrix_ = GetLocalMatrix();
  shader->local_matrix_->preScale(1 / raster_scale->width(),
                                  1 / raster_scale->height());

  return shader;
}

sk_sp<PaintShader> PaintShader::CreateDecodedImage(
    const SkMatrix& ctm,
    SkFilterQuality quality,
    ImageProvider* image_provider,
    uint32_t* transfer_cache_entry_id,
    SkFilterQuality* raster_quality,
    bool* needs_mips) const {
  DCHECK_EQ(shader_type_, Type::kImage);
  if (!image_)
    return nullptr;

  SkMatrix total_image_matrix = GetLocalMatrix();
  total_image_matrix.preConcat(ctm);
  SkRect src_rect = SkRect::MakeIWH(image_.width(), image_.height());
  SkIRect int_src_rect;
  src_rect.roundOut(&int_src_rect);
  DrawImage draw_image(image_, int_src_rect, quality, total_image_matrix);
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
  } else {
    DCHECK(decoded_image.image());

    sk_sp<SkImage> sk_image =
        sk_ref_sp<SkImage>(const_cast<SkImage*>(decoded_image.image().get()));
    decoded_paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(image_.stable_id())
            .set_image(std::move(sk_image), image_.content_id())
            .TakePaintImage();
  }

  // TODO(khushalsagar): Remove filter quality from DecodedDrawImage. All we
  // want to do is cap the filter quality used, but Gpu and Sw cache have
  // different behaviour. D:
  *raster_quality = decoded_image.filter_quality();
  *needs_mips = decoded_image.transfer_cache_entry_needs_mips();
  return PaintShader::MakeImage(decoded_paint_image, tx_, ty_, &final_matrix);
}

sk_sp<SkShader> PaintShader::GetSkShader() const {
  return cached_shader_;
}

void PaintShader::CreateSkShader(const gfx::SizeF* raster_scale,
                                 ImageProvider* image_provider) {
  DCHECK(!cached_shader_);

  switch (shader_type_) {
    case Type::kEmpty:
      cached_shader_ = SkShaders::Empty();
      break;
    case Type::kColor:
      // This will be handled by the fallback check below.
      break;
    case Type::kLinearGradient: {
      SkPoint points[2] = {start_point_, end_point_};
      cached_shader_ = SkGradientShader::MakeLinear(
          points, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          base::OptionalOrNullptr(local_matrix_));
      break;
    }
    case Type::kRadialGradient:
      cached_shader_ = SkGradientShader::MakeRadial(
          center_, start_radius_, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          base::OptionalOrNullptr(local_matrix_));
      break;
    case Type::kTwoPointConicalGradient:
      cached_shader_ = SkGradientShader::MakeTwoPointConical(
          start_point_, start_radius_, end_point_, end_radius_, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          base::OptionalOrNullptr(local_matrix_));
      break;
    case Type::kSweepGradient:
      cached_shader_ = SkGradientShader::MakeSweep(
          center_.x(), center_.y(), colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, start_degrees_, end_degrees_,
          flags_, base::OptionalOrNullptr(local_matrix_));
      break;
    case Type::kImage:
      if (image_ && !image_.IsPaintWorklet()) {
        cached_shader_ = image_.GetSkImage()->makeShader(
            tx_, ty_, base::OptionalOrNullptr(local_matrix_));
      }
      break;
    case Type::kPaintRecord: {
      // Create a recording at the desired scale if this record has images which
      // have been decoded before raster.
      auto picture = ToSkPicture(record_, tile_, raster_scale, image_provider);

      switch (scaling_behavior_) {
        // For raster scale, we create a picture shader directly.
        case ScalingBehavior::kRasterAtScale:
          cached_shader_ = picture->makeShader(
              tx_, ty_, base::OptionalOrNullptr(local_matrix_), nullptr);
          break;
        // For fixed scale, we create an image shader with an image backed by
        // the picture.
        case ScalingBehavior::kFixedScale: {
          auto image = SkImage::MakeFromPicture(
              std::move(picture), SkISize::Make(tile_.width(), tile_.height()),
              nullptr, nullptr, SkImage::BitDepth::kU8,
              SkColorSpace::MakeSRGB());
          cached_shader_ = image->makeShader(
              tx_, ty_, base::OptionalOrNullptr(local_matrix_));
          break;
        }
      }
      break;
    }
    case Type::kShaderCount:
      NOTREACHED();
      break;
  }

  // If we didn't create a shader for whatever reason, create a fallback color
  // one.
  if (!cached_shader_)
    cached_shader_ = SkShaders::Color(fallback_color_);
}

void PaintShader::SetColorsAndPositions(const SkColor* colors,
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
  if (matrix)
    local_matrix_ = *matrix;
  tx_ = tx;
  ty_ = ty;
}

void PaintShader::SetFlagsAndFallback(uint32_t flags, SkColor fallback_color) {
  flags_ = flags;
  fallback_color_ = fallback_color;
}

bool PaintShader::IsOpaque() const {
  // TODO(enne): don't create a shader to answer this.
  return GetSkShader()->isOpaque();
}

bool PaintShader::IsValid() const {
  // If we managed to create a shader already, then we should be valid.
  if (cached_shader_)
    return true;

  switch (shader_type_) {
    case Type::kEmpty:
    case Type::kColor:
      return true;
    case Type::kSweepGradient:
      if (!std::isfinite(start_degrees_) || !std::isfinite(end_degrees_) ||
          start_degrees_ >= end_degrees_) {
        return false;
      }
      FALLTHROUGH;
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

bool PaintShader::operator==(const PaintShader& other) const {
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
  if (!CompareMatrices(local_matrix, other_local_matrix,
                       ignore_scaling_differences)) {
    return false;
  }

  if (fallback_color_ != other.fallback_color_)
    return false;
  if (flags_ != other.flags_)
    return false;
  if (tx_ != other.tx_)
    return false;
  if (ty_ != other.ty_)
    return false;

  if (!ignore_scaling_differences &&
      scaling_behavior_ != other.scaling_behavior_)
    return false;

  // Variables that only some shaders use.
  switch (shader_type_) {
    case Type::kEmpty:
    case Type::kColor:
      break;
    case Type::kSweepGradient:
      if (!PaintOp::AreEqualEvenIfNaN(start_degrees_, other.start_degrees_))
        return false;
      if (!PaintOp::AreEqualEvenIfNaN(end_degrees_, other.end_degrees_))
        return false;
      FALLTHROUGH;
    case Type::kLinearGradient:
    case Type::kRadialGradient:
    case Type::kTwoPointConicalGradient:
      if (!PaintOp::AreEqualEvenIfNaN(start_radius_, other.start_radius_))
        return false;
      if (!PaintOp::AreEqualEvenIfNaN(end_radius_, other.end_radius_))
        return false;
      if (!PaintOp::AreSkPointsEqual(center_, other.center_))
        return false;
      if (!PaintOp::AreSkPointsEqual(start_point_, other.start_point_))
        return false;
      if (!PaintOp::AreSkPointsEqual(end_point_, other.end_point_))
        return false;
      if (colors_ != other.colors_)
        return false;
      if (positions_.size() != other.positions_.size())
        return false;
      for (size_t i = 0; i < positions_.size(); ++i) {
        if (!PaintOp::AreEqualEvenIfNaN(positions_[i], other.positions_[i]))
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
