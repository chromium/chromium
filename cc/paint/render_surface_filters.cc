// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <algorithm>

#include "cc/paint/render_surface_filters.h"

#include "base/numerics/math_constants.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_filter.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/effects/SkAlphaThresholdFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkComposeImageFilter.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkMagnifierImageFilter.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

void GetBrightnessMatrix(float amount, float matrix[20]) {
  // Spec implementation
  // (http://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#brightnessEquivalent)
  // <feFunc[R|G|B] type="linear" slope="[amount]">
  memset(matrix, 0, 20 * sizeof(float));
  matrix[0] = matrix[6] = matrix[12] = amount;
  matrix[18] = 1.f;
}

void GetSaturatingBrightnessMatrix(float amount, float matrix[20]) {
  // Legacy implementation used by internal clients.
  // <feFunc[R|G|B] type="linear" intercept="[amount]"/>
  memset(matrix, 0, 20 * sizeof(float));
  matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1.f;
  matrix[4] = matrix[9] = matrix[14] = amount;
}

void GetContrastMatrix(float amount, float matrix[20]) {
  memset(matrix, 0, 20 * sizeof(float));
  matrix[0] = matrix[6] = matrix[12] = amount;
  matrix[4] = matrix[9] = matrix[14] = (-0.5f * amount + 0.5f);
  matrix[18] = 1.f;
}

void GetSaturateMatrix(float amount, float matrix[20]) {
  // Note, these values are computed to ensure MatrixNeedsClamping is false
  // for amount in [0..1]
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0.f;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0.f;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0.f;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0.f;
  matrix[18] = 1.f;
}

void GetHueRotateMatrix(float hue, float matrix[20]) {
  float cos_hue = cosf(hue * base::kPiFloat / 180.f);
  float sin_hue = sinf(hue * base::kPiFloat / 180.f);
  matrix[0] = 0.213f + cos_hue * 0.787f - sin_hue * 0.213f;
  matrix[1] = 0.715f - cos_hue * 0.715f - sin_hue * 0.715f;
  matrix[2] = 0.072f - cos_hue * 0.072f + sin_hue * 0.928f;
  matrix[3] = matrix[4] = 0.f;
  matrix[5] = 0.213f - cos_hue * 0.213f + sin_hue * 0.143f;
  matrix[6] = 0.715f + cos_hue * 0.285f + sin_hue * 0.140f;
  matrix[7] = 0.072f - cos_hue * 0.072f - sin_hue * 0.283f;
  matrix[8] = matrix[9] = 0.f;
  matrix[10] = 0.213f - cos_hue * 0.213f - sin_hue * 0.787f;
  matrix[11] = 0.715f - cos_hue * 0.715f + sin_hue * 0.715f;
  matrix[12] = 0.072f + cos_hue * 0.928f + sin_hue * 0.072f;
  matrix[13] = matrix[14] = 0.f;
  matrix[15] = matrix[16] = matrix[17] = 0.f;
  matrix[18] = 1.f;
  matrix[19] = 0.f;
}

void GetInvertMatrix(float amount, float matrix[20]) {
  memset(matrix, 0, 20 * sizeof(float));
  matrix[0] = matrix[6] = matrix[12] = 1.f - 2.f * amount;
  matrix[4] = matrix[9] = matrix[14] = amount;
  matrix[18] = 1.f;
}

void GetOpacityMatrix(float amount, float matrix[20]) {
  memset(matrix, 0, 20 * sizeof(float));
  matrix[0] = matrix[6] = matrix[12] = 1.f;
  matrix[18] = amount;
}

void GetGrayscaleMatrix(float amount, float matrix[20]) {
  // Note, these values are computed to ensure MatrixNeedsClamping is false
  // for amount in [0..1]
  matrix[0] = 0.2126f + 0.7874f * amount;
  matrix[1] = 0.7152f - 0.7152f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0.f;

  matrix[5] = 0.2126f - 0.2126f * amount;
  matrix[6] = 0.7152f + 0.2848f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0.f;

  matrix[10] = 0.2126f - 0.2126f * amount;
  matrix[11] = 0.7152f - 0.7152f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0.f;

  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0.f;
  matrix[18] = 1.f;
}

void GetSepiaMatrix(float amount, float matrix[20]) {
  matrix[0] = 0.393f + 0.607f * amount;
  matrix[1] = 0.769f - 0.769f * amount;
  matrix[2] = 0.189f - 0.189f * amount;
  matrix[3] = matrix[4] = 0.f;

  matrix[5] = 0.349f - 0.349f * amount;
  matrix[6] = 0.686f + 0.314f * amount;
  matrix[7] = 0.168f - 0.168f * amount;
  matrix[8] = matrix[9] = 0.f;

  matrix[10] = 0.272f - 0.272f * amount;
  matrix[11] = 0.534f - 0.534f * amount;
  matrix[12] = 0.131f + 0.869f * amount;
  matrix[13] = matrix[14] = 0.f;

  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0.f;
  matrix[18] = 1.f;
}

sk_sp<PaintFilter> CreateMatrixImageFilter(const float matrix[20],
                                           sk_sp<PaintFilter> input) {
  auto color_filter = SkColorFilters::Matrix(matrix);
  if (!color_filter)
    return nullptr;

  return sk_make_sp<ColorFilterPaintFilter>(std::move(color_filter),
                                            std::move(input));
}

}  // namespace

sk_sp<PaintFilter> RenderSurfaceFilters::BuildImageFilter(
    const FilterOperations& filters,
    const gfx::SizeF& size,
    const gfx::Vector2dF& offset) {
  sk_sp<PaintFilter> image_filter;
  float matrix[20];
  for (size_t i = 0; i < filters.size(); ++i) {
    const FilterOperation& op = filters.at(i);
    switch (op.type()) {
      case FilterOperation::GRAYSCALE:
        GetGrayscaleMatrix(1.f - op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::SEPIA:
        GetSepiaMatrix(1.f - op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::SATURATE:
        GetSaturateMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::HUE_ROTATE:
        GetHueRotateMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::INVERT:
        GetInvertMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::OPACITY:
        GetOpacityMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::BRIGHTNESS:
        GetBrightnessMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::CONTRAST:
        GetContrastMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::BLUR:
        image_filter = sk_make_sp<BlurPaintFilter>(op.amount(), op.amount(),
                                                   op.blur_tile_mode(),
                                                   std::move(image_filter));
        break;
      case FilterOperation::DROP_SHADOW:
        image_filter = sk_make_sp<DropShadowPaintFilter>(
            SkIntToScalar(op.drop_shadow_offset().x()),
            SkIntToScalar(op.drop_shadow_offset().y()),
            SkIntToScalar(op.amount()), SkIntToScalar(op.amount()),
            op.drop_shadow_color(),
            SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode,
            std::move(image_filter));
        break;
      case FilterOperation::COLOR_MATRIX:
        image_filter =
            CreateMatrixImageFilter(op.matrix(), std::move(image_filter));
        break;
      case FilterOperation::ZOOM: {
        // The center point, always the midpoint of the unclipped rectangle.
        // When we go to either edge of the screen, the width/height will shrink
        // at the same rate the offset changes. Use abs on the offset since we
        // do not care about the offset direction.
        gfx::Vector2dF center =
            gfx::Vector2dF((size.width() + std::abs(offset.x())) / 2,
                           (size.height() + std::abs(offset.y())) / 2);

        // The dimensions of the source content. This shrinks as the texture
        // rectangle gets clipped.
        gfx::Vector2d src_dimensions =
            gfx::Vector2d((size.width() + std::abs(offset.x())) / op.amount(),
                          (size.height() + std::abs(offset.y())) / op.amount());

        // When the magnifier goes to the left/top border of the screen, we need
        // to adjust the x/y position of the rect. The rate the position gets
        // updated currently only works properly for a 2x magnification.
        DCHECK_EQ(op.amount(), 2.f);
        gfx::Vector2dF center_offset = gfx::Vector2dF(0, 0);
        if (offset.x() >= 0)
          center_offset.set_x(-offset.x() / op.amount());
        if (offset.y() <= 0)
          center_offset.set_y(offset.y() / op.amount());

        sk_sp<PaintFilter> zoom_filter = sk_make_sp<MagnifierPaintFilter>(
            SkRect::MakeXYWH(
                (center.x() - src_dimensions.x() / 2.f) + center_offset.x(),
                (center.y() - src_dimensions.y() / 2.f) + center_offset.y(),
                size.width() / op.amount(), size.height() / op.amount()),
            op.zoom_inset(), nullptr);
        if (image_filter) {
          // TODO(ajuma): When there's a 1-input version of
          // SkMagnifierImageFilter, use that to handle the input filter
          // instead of using an SkComposeImageFilter.
          image_filter = sk_make_sp<ComposePaintFilter>(
              std::move(zoom_filter), std::move(image_filter));
        } else {
          image_filter = std::move(zoom_filter);
        }
        break;
      }
      case FilterOperation::SATURATING_BRIGHTNESS:
        GetSaturatingBrightnessMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::REFERENCE: {
        if (!op.image_filter())
          break;

        sk_sp<SkColorFilter> cf;
        bool has_input = false;
        if (op.image_filter()->type() == PaintFilter::Type::kColorFilter &&
            !op.image_filter()->crop_rect()) {
          auto* color_paint_filter =
              static_cast<ColorFilterPaintFilter*>(op.image_filter().get());
          cf = color_paint_filter->color_filter();
          has_input = !!color_paint_filter->input();
        }

        if (cf && cf->asAColorMatrix(matrix) && !has_input) {
          image_filter =
              CreateMatrixImageFilter(matrix, std::move(image_filter));
        } else if (image_filter) {
          image_filter = sk_make_sp<ComposePaintFilter>(
              op.image_filter(), std::move(image_filter));
        } else {
          image_filter = op.image_filter();
        }
        break;
      }
      case FilterOperation::ALPHA_THRESHOLD: {
        SkRegion region;
        for (const gfx::Rect& rect : op.shape())
          region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
        sk_sp<PaintFilter> alpha_filter = sk_make_sp<AlphaThresholdPaintFilter>(
            region, op.amount(), op.outer_threshold(), nullptr);
        if (image_filter) {
          image_filter = sk_make_sp<ComposePaintFilter>(
              std::move(alpha_filter), std::move(image_filter));
        } else {
          image_filter = std::move(alpha_filter);
        }
        break;
      }
    }
  }
  return image_filter;
}

}  // namespace cc
