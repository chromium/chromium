// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_FILTER_OPERATION_H_
#define CC_PAINT_FILTER_OPERATION_H_

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_filter.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace cc {

class CC_PAINT_EXPORT FilterOperation {
 public:
  // 4x5 color matrix equivalent to SkColorMatrix.
  using Matrix = std::array<float, 20>;
  using ShapeRects = std::vector<gfx::Rect>;
  enum FilterType {
    GRAYSCALE,
    SEPIA,
    SATURATE,
    HUE_ROTATE,
    INVERT,
    BRIGHTNESS,
    CONTRAST,
    OPACITY,
    BLUR,
    DROP_SHADOW,
    COLOR_MATRIX,
    ZOOM,
    REFERENCE,
    SATURATING_BRIGHTNESS,  // Not used in CSS/SVG.
    ALPHA_THRESHOLD,        // Not used in CSS/SVG.
    OFFSET,                 // Not used in CSS/SVG.
    FILTER_TYPE_LAST = OFFSET
  };

  FilterOperation();

  FilterOperation(const FilterOperation& other);

  ~FilterOperation();

  FilterType type() const { return type_; }

  float amount() const {
    DCHECK_NE(type_, ALPHA_THRESHOLD);
    DCHECK_NE(type_, COLOR_MATRIX);
    DCHECK_NE(type_, REFERENCE);
    return amount_;
  }

  gfx::Point offset() const {
    DCHECK(type_ == DROP_SHADOW || type_ == OFFSET);
    return offset_;
  }

  SkColor4f drop_shadow_color() const {
    DCHECK_EQ(type_, DROP_SHADOW);
    return drop_shadow_color_;
  }

  const sk_sp<PaintFilter>& image_filter() const {
    DCHECK_EQ(type_, REFERENCE);
    return image_filter_;
  }

  const Matrix& matrix() const {
    DCHECK_EQ(type_, COLOR_MATRIX);
    return matrix_;
  }

  int zoom_inset() const {
    DCHECK_EQ(type_, ZOOM);
    return zoom_inset_;
  }

  const ShapeRects& shape() const {
    DCHECK_EQ(type_, ALPHA_THRESHOLD);
    return shape_;
  }

  SkTileMode blur_tile_mode() const {
    DCHECK_EQ(type_, BLUR);
    return blur_tile_mode_;
  }

  static FilterOperation CreateGrayscaleFilter(float amount) {
    return FilterOperation(GRAYSCALE, amount);
  }

  static FilterOperation CreateSepiaFilter(float amount) {
    return FilterOperation(SEPIA, amount);
  }

  static FilterOperation CreateSaturateFilter(float amount) {
    return FilterOperation(SATURATE, amount);
  }

  static FilterOperation CreateHueRotateFilter(float amount) {
    return FilterOperation(HUE_ROTATE, amount);
  }

  static FilterOperation CreateInvertFilter(float amount) {
    return FilterOperation(INVERT, amount);
  }

  static FilterOperation CreateBrightnessFilter(float amount) {
    return FilterOperation(BRIGHTNESS, amount);
  }

  static FilterOperation CreateContrastFilter(float amount) {
    return FilterOperation(CONTRAST, amount);
  }

  static FilterOperation CreateOpacityFilter(float amount) {
    return FilterOperation(OPACITY, amount);
  }

  static FilterOperation CreateBlurFilter(
      float amount,
      SkTileMode tile_mode = SkTileMode::kDecal) {
    return FilterOperation(BLUR, amount, tile_mode);
  }

  static FilterOperation CreateDropShadowFilter(const gfx::Point& offset,
                                                float std_deviation,
                                                SkColor4f color) {
    return FilterOperation(DROP_SHADOW, offset, std_deviation, color);
  }

  static FilterOperation CreateColorMatrixFilter(const Matrix& matrix) {
    return FilterOperation(COLOR_MATRIX, matrix);
  }

  static FilterOperation CreateZoomFilter(float amount, int inset) {
    return FilterOperation(ZOOM, amount, inset);
  }

  static FilterOperation CreateReferenceFilter(
      sk_sp<PaintFilter> image_filter) {
    return FilterOperation(REFERENCE, std::move(image_filter));
  }

  static FilterOperation CreateSaturatingBrightnessFilter(float amount) {
    return FilterOperation(SATURATING_BRIGHTNESS, amount);
  }

  static FilterOperation CreateAlphaThresholdFilter(const ShapeRects& shape) {
    return FilterOperation(ALPHA_THRESHOLD, shape);
  }

  static FilterOperation CreateOffsetFilter(const gfx::Point& offset) {
    return FilterOperation(OFFSET, offset);
  }

  bool operator==(const FilterOperation& other) const;

  bool operator!=(const FilterOperation& other) const {
    return !(*this == other);
  }

  // Methods for restoring a FilterOperation.
  static FilterOperation CreateEmptyFilter() {
    return FilterOperation(GRAYSCALE, 0.f);
  }

  void set_type(FilterType type) { type_ = type; }

  void set_amount(float amount) {
    DCHECK_NE(type_, ALPHA_THRESHOLD);
    DCHECK_NE(type_, COLOR_MATRIX);
    DCHECK_NE(type_, REFERENCE);
    amount_ = amount;
  }

  void set_offset(const gfx::Point& offset) {
    DCHECK(type_ == DROP_SHADOW || type_ == OFFSET);
    offset_ = offset;
  }

  void set_drop_shadow_color(SkColor4f color) {
    DCHECK_EQ(type_, DROP_SHADOW);
    drop_shadow_color_ = color;
  }

  void set_image_filter(sk_sp<PaintFilter> image_filter) {
    DCHECK_EQ(type_, REFERENCE);
    image_filter_ = std::move(image_filter);
  }

  void set_matrix(base::span<const SkScalar, 20> matrix) {
    DCHECK_EQ(type_, COLOR_MATRIX);
    for (unsigned i = 0; i < 20; ++i)
      matrix_[i] = matrix[i];
  }

  void set_zoom_inset(int inset) {
    DCHECK_EQ(type_, ZOOM);
    zoom_inset_ = inset;
  }

  void set_shape(const ShapeRects& shape) {
    DCHECK_EQ(type_, ALPHA_THRESHOLD);
    shape_ = shape;
  }

  void set_blur_tile_mode(SkTileMode tile_mode) {
    DCHECK_EQ(type_, BLUR);
    blur_tile_mode_ = tile_mode;
  }

  // Given two filters of the same type, returns a filter operation created by
  // linearly interpolating a |progress| fraction from |from| to |to|. If either
  // |from| or |to| (but not both) is null, it is treated as a no-op filter of
  // the same type as the other given filter. If both |from| and |to| are null,
  // or if |from| and |to| are non-null but of different types, returns a
  // no-op filter.
  static FilterOperation Blend(const FilterOperation* from,
                               const FilterOperation* to,
                               double progress);

  void AsValueInto(base::trace_event::TracedValue* value) const;

  // Maps "forward" to determine which pixels in a destination rect are affected
  // by pixels in the source rect. See PaintFilter::MapRect() about `ctm`.
  gfx::Rect MapRect(const gfx::Rect& rect,
                    const std::optional<SkMatrix>& ctm = std::nullopt) const;

  // Maps "backward" to determine which pixels in the source affect the pixels
  // in the destination rect. See PaintFilter::MapRect() about `ctm`.
  gfx::Rect MapRectReverse(const gfx::Rect& rect, const SkMatrix& ctm) const;

 private:
  FilterOperation(FilterType type, float amount);

  FilterOperation(FilterType type, float amount, SkTileMode tile_mode);

  FilterOperation(FilterType type,
                  const gfx::Point& offset,
                  float stdDeviation,
                  SkColor4f color);

  FilterOperation(FilterType, const Matrix& matrix);

  FilterOperation(FilterType type, float amount, int inset);

  FilterOperation(FilterType type, const gfx::Point& offset);

  FilterOperation(FilterType type, sk_sp<PaintFilter> image_filter);

  FilterOperation(FilterType type, const ShapeRects& shape);

  FilterType type_;
  float amount_;
  gfx::Point offset_;
  SkColor4f drop_shadow_color_;
  sk_sp<PaintFilter> image_filter_;
  Matrix matrix_;
  int zoom_inset_;

  // Use a collection of |gfx::Rect| to make serialization simpler.
  ShapeRects shape_;
  SkTileMode blur_tile_mode_;
};

}  // namespace cc

#endif  // CC_PAINT_FILTER_OPERATION_H_
