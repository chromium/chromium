// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

#include <algorithm>
#include <utility>

#include "base/memory/values_equivalent.h"
#include "base/notreached.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPathEffect.h"
#include "third_party/skia/include/core/SkPathUtils.h"

namespace {

template <typename T>
bool AreValuesEqualForTesting(const sk_sp<T>& a, const sk_sp<T>& b) {
  return base::ValuesEquivalent(a, b, [](const T& x, const T& y) {
    return x.EqualsForTesting(y);  // IN-TEST
  });
}

}  // namespace

namespace cc {

CorePaintFlags::CorePaintFlags() {
  // Match SkPaint defaults.
  bitfields_uint_ = 0u;
  bitfields_.cap_type_ = SkPaint::kDefault_Cap;
  bitfields_.join_type_ = SkPaint::kDefault_Join;
  bitfields_.style_ = SkPaint::kFill_Style;
  bitfields_.blend_mode_ = static_cast<int>(SkBlendMode::kSrcOver);
  bitfields_.filter_quality_ =
      static_cast<int>(PaintFlags::FilterQuality::kNone);
  bitfields_.dynamic_range_limit_standard_mix_ = 0;
  bitfields_.dynamic_range_limit_constrained_high_mix_ = 0;

  static_assert(sizeof(bitfields_) <= sizeof(bitfields_uint_),
                "Too many bitfields");
}

bool CorePaintFlags::operator==(const CorePaintFlags& other) const {
  return color_ == other.color_ && width_ == other.width_ &&
         miter_limit_ == other.miter_limit_ &&
         bitfields_uint_ == other.bitfields_uint_;
}

PaintFlags::PaintFlags() = default;

PaintFlags::PaintFlags(const PaintFlags& flags) = default;

PaintFlags::PaintFlags(const CorePaintFlags& flags) : CorePaintFlags(flags) {}

PaintFlags::PaintFlags(PaintFlags&& other) = default;

PaintFlags& PaintFlags::operator=(const PaintFlags& other) = default;

PaintFlags& PaintFlags::operator=(PaintFlags&& other) = default;

PaintFlags::~PaintFlags() = default;

bool PaintFlags::CanConvertToCorePaintFlags() const {
  return IsValid() && !path_effect_ && !shader_ && !color_filter_ &&
         !draw_looper_ && !image_filter_;
}

CorePaintFlags PaintFlags::ToCorePaintFlags() const {
  DCHECK(CanConvertToCorePaintFlags());
  return CorePaintFlags(*this);
}

void PaintFlags::setImageFilter(sk_sp<PaintFilter> filter) {
  image_filter_ = std::move(filter);
}

bool PaintFlags::ShaderIsOpaque() const {
  return shader_->IsOpaque();
}

void PaintFlags::setShader(sk_sp<PaintShader> shader) {
  shader_ = std::move(shader);
}

bool PaintFlags::nothingToDraw() const {
  // Duplicated from SkPaint to avoid having to construct an SkPaint to
  // answer this question.
  if (getLooper())
    return false;

  switch (getBlendMode()) {
    case SkBlendMode::kSrcOver:
    case SkBlendMode::kSrcATop:
    case SkBlendMode::kDstOut:
    case SkBlendMode::kDstOver:
    case SkBlendMode::kPlus:
      if (isFullyTransparent()) {
        return !color_filter_ && !image_filter_;
      }
      break;
    case SkBlendMode::kDst:
      return true;
    default:
      break;
  }
  return false;
}

bool PaintFlags::getFillPath(const SkPath& src,
                             SkPath* dst,
                             const SkRect* cull_rect,
                             SkScalar res_scale) const {
  SkPaint paint = ToSkPaint();
  return skpathutils::FillPathWithPaint(src, paint, dst, cull_rect, res_scale);
}

bool PaintFlags::SupportsFoldingAlpha() const {
  if (getBlendMode() != SkBlendMode::kSrcOver) {
    return false;
  }
  if (getColorFilter()) {
    return false;
  }
  if (getImageFilter()) {
    return false;
  }
  if (getLooper()) {
    return false;
  }
  return true;
}

SkPaint PaintFlags::ToSkPaint() const {
  SkPaint paint;
  if (path_effect_) {
    paint.setPathEffect(path_effect_->GetSkPathEffect());
  }
  if (shader_) {
    paint.setShader(shader_->GetSkShader(getFilterQuality()));
  }
  if (color_filter_) {
    paint.setColorFilter(color_filter_->sk_color_filter_);
  }
  if (image_filter_) {
    paint.setImageFilter(image_filter_->cached_sk_filter_);
  }
  paint.setColor(getColor4f());
  paint.setStrokeWidth(getStrokeWidth());
  paint.setStrokeMiter(getStrokeMiter());
  paint.setBlendMode(getBlendMode());
  paint.setAntiAlias(isAntiAlias());
  paint.setDither(isDither());
  paint.setStrokeCap(static_cast<SkPaint::Cap>(getStrokeCap()));
  paint.setStrokeJoin(static_cast<SkPaint::Join>(getStrokeJoin()));
  paint.setStyle(static_cast<SkPaint::Style>(getStyle()));
  return paint;
}

SkSamplingOptions PaintFlags::FilterQualityToSkSamplingOptions(
    PaintFlags::FilterQuality filter_quality) {
  return FilterQualityToSkSamplingOptions(filter_quality,
                                          ScalingOperation::kDefault);
}

SkSamplingOptions PaintFlags::FilterQualityToSkSamplingOptions(
    PaintFlags::FilterQuality filter_quality,
    PaintFlags::ScalingOperation scaling_op) {
  switch (filter_quality) {
    case PaintFlags::FilterQuality::kHigh:
      switch (scaling_op) {
        case PaintFlags::ScalingOperation::kDefault:
          return SkSamplingOptions(SkCubicResampler::CatmullRom());
        case PaintFlags::ScalingOperation::kUnknown:
          return SkSamplingOptions(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
        case PaintFlags::ScalingOperation::kUpscale:
          return SkSamplingOptions(SkCubicResampler::Mitchell());
      }
    case PaintFlags::FilterQuality::kMedium:
      switch (scaling_op) {
        case PaintFlags::ScalingOperation::kDefault:
          return SkSamplingOptions(SkFilterMode::kLinear,
                                   SkMipmapMode::kNearest);
        case PaintFlags::ScalingOperation::kUnknown:
          return SkSamplingOptions(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
        case PaintFlags::ScalingOperation::kUpscale:
          return SkSamplingOptions(SkCubicResampler::Mitchell());
      }
    case PaintFlags::FilterQuality::kLow:
      return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone);
    case PaintFlags::FilterQuality::kNone:
      return SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);
  }
}

bool CorePaintFlags::IsValid() const {
  return PaintOp::IsValidPaintFlagsSkBlendMode(getBlendMode());
}

bool PaintFlags::EqualsForTesting(const PaintFlags& other) const {
  // Can't just ToSkPaint and operator== here as SkPaint does pointer
  // comparisons on all the ref'd skia objects on the SkPaint, which
  // is not true after serialization.
  return getColor() == other.getColor() &&
         getStrokeWidth() == other.getStrokeWidth() &&
         getStrokeMiter() == other.getStrokeMiter() &&
         getBlendMode() == other.getBlendMode() &&
         getStrokeCap() == other.getStrokeCap() &&
         getStrokeJoin() == other.getStrokeJoin() &&
         getStyle() == other.getStyle() &&
         getFilterQuality() == other.getFilterQuality() &&
         getDynamicRangeLimit() == other.getDynamicRangeLimit() &&
         isArcClosed() == other.isArcClosed() &&
         AreValuesEqualForTesting(path_effect_,  // IN-TEST
                                  other.path_effect_) &&
         AreValuesEqualForTesting(color_filter_,  // IN-TEST
                                  other.color_filter_) &&
         AreValuesEqualForTesting(draw_looper_,  // IN-TEST
                                  other.draw_looper_) &&
         AreValuesEqualForTesting(image_filter_,  // IN-TEST
                                  other.image_filter_) &&
         AreValuesEqualForTesting(shader_, other.shader_);  // IN-TEST
}

bool PaintFlags::HasDiscardableImages(
    gfx::ContentColorUsage* content_color_usage) const {
  bool has_discardable_images = false;
  if (shader_) {
    has_discardable_images = shader_->HasDiscardableImages(content_color_usage);
  }
  if (image_filter_ && image_filter_->has_discardable_images()) {
    if (content_color_usage) {
      *content_color_usage =
          std::max(*content_color_usage, image_filter_->GetContentColorUsage());
    }
    has_discardable_images = true;
  }
  return has_discardable_images;
}

}  // namespace cc
