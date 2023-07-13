// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

#include <utility>

#include "base/memory/values_equivalent.h"
#include "base/notreached.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPathUtils.h"

namespace {

template <typename T>
bool AreValuesEqualForTesting(const sk_sp<T>& a, const sk_sp<T>& b) {
  return base::ValuesEquivalent(a, b, [](const T& x, const T& y) {
    return x.EqualsForTesting(y);  // IN-TEST
  });
}

bool AreSkFlattenablesEqualForTesting(const sk_sp<SkFlattenable> a,  // IN-TEST
                                      const sk_sp<SkFlattenable> b) {
  return base::ValuesEquivalent(
      a, b, [](const SkFlattenable& x, const SkFlattenable& y) {
        return x.serialize()->equals(y.serialize().get());
      });
}

}  // namespace

namespace cc {

PaintFlags::PaintFlags() {
  // Match SkPaint defaults.
  bitfields_uint_ = 0u;
  bitfields_.cap_type_ = SkPaint::kDefault_Cap;
  bitfields_.join_type_ = SkPaint::kDefault_Join;
  bitfields_.style_ = SkPaint::kFill_Style;
  bitfields_.filter_quality_ =
      static_cast<int>(PaintFlags::FilterQuality::kNone);

  static_assert(sizeof(bitfields_) <= sizeof(bitfields_uint_),
                "Too many bitfields");
}

PaintFlags::PaintFlags(const PaintFlags& flags) = default;

PaintFlags::PaintFlags(PaintFlags&& other) = default;

PaintFlags::~PaintFlags() {
  // TODO(enne): non-default dtor to investigate http://crbug.com/790915

  // Sanity check accessing this object doesn't crash.
  blend_mode_ = static_cast<uint32_t>(SkBlendMode::kLastMode);

  // Free refcounted objects one by one.
  path_effect_.reset();
  shader_.reset();
  mask_filter_.reset();
  color_filter_.reset();
  draw_looper_.reset();
  image_filter_.reset();
}

PaintFlags& PaintFlags::operator=(const PaintFlags& other) = default;

PaintFlags& PaintFlags::operator=(PaintFlags&& other) = default;

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
      if (getAlpha() == 0) {
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
  paint.setPathEffect(path_effect_);
  if (shader_)
    paint.setShader(shader_->GetSkShader(getFilterQuality()));
  paint.setMaskFilter(mask_filter_);
  if (color_filter_) {
    paint.setColorFilter(color_filter_->sk_color_filter_);
  }
  if (image_filter_)
    paint.setImageFilter(image_filter_->cached_sk_filter_);
  paint.setColor(color_);
  paint.setStrokeWidth(width_);
  paint.setStrokeMiter(miter_limit_);
  paint.setBlendMode(getBlendMode());
  paint.setAntiAlias(bitfields_.antialias_);
  paint.setDither(bitfields_.dither_);
  paint.setStrokeCap(static_cast<SkPaint::Cap>(getStrokeCap()));
  paint.setStrokeJoin(static_cast<SkPaint::Join>(getStrokeJoin()));
  paint.setStyle(static_cast<SkPaint::Style>(getStyle()));
  return paint;
}

SkSamplingOptions PaintFlags::FilterQualityToSkSamplingOptions(
    PaintFlags::FilterQuality filter_quality) {
  switch (filter_quality) {
    case PaintFlags::FilterQuality::kHigh:
      return SkSamplingOptions(SkCubicResampler::CatmullRom());
    case PaintFlags::FilterQuality::kMedium:
      return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest);
    case PaintFlags::FilterQuality::kLow:
      return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone);
    case PaintFlags::FilterQuality::kNone:
      return SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);
    default:
      NOTREACHED();
  }
}

bool PaintFlags::IsValid() const {
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
         AreSkFlattenablesEqualForTesting(path_effect_,  // IN-TEST
                                          other.path_effect_) &&
         AreSkFlattenablesEqualForTesting(mask_filter_,  // IN-TEST
                                          other.mask_filter_) &&
         AreValuesEqualForTesting(color_filter_,  // IN-TEST
                                  other.color_filter_) &&
         AreSkFlattenablesEqualForTesting(draw_looper_,  // IN-TEST
                                          other.draw_looper_) &&
         AreValuesEqualForTesting(image_filter_,  // IN-TEST
                                  other.image_filter_) &&
         AreValuesEqualForTesting(shader_, other.shader_);  // IN-TEST
}

bool PaintFlags::HasDiscardableImages() const {
  return (shader_ && shader_->has_discardable_images()) ||
         (image_filter_ && image_filter_->has_discardable_images());
}

}  // namespace cc
