// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <limits>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
namespace {

TEST(FilterOperationsTest, MapRectBlur) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateBlurFilter(20));
  EXPECT_EQ(gfx::Rect(-60, -60, 130, 130),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-120, -120, 260, 260),
            ops.MapRect(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(-60, -70, 130, 130),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectBlurOverflow) {
  // Passes if float-cast-overflow does not occur in ubsan builds.
  // The blur spread exceeds INT_MAX.
  FilterOperations ops;
  ops.Append(FilterOperation::CreateBlurFilter(2e9f));
  ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I());
}

TEST(FilterOperationsTest, MapRectReverseBlur) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateBlurFilter(20));
  EXPECT_EQ(gfx::Rect(-60, -60, 130, 130),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-120, -120, 260, 260),
            ops.MapRectReverse(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(-60, -70, 130, 130),
      ops.MapRectReverse(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectLargeBlurReferenceFilter) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<BlurPaintFilter>(10000, 10000, SkTileMode::kDecal, nullptr)));
  gfx::Rect input(20000, 20000);
  gfx::Rect result_unspecified_space = ops.MapRect(input);
  // In unspecified space, the spread is always 3 * std_deviation.
  EXPECT_EQ(gfx::Rect(-30000, -30000, 80000, 80000), result_unspecified_space);
  // In device space, large blur is clamped.
  gfx::Rect result_device_space = ops.MapRect(input, SkMatrix::I());
  EXPECT_NE(result_unspecified_space, result_device_space);
  EXPECT_TRUE(result_unspecified_space.Contains(result_device_space));
}

TEST(FilterOperationsTest, MapRectDropShadowReferenceFilter) {
  FilterOperations ops;
  ops.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<DropShadowPaintFilter>(
          SkIntToScalar(3), SkIntToScalar(8), SkIntToScalar(4),
          SkIntToScalar(9), SkColors::kBlack,
          DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground,
          nullptr)));
  EXPECT_EQ(gfx::Rect(-9, -19, 34, 64),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-18, -38, 68, 128),
            ops.MapRect(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(-9, -45, 34, 64),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectReverseDropShadowReferenceFilter) {
  FilterOperations ops;
  ops.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<DropShadowPaintFilter>(
          SkIntToScalar(3), SkIntToScalar(8), SkIntToScalar(4),
          SkIntToScalar(9), SkColors::kBlack,
          DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground,
          nullptr)));

  // DropShadow includes a 1px buffer for bilinear filtering.
  EXPECT_EQ(gfx::Rect(-16, -36, 36, 66),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-31, -71, 70, 130),
            ops.MapRectReverse(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(-16, -30, 36, 66),
      ops.MapRectReverse(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectOffsetReferenceFilter) {
  sk_sp<PaintFilter> filter = sk_make_sp<OffsetPaintFilter>(30, 40, nullptr);
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(std::move(filter)));
  EXPECT_EQ(gfx::Rect(30, 40, 10, 10),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(60, 80, 20, 20),
            ops.MapRect(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(30, -50, 10, 10),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectReverseOffsetReferenceFilter) {
  sk_sp<PaintFilter> filter = sk_make_sp<OffsetPaintFilter>(30, 40, nullptr);
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(std::move(filter)));
  EXPECT_EQ(gfx::Rect(-30, -40, 10, 10),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-60, -80, 20, 20),
            ops.MapRectReverse(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(-30, 30, 10, 10),
      ops.MapRectReverse(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectCombineNonCommutative) {
  // Offsets by 100px in each axis, then scales the resulting image by 2.
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<OffsetPaintFilter>(100, 100, nullptr)));
  SkMatrix scaleMatrix;
  scaleMatrix.setScale(2, 2);
  ops.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<MatrixPaintFilter>(
          scaleMatrix, PaintFlags::FilterQuality::kNone, nullptr)));

  EXPECT_EQ(gfx::Rect(200, 200, 20, 20),
            ops.MapRect(gfx::Rect(10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(400, 400, 40, 40),
            ops.MapRect(gfx::Rect(20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(200, -220, 20, 20),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectReverseCombineNonCommutative) {
  // Offsets by 100px in each axis, then scales the resulting image by 2.
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<OffsetPaintFilter>(100, 100, nullptr)));
  SkMatrix scaleMatrix;
  scaleMatrix.setScale(2, 2);
  ops.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<MatrixPaintFilter>(
          scaleMatrix, PaintFlags::FilterQuality::kNone, nullptr)));

  EXPECT_EQ(gfx::Rect(10, 10),
            ops.MapRectReverse(gfx::Rect(200, 200, 20, 20), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(20, 20), ops.MapRectReverse(gfx::Rect(400, 400, 40, 40),
                                                  SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(0, -10, 10, 10),
      ops.MapRectReverse(gfx::Rect(200, -220, 20, 20), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectNullReferenceFilter) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(nullptr));
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            ops.MapRect(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(0, -10, 10, 10),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectReverseNullReferenceFilter) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateReferenceFilter(nullptr));
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            ops.MapRectReverse(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(0, -10, 10, 10),
      ops.MapRectReverse(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectDropShadow) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateDropShadowFilter(gfx::Point(3, 8), 20,
                                                     SkColors::kTransparent));
  EXPECT_EQ(gfx::Rect(-57, -52, 130, 130),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-114, -104, 260, 260),
            ops.MapRect(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(-57, -78, 130, 130),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectReverseDropShadow) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateDropShadowFilter(gfx::Point(3, 8), 20,
                                                     SkColors::kTransparent));
  EXPECT_EQ(gfx::Rect(-63, -68, 130, 130),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-126, -136, 260, 260),
            ops.MapRectReverse(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(-63, -62, 130, 130),
      ops.MapRectReverse(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectDropShadowDoesNotContract) {
  // Even with a drop-shadow, the original content is still drawn. Thus the
  // content bounds are never contracted due to a drop-shadow.
  FilterOperations ops;
  ops.Append(FilterOperation::CreateDropShadowFilter(gfx::Point(3, 8), 0,
                                                     SkColors::kTransparent));
  EXPECT_EQ(gfx::Rect(0, 0, 13, 18),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
}

TEST(FilterOperationsTest, MapRectReverseDropShadowDoesNotContract) {
  // Even with a drop-shadow, the original content is still drawn. Thus the
  // content bounds are never contracted due to a drop-shadow.
  FilterOperations ops;
  ops.Append(FilterOperation::CreateDropShadowFilter(gfx::Point(3, 8), 0,
                                                     SkColors::kTransparent));
  EXPECT_EQ(gfx::Rect(-3, -8, 13, 18),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
}

TEST(FilterOperationsTest, MapRectOffset) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateOffsetFilter(gfx::Point(30, 40)));
  EXPECT_EQ(gfx::Rect(30, 40, 10, 10),
            ops.MapRect(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(60, 80, 20, 20),
            ops.MapRect(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(gfx::Rect(30, -50, 10, 10),
            ops.MapRect(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectReverseOffset) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateOffsetFilter(gfx::Point(30, 40)));
  EXPECT_EQ(gfx::Rect(-30, -40, 10, 10),
            ops.MapRectReverse(gfx::Rect(0, 0, 10, 10), SkMatrix::I()));
  EXPECT_EQ(gfx::Rect(-60, -80, 20, 20),
            ops.MapRectReverse(gfx::Rect(0, 0, 20, 20), SkMatrix::Scale(2, 2)));
  EXPECT_EQ(
      gfx::Rect(-30, 30, 10, 10),
      ops.MapRectReverse(gfx::Rect(0, -10, 10, 10), SkMatrix::Scale(1, -1)));
}

TEST(FilterOperationsTest, MapRectTypeConversionDoesNotOverflow) {
  // Must be bigger than half of the positive range so that the width/height
  // overflow happens, but small enough that there aren't other issues before
  // the overflow would happen.
  SkScalar big_offset =
      SkFloatToScalar(std::numeric_limits<int>::max()) * 2 / 3;

  FilterOperations ops;
  ops.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<XfermodePaintFilter>(
          SkBlendMode::kSrcOver,
          sk_make_sp<OffsetPaintFilter>(-big_offset, -big_offset, nullptr),
          sk_make_sp<OffsetPaintFilter>(big_offset, big_offset, nullptr))));
  gfx::Rect rect = ops.MapRect(gfx::Rect(-10, -10, 20, 20), SkMatrix::I());
  EXPECT_GT(rect.width(), 0);
  EXPECT_GT(rect.height(), 0);
}

#define SAVE_RESTORE_AMOUNT(filter_name, filter_type, a)                  \
  {                                                                       \
    FilterOperation op = FilterOperation::Create##filter_name##Filter(a); \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                   \
    EXPECT_EQ(a, op.amount());                                            \
                                                                          \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();           \
    op2.set_type(FilterOperation::filter_type);                           \
                                                                          \
    EXPECT_NE(a, op2.amount());                                           \
                                                                          \
    op2.set_amount(a);                                                    \
                                                                          \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                  \
    EXPECT_EQ(a, op2.amount());                                           \
  }

#define SAVE_RESTORE_OFFSET_AMOUNT_COLOR(filter_name, filter_type, a, b, c) \
  {                                                                         \
    FilterOperation op =                                                    \
        FilterOperation::Create##filter_name##Filter(a, b, c);              \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                     \
    EXPECT_EQ(a, op.offset());                                              \
    EXPECT_EQ(b, op.amount());                                              \
    EXPECT_EQ(c, op.drop_shadow_color());                                   \
                                                                            \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();             \
    op2.set_type(FilterOperation::filter_type);                             \
                                                                            \
    EXPECT_NE(a, op2.offset());                                             \
    EXPECT_NE(b, op2.amount());                                             \
    EXPECT_NE(c, op2.drop_shadow_color());                                  \
                                                                            \
    op2.set_offset(a);                                                      \
    op2.set_amount(b);                                                      \
    op2.set_drop_shadow_color(c);                                           \
                                                                            \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                    \
    EXPECT_EQ(a, op2.offset());                                             \
    EXPECT_EQ(b, op2.amount());                                             \
    EXPECT_EQ(c, op2.drop_shadow_color());                                  \
  }

#define SAVE_RESTORE_MATRIX(filter_name, filter_type, a)                  \
  {                                                                       \
    FilterOperation op = FilterOperation::Create##filter_name##Filter(a); \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                   \
    for (size_t i = 0; i < 20; ++i)                                       \
      EXPECT_EQ(a[i], op.matrix()[i]);                                    \
                                                                          \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();           \
    op2.set_type(FilterOperation::filter_type);                           \
                                                                          \
    for (size_t i = 0; i < 20; ++i)                                       \
      EXPECT_NE(a[i], op2.matrix()[i]);                                   \
                                                                          \
    op2.set_matrix(a);                                                    \
                                                                          \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                  \
    for (size_t i = 0; i < 20; ++i)                                       \
      EXPECT_EQ(a[i], op.matrix()[i]);                                    \
  }

#define SAVE_RESTORE_AMOUNT_INSET(filter_name, filter_type, a, b)            \
  {                                                                          \
    FilterOperation op = FilterOperation::Create##filter_name##Filter(a, b); \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                      \
    EXPECT_EQ(a, op.amount());                                               \
    EXPECT_EQ(b, op.zoom_inset());                                           \
                                                                             \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();              \
    op2.set_type(FilterOperation::filter_type);                              \
                                                                             \
    EXPECT_NE(a, op2.amount());                                              \
    EXPECT_NE(b, op2.zoom_inset());                                          \
                                                                             \
    op2.set_amount(a);                                                       \
    op2.set_zoom_inset(b);                                                   \
                                                                             \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                     \
    EXPECT_EQ(a, op2.amount());                                              \
    EXPECT_EQ(b, op2.zoom_inset());                                          \
  }

TEST(FilterOperationsTest, SaveAndRestore) {
  SAVE_RESTORE_AMOUNT(Grayscale, GRAYSCALE, 0.6f);
  SAVE_RESTORE_AMOUNT(Sepia, SEPIA, 0.6f);
  SAVE_RESTORE_AMOUNT(Saturate, SATURATE, 0.6f);
  SAVE_RESTORE_AMOUNT(HueRotate, HUE_ROTATE, 0.6f);
  SAVE_RESTORE_AMOUNT(Invert, INVERT, 0.6f);
  SAVE_RESTORE_AMOUNT(Brightness, BRIGHTNESS, 0.6f);
  SAVE_RESTORE_AMOUNT(Contrast, CONTRAST, 0.6f);
  SAVE_RESTORE_AMOUNT(Opacity, OPACITY, 0.6f);
  SAVE_RESTORE_AMOUNT(Blur, BLUR, 0.6f);
  SAVE_RESTORE_AMOUNT(SaturatingBrightness, SATURATING_BRIGHTNESS, 0.6f);
  SAVE_RESTORE_OFFSET_AMOUNT_COLOR(DropShadow, DROP_SHADOW, gfx::Point(3, 4),
                                   0.4f, SkColors::kYellow);

  FilterOperation::Matrix matrix = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                    11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  SAVE_RESTORE_MATRIX(ColorMatrix, COLOR_MATRIX, matrix);

  SAVE_RESTORE_AMOUNT_INSET(Zoom, ZOOM, 0.5f, 32);
}

TEST(FilterOperationsTest, BlendGrayscaleFilters) {
  FilterOperation from = FilterOperation::CreateGrayscaleFilter(0.25f);
  FilterOperation to = FilterOperation::CreateGrayscaleFilter(0.75f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateGrayscaleFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateGrayscaleFilter(0.625f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.8);
  expected = FilterOperation::CreateGrayscaleFilter(1.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendGrayscaleWithNull) {
  FilterOperation filter = FilterOperation::CreateGrayscaleFilter(1.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateGrayscaleFilter(0.75f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateGrayscaleFilter(0.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendSepiaFilters) {
  FilterOperation from = FilterOperation::CreateSepiaFilter(0.25f);
  FilterOperation to = FilterOperation::CreateSepiaFilter(0.75f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateSepiaFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateSepiaFilter(0.625f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.8);
  expected = FilterOperation::CreateSepiaFilter(1.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendSepiaWithNull) {
  FilterOperation filter = FilterOperation::CreateSepiaFilter(1.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateSepiaFilter(0.75f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateSepiaFilter(0.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendSaturateFilters) {
  FilterOperation from = FilterOperation::CreateSaturateFilter(0.25f);
  FilterOperation to = FilterOperation::CreateSaturateFilter(0.75f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateSaturateFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateSaturateFilter(0.625f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 2.0);
  expected = FilterOperation::CreateSaturateFilter(1.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendSaturateWithNull) {
  FilterOperation filter = FilterOperation::CreateSaturateFilter(0.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateSaturateFilter(0.25f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateSaturateFilter(0.75f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendHueRotateFilters) {
  FilterOperation from = FilterOperation::CreateHueRotateFilter(3.f);
  FilterOperation to = FilterOperation::CreateHueRotateFilter(7.f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateHueRotateFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateHueRotateFilter(6.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateHueRotateFilter(9.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendHueRotateWithNull) {
  FilterOperation filter = FilterOperation::CreateHueRotateFilter(1.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateHueRotateFilter(0.75f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateHueRotateFilter(0.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendInvertFilters) {
  FilterOperation from = FilterOperation::CreateInvertFilter(0.25f);
  FilterOperation to = FilterOperation::CreateInvertFilter(0.75f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateInvertFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateInvertFilter(0.625f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.8);
  expected = FilterOperation::CreateInvertFilter(1.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendInvertWithNull) {
  FilterOperation filter = FilterOperation::CreateInvertFilter(1.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateInvertFilter(0.75f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateInvertFilter(0.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendBrightnessFilters) {
  FilterOperation from = FilterOperation::CreateBrightnessFilter(3.f);
  FilterOperation to = FilterOperation::CreateBrightnessFilter(7.f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.9);
  FilterOperation expected = FilterOperation::CreateBrightnessFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateBrightnessFilter(6.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateBrightnessFilter(9.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendBrightnessWithNull) {
  FilterOperation filter = FilterOperation::CreateBrightnessFilter(0.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateBrightnessFilter(0.25f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateBrightnessFilter(0.75f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendContrastFilters) {
  FilterOperation from = FilterOperation::CreateContrastFilter(3.f);
  FilterOperation to = FilterOperation::CreateContrastFilter(7.f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.9);
  FilterOperation expected = FilterOperation::CreateContrastFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateContrastFilter(6.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateContrastFilter(9.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendContrastWithNull) {
  FilterOperation filter = FilterOperation::CreateContrastFilter(0.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateContrastFilter(0.25f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateContrastFilter(0.75f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendOpacityFilters) {
  FilterOperation from = FilterOperation::CreateOpacityFilter(0.25f);
  FilterOperation to = FilterOperation::CreateOpacityFilter(0.75f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateOpacityFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateOpacityFilter(0.625f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.8);
  expected = FilterOperation::CreateOpacityFilter(1.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendOpacityWithNull) {
  FilterOperation filter = FilterOperation::CreateOpacityFilter(0.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateOpacityFilter(0.25f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateOpacityFilter(0.75f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendBlurFilters) {
  FilterOperation from = FilterOperation::CreateBlurFilter(3.f);
  FilterOperation to = FilterOperation::CreateBlurFilter(7.f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.9);
  FilterOperation expected = FilterOperation::CreateBlurFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateBlurFilter(6.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateBlurFilter(9.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendBlurWithNull) {
  FilterOperation filter = FilterOperation::CreateBlurFilter(1.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateBlurFilter(0.75f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateBlurFilter(0.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendDropShadowFilters) {
  FilterOperation from = FilterOperation::CreateDropShadowFilter(
      gfx::Point(0, 0), 2.f, SkColor4f{0.13f, 0.27f, 0.53f, 0.06f});
  FilterOperation to = FilterOperation::CreateDropShadowFilter(
      gfx::Point(3, 5), 6.f, SkColor4f{0.12f, 0.24f, 0.47f, 0.2f});

  // In the test below we have to use EXPECT_NEAR as the color contain float for
  // the components. In order to properly test the filterOperation we are
  // equalizing the color aftewards.
  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateDropShadowFilter(
      gfx::Point(-2, -4), 0.f, SkColor4f{0.0f, 0.0f, 0.0f, 0.0f});
  SkColor4f blended_color = blended.drop_shadow_color();
  SkColor4f expected_color = expected.drop_shadow_color();
  EXPECT_NEAR(blended_color.fR, expected_color.fR, 0.0001f);
  EXPECT_NEAR(blended_color.fG, expected_color.fG, 0.0001f);
  EXPECT_NEAR(blended_color.fB, expected_color.fB, 0.0001f);
  EXPECT_NEAR(blended_color.fA, expected_color.fA, 0.0001f);
  expected.set_drop_shadow_color(blended_color);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.25);
  expected = FilterOperation::CreateDropShadowFilter(
      gfx::Point(1, 1), 3.f, SkColor4f{0.1247f, 0.2542f, 0.4984f, 0.095f});
  blended_color = blended.drop_shadow_color();
  expected_color = expected.drop_shadow_color();
  EXPECT_NEAR(blended_color.fR, expected_color.fR, 0.0001f);
  EXPECT_NEAR(blended_color.fG, expected_color.fG, 0.0001f);
  EXPECT_NEAR(blended_color.fB, expected_color.fB, 0.0001f);
  EXPECT_NEAR(blended_color.fA, expected_color.fA, 0.0001f);
  expected.set_drop_shadow_color(blended_color);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateDropShadowFilter(
      gfx::Point(2, 4), 5.f, SkColor4f{0.1209f, 0.2427f, 0.4755f, 0.1649f});
  blended_color = blended.drop_shadow_color();
  expected_color = expected.drop_shadow_color();
  EXPECT_NEAR(blended_color.fR, expected_color.fR, 0.0001f);
  EXPECT_NEAR(blended_color.fG, expected_color.fG, 0.0001f);
  EXPECT_NEAR(blended_color.fB, expected_color.fB, 0.0001f);
  EXPECT_NEAR(blended_color.fA, expected_color.fA, 0.0001f);
  expected.set_drop_shadow_color(blended_color);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateDropShadowFilter(
      gfx::Point(5, 8), 8.f, SkColor4f{0.1188f, 0.2366f, 0.4633f, 0.27});
  blended_color = blended.drop_shadow_color();
  expected_color = expected.drop_shadow_color();
  EXPECT_NEAR(blended_color.fR, expected_color.fR, 0.0001f);
  EXPECT_NEAR(blended_color.fG, expected_color.fG, 0.0001f);
  EXPECT_NEAR(blended_color.fB, expected_color.fB, 0.0001f);
  EXPECT_NEAR(blended_color.fA, expected_color.fA, 0.0001f);
  expected.set_drop_shadow_color(blended_color);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendDropShadowWithNull) {
  FilterOperation filter = FilterOperation::CreateDropShadowFilter(
      gfx::Point(4, 4), 4.f, SkColor4f{0.16f, 0.0f, 0.0f, 1.0f});

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateDropShadowFilter(
      gfx::Point(3, 3), 3.f, SkColor4f{0.16f, 0.0f, 0.0f, 0.75f});
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateDropShadowFilter(
      gfx::Point(1, 1), 1.f, SkColor4f{0.16f, 0.0f, 0.0f, 0.25f});
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendZoomFilters) {
  FilterOperation from = FilterOperation::CreateZoomFilter(2.f, 3);
  FilterOperation to = FilterOperation::CreateZoomFilter(6.f, 0);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected = FilterOperation::CreateZoomFilter(1.f, 5);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateZoomFilter(5.f, 1);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateZoomFilter(8.f, 0);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendZoomWithNull) {
  FilterOperation filter = FilterOperation::CreateZoomFilter(2.f, 1);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected = FilterOperation::CreateZoomFilter(1.75f, 1);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateZoomFilter(1.25f, 0);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendSaturatingBrightnessFilters) {
  FilterOperation from = FilterOperation::CreateSaturatingBrightnessFilter(3.f);
  FilterOperation to = FilterOperation::CreateSaturatingBrightnessFilter(7.f);

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  FilterOperation expected =
      FilterOperation::CreateSaturatingBrightnessFilter(0.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 0.75);
  expected = FilterOperation::CreateSaturatingBrightnessFilter(6.f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  expected = FilterOperation::CreateSaturatingBrightnessFilter(9.f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendSaturatingBrightnessWithNull) {
  FilterOperation filter =
      FilterOperation::CreateSaturatingBrightnessFilter(1.f);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  FilterOperation expected =
      FilterOperation::CreateSaturatingBrightnessFilter(0.75f);
  EXPECT_EQ(expected, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  expected = FilterOperation::CreateSaturatingBrightnessFilter(0.25f);
  EXPECT_EQ(expected, blended);
}

TEST(FilterOperationsTest, BlendReferenceFilters) {
  sk_sp<PaintFilter> from_filter(
      sk_make_sp<BlurPaintFilter>(1.f, 1.f, SkTileMode::kDecal, nullptr));
  sk_sp<PaintFilter> to_filter(
      sk_make_sp<BlurPaintFilter>(2.f, 2.f, SkTileMode::kDecal, nullptr));
  FilterOperation from =
      FilterOperation::CreateReferenceFilter(std::move(from_filter));
  FilterOperation to =
      FilterOperation::CreateReferenceFilter(std::move(to_filter));

  FilterOperation blended = FilterOperation::Blend(&from, &to, -0.75);
  EXPECT_EQ(from, blended);

  blended = FilterOperation::Blend(&from, &to, 0.5);
  EXPECT_EQ(from, blended);

  blended = FilterOperation::Blend(&from, &to, 0.6);
  EXPECT_EQ(to, blended);

  blended = FilterOperation::Blend(&from, &to, 1.5);
  EXPECT_EQ(to, blended);
}

TEST(FilterOperationsTest, BlendReferenceWithNull) {
  sk_sp<PaintFilter> image_filter(
      sk_make_sp<BlurPaintFilter>(1.f, 1.f, SkTileMode::kDecal, nullptr));
  FilterOperation filter =
      FilterOperation::CreateReferenceFilter(std::move(image_filter));
  FilterOperation null_filter = FilterOperation::CreateReferenceFilter(nullptr);

  FilterOperation blended = FilterOperation::Blend(&filter, nullptr, 0.25);
  EXPECT_EQ(filter, blended);
  blended = FilterOperation::Blend(&filter, nullptr, 0.75);
  EXPECT_EQ(null_filter, blended);

  blended = FilterOperation::Blend(nullptr, &filter, 0.25);
  EXPECT_EQ(null_filter, blended);
  blended = FilterOperation::Blend(nullptr, &filter, 0.75);
  EXPECT_EQ(filter, blended);
}

// Tests blending non-empty sequences that have the same length and matching
// operations.
TEST(FilterOperationsTest, BlendMatchingSequences) {
  FilterOperations from;
  FilterOperations to;

  from.Append(FilterOperation::CreateBlurFilter(0.f));
  to.Append(FilterOperation::CreateBlurFilter(2.f));

  from.Append(FilterOperation::CreateSaturateFilter(4.f));
  to.Append(FilterOperation::CreateSaturateFilter(0.f));

  from.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  to.Append(FilterOperation::CreateZoomFilter(10.f, 9));

  FilterOperations blended = to.Blend(from, -0.75);
  FilterOperations expected;
  expected.Append(FilterOperation::CreateBlurFilter(0.f));
  expected.Append(FilterOperation::CreateSaturateFilter(7.f));
  expected.Append(FilterOperation::CreateZoomFilter(1.f, 0));
  EXPECT_EQ(blended, expected);

  blended = to.Blend(from, 0.75);
  expected.Clear();
  expected.Append(FilterOperation::CreateBlurFilter(1.5f));
  expected.Append(FilterOperation::CreateSaturateFilter(1.f));
  expected.Append(FilterOperation::CreateZoomFilter(8.f, 7));
  EXPECT_EQ(blended, expected);

  blended = to.Blend(from, 1.5);
  expected.Clear();
  expected.Append(FilterOperation::CreateBlurFilter(3.f));
  expected.Append(FilterOperation::CreateSaturateFilter(0.f));
  expected.Append(FilterOperation::CreateZoomFilter(14.f, 13));
  EXPECT_EQ(blended, expected);
}

TEST(FilterOperationsTest, BlendEmptyAndNonEmptySequences) {
  FilterOperations empty;
  FilterOperations filters;

  filters.Append(FilterOperation::CreateGrayscaleFilter(0.75f));
  filters.Append(FilterOperation::CreateBrightnessFilter(2.f));
  filters.Append(FilterOperation::CreateHueRotateFilter(10.0f));

  FilterOperations blended = empty.Blend(filters, -0.75);
  FilterOperations expected;
  expected.Append(FilterOperation::CreateGrayscaleFilter(1.f));
  expected.Append(FilterOperation::CreateBrightnessFilter(2.75f));
  expected.Append(FilterOperation::CreateHueRotateFilter(17.5f));
  EXPECT_EQ(blended, expected);

  blended = empty.Blend(filters, 0.75);
  expected.Clear();
  expected.Append(FilterOperation::CreateGrayscaleFilter(0.1875f));
  expected.Append(FilterOperation::CreateBrightnessFilter(1.25f));
  expected.Append(FilterOperation::CreateHueRotateFilter(2.5f));
  EXPECT_EQ(blended, expected);

  blended = empty.Blend(filters, 1.5);
  expected.Clear();
  expected.Append(FilterOperation::CreateGrayscaleFilter(0.f));
  expected.Append(FilterOperation::CreateBrightnessFilter(0.5f));
  expected.Append(FilterOperation::CreateHueRotateFilter(-5.f));
  EXPECT_EQ(blended, expected);

  blended = filters.Blend(empty, -0.75);
  expected.Clear();
  expected.Append(FilterOperation::CreateGrayscaleFilter(0.f));
  expected.Append(FilterOperation::CreateBrightnessFilter(0.25f));
  expected.Append(FilterOperation::CreateHueRotateFilter(-7.5f));
  EXPECT_EQ(blended, expected);

  blended = filters.Blend(empty, 0.75);
  expected.Clear();
  expected.Append(FilterOperation::CreateGrayscaleFilter(0.5625f));
  expected.Append(FilterOperation::CreateBrightnessFilter(1.75f));
  expected.Append(FilterOperation::CreateHueRotateFilter(7.5f));
  EXPECT_EQ(blended, expected);

  blended = filters.Blend(empty, 1.5);
  expected.Clear();
  expected.Append(FilterOperation::CreateGrayscaleFilter(1.f));
  expected.Append(FilterOperation::CreateBrightnessFilter(2.5f));
  expected.Append(FilterOperation::CreateHueRotateFilter(15.f));
  EXPECT_EQ(blended, expected);
}

TEST(FilterOperationsTest, BlendEmptySequences) {
  FilterOperations empty;

  FilterOperations blended = empty.Blend(empty, -0.75);
  EXPECT_EQ(blended, empty);

  blended = empty.Blend(empty, 0.75);
  EXPECT_EQ(blended, empty);

  blended = empty.Blend(empty, 1.5);
  EXPECT_EQ(blended, empty);
}

// Tests blending non-empty sequences that have non-matching operations.
TEST(FilterOperationsTest, BlendNonMatchingSequences) {
  FilterOperations from;
  FilterOperations to;

  from.Append(FilterOperation::CreateSaturateFilter(3.f));
  from.Append(FilterOperation::CreateBlurFilter(2.f));
  to.Append(FilterOperation::CreateSaturateFilter(4.f));
  to.Append(FilterOperation::CreateHueRotateFilter(0.5f));

  FilterOperations blended = to.Blend(from, -0.75);
  EXPECT_EQ(to, blended);
  blended = to.Blend(from, 0.75);
  EXPECT_EQ(to, blended);
  blended = to.Blend(from, 1.5);
  EXPECT_EQ(to, blended);
}

// Tests blending non-empty sequences of different sizes.
TEST(FilterOperationsTest, BlendRaggedSequences) {
  FilterOperations from;
  FilterOperations to;

  from.Append(FilterOperation::CreateSaturateFilter(3.f));
  from.Append(FilterOperation::CreateBlurFilter(2.f));
  to.Append(FilterOperation::CreateSaturateFilter(4.f));

  FilterOperations blended = to.Blend(from, -0.75);
  FilterOperations expected;
  expected.Append(FilterOperation::CreateSaturateFilter(2.25f));
  expected.Append(FilterOperation::CreateBlurFilter(3.5f));
  EXPECT_EQ(expected, blended);

  blended = to.Blend(from, 0.75);
  expected.Clear();
  expected.Append(FilterOperation::CreateSaturateFilter(3.75f));
  expected.Append(FilterOperation::CreateBlurFilter(0.5f));
  EXPECT_EQ(expected, blended);

  blended = to.Blend(from, 1.5);
  expected.Clear();
  expected.Append(FilterOperation::CreateSaturateFilter(4.5f));
  expected.Append(FilterOperation::CreateBlurFilter(0.f));
  EXPECT_EQ(expected, blended);

  from.Append(FilterOperation::CreateOpacityFilter(1.f));
  to.Append(FilterOperation::CreateOpacityFilter(1.f));
  blended = to.Blend(from, -0.75);
  EXPECT_EQ(to, blended);
  blended = to.Blend(from, 0.75);
  EXPECT_EQ(to, blended);
  blended = to.Blend(from, 1.5);
  EXPECT_EQ(to, blended);
}

TEST(FilterOperationsTest, ToString) {
  FilterOperations filters;
  EXPECT_EQ(std::string("{\"FilterOperations\":[]}"), filters.ToString());

  filters.Append(FilterOperation::CreateSaturateFilter(3.f));
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  EXPECT_EQ(std::string("{\"FilterOperations\":[{\"type\":2,\"amount\":3.0},"
                        "{\"type\":8,\"amount\":2.0}]}"),
            filters.ToString());
}

TEST(FilterOperationsTest, HasFilterOfType) {
  FilterOperations filters;

  EXPECT_FALSE(filters.HasFilterOfType(FilterOperation::GRAYSCALE));
  EXPECT_FALSE(filters.HasFilterOfType(FilterOperation::BLUR));
  EXPECT_FALSE(filters.HasReferenceFilter());
  EXPECT_FALSE(filters.HasFilterOfType(FilterOperation::OPACITY));
  EXPECT_FALSE(filters.HasFilterOfType(FilterOperation::ZOOM));

  filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
  filters.Append(FilterOperation::CreateBlurFilter(20));
  sk_sp<PaintFilter> filter(
      sk_make_sp<BlurPaintFilter>(1.f, 1.f, SkTileMode::kDecal, nullptr));
  filters.Append(FilterOperation::CreateReferenceFilter(std::move(filter)));

  EXPECT_TRUE(filters.HasFilterOfType(FilterOperation::GRAYSCALE));
  EXPECT_TRUE(filters.HasFilterOfType(FilterOperation::BLUR));
  EXPECT_TRUE(filters.HasReferenceFilter());
  EXPECT_FALSE(filters.HasFilterOfType(FilterOperation::OPACITY));
  EXPECT_FALSE(filters.HasFilterOfType(FilterOperation::ZOOM));
}

std::string PostTestCaseName(const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "UseMapRect" : "RectExpansion";
}

class UseMapRectFilterOperationsTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  UseMapRectFilterOperationsTest();
  ~UseMapRectFilterOperationsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

UseMapRectFilterOperationsTest::UseMapRectFilterOperationsTest() {
  if (GetParam()) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kUseMapRectForPixelMovement);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        features::kUseMapRectForPixelMovement);
  }
}

TEST_P(UseMapRectFilterOperationsTest, ExpandRectForPixelMovement) {
  constexpr gfx::Rect test_rect(0, 0, 100, 100);
  FilterOperations filters;

  filters.Append(FilterOperation::CreateBlurFilter(20));
  EXPECT_EQ(gfx::Rect(-60, -60, 220, 220),
            filters.ExpandRectForPixelMovement(test_rect));

  filters.Clear();
  filters.Append(FilterOperation::CreateDropShadowFilter(
      gfx::Point(3, -8), 20, SkColors::kTransparent));
  if (GetParam()) {
    EXPECT_EQ(gfx::Rect(-57, -68, 220, 220),
              filters.ExpandRectForPixelMovement(test_rect));
  } else {
    // max_movement = max(std::abs(3), std::abs(-8)) + 20 * 3;
    EXPECT_EQ(gfx::Rect(-68, -68, 236, 236),
              filters.ExpandRectForPixelMovement(test_rect));
  }

  // The zoom filter is a pixel moving filter but it only moves pixels inside
  // the filtered rect and doesn't expand the rect.
  filters.Clear();
  filters.Append(FilterOperation::CreateZoomFilter(2, 3));
  if (GetParam()) {
    EXPECT_EQ(test_rect, filters.ExpandRectForPixelMovement(test_rect));
  } else {
    // max movement = zoom_inset = 3
    EXPECT_EQ(gfx::Rect(-3, -3, 106, 106),
              filters.ExpandRectForPixelMovement(test_rect));
  }

  filters.Clear();
  filters.Append(FilterOperation::CreateOffsetFilter(gfx::Point(3, -4)));
  if (GetParam()) {
    EXPECT_EQ(gfx::Rect(3, -4, 100, 100),
              filters.ExpandRectForPixelMovement(test_rect));
  } else {
    EXPECT_EQ(gfx::Rect(-4, -4, 108, 108),
              filters.ExpandRectForPixelMovement(test_rect));
  }

  filters.Clear();
  if (GetParam()) {
    filters.Append(FilterOperation::CreateReferenceFilter(
        sk_make_sp<OffsetPaintFilter>(10, 8, nullptr)));
    EXPECT_EQ(gfx::Rect(10, 8, 100, 100),
              filters.ExpandRectForPixelMovement(test_rect));
  } else {
    filters.Append(FilterOperation::CreateReferenceFilter(
        sk_make_sp<OffsetPaintFilter>(10, 10, nullptr)));
    // max movement = 100.
    EXPECT_EQ(gfx::Rect(-100, -100, 300, 300),
              filters.ExpandRectForPixelMovement(test_rect));
  }

  // For filters that don't move pixels. HasFilterThatMovesPixels() = false.
  filters.Clear();
  filters.Append(FilterOperation::CreateOpacityFilter(0.25f));
  FilterOperation::Matrix matrix = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                    11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  filters.Append(FilterOperation::CreateColorMatrixFilter(matrix));
  filters.Append(FilterOperation::CreateGrayscaleFilter(0.75f));
  filters.Append(FilterOperation::CreateSepiaFilter(0.625f));
  filters.Append(FilterOperation::CreateSaturateFilter(1.25f));
  filters.Append(FilterOperation::CreateHueRotateFilter(6.f));
  filters.Append(FilterOperation::CreateInvertFilter(0.75f));
  filters.Append(FilterOperation::CreateBrightnessFilter(9.f));
  filters.Append(FilterOperation::CreateContrastFilter(3.f));
  filters.Append(FilterOperation::CreateSaturatingBrightnessFilter(7.f));

  EXPECT_EQ(test_rect, filters.ExpandRectForPixelMovement(test_rect));
}

TEST_P(UseMapRectFilterOperationsTest,
       ExpandRectForPixelMovement_MultipleFilters) {
  if (!GetParam()) {
    return;
  }
  constexpr gfx::Rect test_rect(0, 0, 100, 100);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(20));
  filters.Append(FilterOperation::CreateDropShadowFilter(
      gfx::Point(5, 10), 10, SkColors::kTransparent));

  // Blur expand 60 all directions and drop shadow shifts (5, 10) and expands
  // 30 all directions.
  EXPECT_EQ(gfx::Rect(-85, -80, 280, 280),
            filters.ExpandRectForPixelMovement(test_rect));

  filters.Clear();
  filters.Append(FilterOperation::CreateOffsetFilter(gfx::Point(-20, 50)));
  filters.Append(FilterOperation::CreateBlurFilter(20));

  // Offset shifts (-20, 50) and blur expands 60 all directions.
  EXPECT_EQ(gfx::Rect(-80, -10, 220, 220),
            filters.ExpandRectForPixelMovement(test_rect));
}

INSTANTIATE_TEST_SUITE_P(,
                         UseMapRectFilterOperationsTest,
                         testing::Bool(),
                         &PostTestCaseName);

}  // namespace
}  // namespace cc
