// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/filter_operation.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/notreached.h"
#include "base/trace_event/traced_value.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

bool FilterOperation::operator==(const FilterOperation& other) const {
  if (type_ != other.type_)
    return false;
  if (type_ == COLOR_MATRIX)
    return matrix_ == other.matrix_;
  if (type_ == BLUR)
    return amount_ == other.amount_ && blur_tile_mode_ == other.blur_tile_mode_;
  if (type_ == DROP_SHADOW) {
    return amount_ == other.amount_ && offset_ == other.offset_ &&
           drop_shadow_color_ == other.drop_shadow_color_;
  }
  if (type_ == REFERENCE) {
    return image_filter_.get() == other.image_filter_.get();
  }
  if (type_ == ALPHA_THRESHOLD) {
    return shape_ == other.shape_;
  }
  if (type_ == OFFSET) {
    return offset_ == other.offset_;
  }
  return amount_ == other.amount_;
}

FilterOperation::FilterOperation() : FilterOperation(GRAYSCALE, 0.f) {}

FilterOperation::FilterOperation(FilterType type, float amount)
    : type_(type),
      amount_(amount),
      offset_(0, 0),
      drop_shadow_color_(SkColors::kTransparent),
      zoom_inset_(0) {
  DCHECK_NE(type_, DROP_SHADOW);
  DCHECK_NE(type_, COLOR_MATRIX);
  DCHECK_NE(type_, REFERENCE);
  DCHECK_NE(type_, OFFSET);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(FilterType type,
                                 float amount,
                                 SkTileMode tile_mode)
    : type_(type),
      amount_(amount),
      offset_(0, 0),
      drop_shadow_color_(SkColors::kTransparent),
      zoom_inset_(0),
      blur_tile_mode_(tile_mode) {
  DCHECK_EQ(type_, BLUR);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(FilterType type,
                                 const gfx::Point& offset,
                                 float stdDeviation,
                                 SkColor4f color)
    : type_(type),
      amount_(stdDeviation),
      offset_(offset),
      drop_shadow_color_(color),
      zoom_inset_(0) {
  DCHECK_EQ(type_, DROP_SHADOW);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(FilterType type, const Matrix& matrix)
    : type_(type),
      amount_(0),
      offset_(0, 0),
      drop_shadow_color_(SkColors::kTransparent),
      matrix_(matrix),
      zoom_inset_(0) {
  DCHECK_EQ(type_, COLOR_MATRIX);
}

FilterOperation::FilterOperation(FilterType type, float amount, int inset)
    : type_(type),
      amount_(amount),
      offset_(0, 0),
      drop_shadow_color_(SkColors::kTransparent),
      zoom_inset_(inset) {
  DCHECK_EQ(type_, ZOOM);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(FilterType type, const gfx::Point& offset)
    : type_(type),
      amount_(0),
      offset_(offset),
      drop_shadow_color_(SkColors::kTransparent),
      zoom_inset_(0) {
  DCHECK_EQ(type_, OFFSET);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(FilterType type,
                                 sk_sp<PaintFilter> image_filter)
    : type_(type),
      amount_(0),
      offset_(0, 0),
      drop_shadow_color_(SkColors::kTransparent),
      image_filter_(std::move(image_filter)),
      zoom_inset_(0) {
  DCHECK_EQ(type_, REFERENCE);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(FilterType type, const ShapeRects& shape)
    : type_(type),
      amount_(0),
      offset_(0, 0),
      drop_shadow_color_(SkColors::kTransparent),
      zoom_inset_(0),
      shape_(shape) {
  DCHECK_EQ(type_, ALPHA_THRESHOLD);
  matrix_.fill(0.0f);
}

FilterOperation::FilterOperation(const FilterOperation& other) = default;
FilterOperation::~FilterOperation() = default;

static FilterOperation CreateNoOpFilter(FilterOperation::FilterType type) {
  switch (type) {
    case FilterOperation::GRAYSCALE:
      return FilterOperation::CreateGrayscaleFilter(0.f);
    case FilterOperation::SEPIA:
      return FilterOperation::CreateSepiaFilter(0.f);
    case FilterOperation::SATURATE:
      return FilterOperation::CreateSaturateFilter(1.f);
    case FilterOperation::HUE_ROTATE:
      return FilterOperation::CreateHueRotateFilter(0.f);
    case FilterOperation::INVERT:
      return FilterOperation::CreateInvertFilter(0.f);
    case FilterOperation::BRIGHTNESS:
      return FilterOperation::CreateBrightnessFilter(1.f);
    case FilterOperation::CONTRAST:
      return FilterOperation::CreateContrastFilter(1.f);
    case FilterOperation::OPACITY:
      return FilterOperation::CreateOpacityFilter(1.f);
    case FilterOperation::BLUR:
      return FilterOperation::CreateBlurFilter(0.f);
    case FilterOperation::DROP_SHADOW:
      return FilterOperation::CreateDropShadowFilter(gfx::Point(0, 0), 0.f,
                                                     SkColors::kTransparent);
    case FilterOperation::COLOR_MATRIX: {
      FilterOperation::Matrix matrix = {};
      matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1.f;
      return FilterOperation::CreateColorMatrixFilter(matrix);
    }
    case FilterOperation::ZOOM:
      return FilterOperation::CreateZoomFilter(1.f, 0);
    case FilterOperation::SATURATING_BRIGHTNESS:
      return FilterOperation::CreateSaturatingBrightnessFilter(0.f);
    case FilterOperation::REFERENCE:
      return FilterOperation::CreateReferenceFilter(nullptr);
    case FilterOperation::ALPHA_THRESHOLD:
      return FilterOperation::CreateAlphaThresholdFilter(
          FilterOperation::ShapeRects());
    case FilterOperation::OFFSET:
      return FilterOperation::CreateOffsetFilter(gfx::Point(0, 0));
  }
  NOTREACHED();
}

static float ClampAmountForFilterType(float amount,
                                      FilterOperation::FilterType type) {
  switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
      return std::clamp(amount, 0.f, 1.f);
    case FilterOperation::SATURATE:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::BLUR:
    case FilterOperation::DROP_SHADOW:
      return std::max(amount, 0.f);
    case FilterOperation::ZOOM:
      return std::max(amount, 1.f);
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::SATURATING_BRIGHTNESS:
      return amount;
    case FilterOperation::ALPHA_THRESHOLD:
    case FilterOperation::COLOR_MATRIX:
    case FilterOperation::OFFSET:
    case FilterOperation::REFERENCE:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
FilterOperation FilterOperation::Blend(const FilterOperation* from,
                                       const FilterOperation* to,
                                       double progress) {
  FilterOperation blended_filter = FilterOperation::CreateEmptyFilter();

  if (!from && !to)
    return blended_filter;

  const FilterOperation& from_op = from ? *from : CreateNoOpFilter(to->type());
  const FilterOperation& to_op = to ? *to : CreateNoOpFilter(from->type());

  if (from_op.type() != to_op.type())
    return blended_filter;

  DCHECK(to_op.type() != FilterOperation::COLOR_MATRIX);
  blended_filter.set_type(to_op.type());

  if (to_op.type() == FilterOperation::REFERENCE) {
    if (progress > 0.5)
      blended_filter.set_image_filter(to_op.image_filter());
    else
      blended_filter.set_image_filter(from_op.image_filter());
    return blended_filter;
  } else if (to_op.type() == FilterOperation::ALPHA_THRESHOLD) {
    if (progress > 0.5) {
      blended_filter.set_shape(to_op.shape());
    } else {
      blended_filter.set_shape(from_op.shape());
    }
    return blended_filter;
  }

  blended_filter.set_amount(ClampAmountForFilterType(
      gfx::Tween::FloatValueBetween(progress, from_op.amount(), to_op.amount()),
      to_op.type()));

  if (to_op.type() == FilterOperation::BLUR) {
    blended_filter.set_blur_tile_mode(to_op.blur_tile_mode());
  } else if (to_op.type() == FilterOperation::DROP_SHADOW) {
    gfx::Point blended_offset(
        gfx::Tween::LinearIntValueBetween(progress, from_op.offset().x(),
                                          to_op.offset().x()),
        gfx::Tween::LinearIntValueBetween(progress, from_op.offset().y(),
                                          to_op.offset().y()));
    blended_filter.set_offset(blended_offset);
    blended_filter.set_drop_shadow_color(gfx::Tween::ColorValueBetween(
        progress, from_op.drop_shadow_color(), to_op.drop_shadow_color()));
  } else if (to_op.type() == FilterOperation::ZOOM) {
    blended_filter.set_zoom_inset(
        std::max(gfx::Tween::LinearIntValueBetween(
                     progress, from_op.zoom_inset(), to_op.zoom_inset()),
                 0));
  }

  return blended_filter;
}

void FilterOperation::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("type", type_);
  switch (type_) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
    case FilterOperation::BLUR:
    case FilterOperation::SATURATING_BRIGHTNESS:
      value->SetDouble("amount", amount_);
      break;
    case FilterOperation::DROP_SHADOW:
      value->SetDouble("std_deviation", amount_);
      MathUtil::AddToTracedValue("offset", offset_, value);
      // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
      value->SetInteger("color", drop_shadow_color_.toSkColor());
      break;
    case FilterOperation::COLOR_MATRIX: {
      value->BeginArray("matrix");
      for (size_t i = 0; i < std::size(matrix_); ++i)
        value->AppendDouble(matrix_[i]);
      value->EndArray();
      break;
    }
    case FilterOperation::ZOOM:
      value->SetDouble("amount", amount_);
      value->SetDouble("inset", zoom_inset_);
      break;
    case FilterOperation::REFERENCE: {
      value->SetBoolean("is_null", !image_filter_);
      if (image_filter_) {
        value->SetString("filter_type",
                         PaintFilter::TypeToString(image_filter_->type()));
      }
      break;
    }
    case FilterOperation::ALPHA_THRESHOLD: {
      value->BeginArray("shape");
      for (const gfx::Rect& rect : shape_) {
        value->AppendInteger(rect.x());
        value->AppendInteger(rect.y());
        value->AppendInteger(rect.width());
        value->AppendInteger(rect.height());
      }
      value->EndArray();
    } break;
    case FilterOperation::OFFSET:
      MathUtil::AddToTracedValue("offset", offset_, value);
      break;
  }
}

namespace {

SkVector MapStdDeviation(float std_deviation, const SkMatrix* ctm) {
  // Corresponds to SpreadForStdDeviation in filter_operations.cc.
  SkVector sigma = SkVector::Make(std_deviation, std_deviation);
  if (ctm) {
    ctm->mapVectors(&sigma, 1);
  }
  return sigma * SkIntToScalar(3);
}

gfx::Rect MapRectInternal(const FilterOperation& op,
                          const gfx::Rect& rect,
                          const SkMatrix* ctm,
                          SkImageFilter::MapDirection direction) {
  switch (op.type()) {
    case FilterOperation::BLUR: {
      SkVector spread = MapStdDeviation(op.amount(), ctm);
      float spread_x = std::abs(spread.x());
      float spread_y = std::abs(spread.y());

      // Mapping a blur both forward/backward requires an outset. For the
      // forward case this is the bounds that will be modified by the filter
      // which is larger than `rect`. For the reverse case this is the pixels
      // needed as input for the filter which is also larger than `rect`. See
      // https://crbug.com/1385154.
      gfx::RectF result(rect);
      result.Outset(gfx::OutsetsF::VH(spread_x, spread_y));
      return gfx::ToEnclosingRect(result);
    }
    case FilterOperation::DROP_SHADOW: {
      SkVector spread = MapStdDeviation(op.amount(), ctm);
      float spread_x = std::abs(spread.x());
      float spread_y = std::abs(spread.y());
      gfx::RectF result(rect);
      result.Inset(gfx::InsetsF::VH(-spread_y, -spread_x));

      SkVector mapped_drop_shadow_offset =
          SkVector::Make(op.offset().x(), op.offset().y());
      if (ctm) {
        ctm->mapVectors(&mapped_drop_shadow_offset, 1);
      }
      if (direction == SkImageFilter::kReverse_MapDirection)
        mapped_drop_shadow_offset = -mapped_drop_shadow_offset;
      result += gfx::Vector2dF(mapped_drop_shadow_offset.x(),
                               mapped_drop_shadow_offset.y());
      result.Union(gfx::RectF(rect));
      return gfx::ToEnclosingRect(result);
    }
    case FilterOperation::REFERENCE: {
      if (!op.image_filter())
        return rect;
      return gfx::SkIRectToRect(
          op.image_filter()->MapRect(gfx::RectToSkIRect(rect), ctm, direction));
    }
    case FilterOperation::OFFSET: {
      SkVector mapped_offset = SkVector::Make(op.offset().x(), op.offset().y());
      if (ctm) {
        ctm->mapVectors(&mapped_offset, 1);
      }
      if (direction == SkImageFilter::kReverse_MapDirection)
        mapped_offset = -mapped_offset;
      return gfx::ToEnclosingRect(
          gfx::RectF(rect) +
          gfx::Vector2dF(mapped_offset.x(), mapped_offset.y()));
    }
    default:
      return rect;
  }
}

}  // namespace

gfx::Rect FilterOperation::MapRect(const gfx::Rect& rect,
                                   const std::optional<SkMatrix>& ctm) const {
  return MapRectInternal(*this, rect, base::OptionalToPtr(ctm),
                         SkImageFilter::kForward_MapDirection);
}

gfx::Rect FilterOperation::MapRectReverse(const gfx::Rect& rect,
                                          const SkMatrix& ctm) const {
  return MapRectInternal(*this, rect, &ctm,
                         SkImageFilter::kReverse_MapDirection);
}

}  // namespace cc
