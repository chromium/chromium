// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_writer.h"

namespace {

static bool affects_alpha(const SkColorFilter* cf) {
  return cf && !(cf->getFlags() & SkColorFilter::kAlphaUnchanged_Flag);
}

}  // namespace

namespace cc {

PaintFlags::PaintFlags() {
  // Match SkPaint defaults.
  bitfields_uint_ = 0u;
  bitfields_.cap_type_ = SkPaint::kDefault_Cap;
  bitfields_.join_type_ = SkPaint::kDefault_Join;
  bitfields_.style_ = SkPaint::kFill_Style;
  bitfields_.filter_quality_ = SkFilterQuality::kNone_SkFilterQuality;

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
        return !affects_alpha(color_filter_.get()) && !image_filter_;
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
  return paint.getFillPath(src, dst, cull_rect, res_scale);
}

bool PaintFlags::IsSimpleOpacity() const {
  uint32_t color = getColor();
  if (SK_ColorTRANSPARENT != SkColorSetA(color, SK_AlphaTRANSPARENT))
    return false;
  if (getBlendMode() != SkBlendMode::kSrcOver)
    return false;
  if (getLooper())
    return false;
  if (getPathEffect())
    return false;
  if (HasShader())
    return false;
  if (getMaskFilter())
    return false;
  if (getColorFilter())
    return false;
  if (getImageFilter())
    return false;
  return true;
}

bool PaintFlags::SupportsFoldingAlpha() const {
  if (getBlendMode() != SkBlendMode::kSrcOver)
    return false;
  if (getColorFilter())
    return false;
  if (getImageFilter())
    return false;
  if (getLooper())
    return false;
  return true;
}

SkPaint PaintFlags::ToSkPaint() const {
  SkPaint paint;
  paint.setPathEffect(path_effect_);
  if (shader_)
    paint.setShader(shader_->GetSkShader());
  paint.setMaskFilter(mask_filter_);
  paint.setColorFilter(color_filter_);
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
  paint.setFilterQuality(getFilterQuality());
  return paint;
}

bool PaintFlags::IsValid() const {
  return PaintOp::IsValidPaintFlagsSkBlendMode(getBlendMode());
}

bool PaintFlags::operator==(const PaintFlags& other) const {
  // Can't just ToSkPaint and operator== here as SkPaint does pointer
  // comparisons on all the ref'd skia objects on the SkPaint, which
  // is not true after serialization.
  if (getColor() != other.getColor())
    return false;
  if (!PaintOp::AreEqualEvenIfNaN(getStrokeWidth(), other.getStrokeWidth()))
    return false;
  if (!PaintOp::AreEqualEvenIfNaN(getStrokeMiter(), other.getStrokeMiter()))
    return false;
  if (getBlendMode() != other.getBlendMode())
    return false;
  if (getStrokeCap() != other.getStrokeCap())
    return false;
  if (getStrokeJoin() != other.getStrokeJoin())
    return false;
  if (getStyle() != other.getStyle())
    return false;
  if (getFilterQuality() != other.getFilterQuality())
    return false;

  if (!PaintOp::AreSkFlattenablesEqual(getPathEffect().get(),
                                       other.getPathEffect().get())) {
    return false;
  }
  if (!PaintOp::AreSkFlattenablesEqual(getMaskFilter().get(),
                                       other.getMaskFilter().get())) {
    return false;
  }
  if (!PaintOp::AreSkFlattenablesEqual(getColorFilter().get(),
                                       other.getColorFilter().get())) {
    return false;
  }
  if (!PaintOp::AreSkFlattenablesEqual(getLooper().get(),
                                       other.getLooper().get())) {
    return false;
  }

  if (!getImageFilter() != !other.getImageFilter())
    return false;
  if (getImageFilter() && *getImageFilter() != *other.getImageFilter())
    return false;

  if (!getShader() != !other.getShader())
    return false;
  if (getShader() && *getShader() != *other.getShader())
    return false;
  return true;
}

bool PaintFlags::HasDiscardableImages() const {
  return (shader_ && shader_->has_discardable_images()) ||
         (image_filter_ && image_filter_->has_discardable_images());
}

size_t PaintFlags::GetSerializedSize() const {
  return sizeof(color_) + sizeof(width_) + sizeof(miter_limit_) +
         sizeof(blend_mode_) + sizeof(bitfields_uint_) +
         PaintOpWriter::GetFlattenableSize(path_effect_.get()) +
         PaintOpWriter::Alignment() +
         PaintOpWriter::GetFlattenableSize(mask_filter_.get()) +
         PaintOpWriter::Alignment() +
         PaintOpWriter::GetFlattenableSize(color_filter_.get()) +
         PaintOpWriter::Alignment() +
         PaintOpWriter::GetFlattenableSize(draw_looper_.get()) +
         PaintFilter::GetFilterSize(image_filter_.get()) +
         PaintShader::GetSerializedSize(shader_.get());
}

}  // namespace cc
