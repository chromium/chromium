// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/md_icon_normalizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"

namespace {

constexpr int kIconSize = 192;
constexpr int kGuidelineSizeSquare = 152;
constexpr int kGuidelineSizeRound = 176;

constexpr gfx::RectF kIconFrame(0, 0, kIconSize, kIconSize);

constexpr int kHalfSize = kIconSize / 2;
constexpr gfx::PointF kCenter(kHalfSize, kHalfSize);

constexpr SkColor kFillColor = SK_ColorWHITE;
constexpr SkAlpha kMaxShadowAlpha = 40;

float GetScale(const SkBitmap& bitmap) {
  return app_list::GetMdIconScaleForTest(bitmap);
}

void ExpectScaledSize(int expected, int original, const SkBitmap& bitmap) {
  const float scaled = original * GetScale(bitmap);
  EXPECT_GT(0.5, std::abs(expected - scaled));
}

}  // namespace

class MdIconNormalizerTest : public testing::Test {
 protected:
  void SetUp() override {
    flags_opaque.setStyle(cc::PaintFlags::kFill_Style);
    flags_opaque.setColor(kFillColor);

    flags_opaque_min.setStyle(cc::PaintFlags::kFill_Style);
    flags_opaque_min.setColor(SkColorSetA(kFillColor, kMaxShadowAlpha + 1));

    flags_shadow.setStyle(cc::PaintFlags::kFill_Style);
    flags_shadow.setColor(SkColorSetA(kFillColor, kMaxShadowAlpha));

    flags_transparent.setStyle(cc::PaintFlags::kFill_Style);
    flags_transparent.setColor(SK_ColorTRANSPARENT);

    ResetCanvas();
  }

  void ResetCanvas() {
    canvas = std::make_unique<gfx::Canvas>(gfx::Size(kIconSize, kIconSize),
                                           1 /* image_scale */,
                                           false /* is_opaque */);
  }

  float GetScale() { return ::GetScale(canvas->GetBitmap()); }

  void ExpectScaledSize(int expected, int original) {
    ::ExpectScaledSize(expected, original, canvas->GetBitmap());
  }

  std::unique_ptr<gfx::Canvas> canvas;

  cc::PaintFlags flags_opaque;
  cc::PaintFlags flags_opaque_min;
  cc::PaintFlags flags_shadow;
  cc::PaintFlags flags_transparent;
};

TEST_F(MdIconNormalizerTest, SquareIcon) {
  // Full size square, should scale to the exact guideline size.
  canvas->DrawRect(kIconFrame, flags_opaque);
  ExpectScaledSize(kGuidelineSizeSquare, kIconFrame.width());

  // Add a transparent hole in the middle, should scale the same as above.
  gfx::RectF inner_frame = kIconFrame;
  constexpr int kInset = 4;
  inner_frame.Inset(kInset);
  canvas->DrawRect(inner_frame, flags_transparent);
  ExpectScaledSize(kGuidelineSizeSquare, kIconFrame.width());

  // Smaller square, but still larger than the guideline.
  // Should scale to the guideline size.
  ResetCanvas();
  canvas->DrawRect(inner_frame, flags_opaque);
  ExpectScaledSize(kGuidelineSizeSquare, inner_frame.width());

  // Half size square is too small to be scaled.
  ResetCanvas();
  gfx::RectF half_frame = kIconFrame;
  half_frame.Inset(
      gfx::InsetsF::VH(kIconFrame.height() / 4, kIconFrame.width() / 4));
  canvas->DrawRect(half_frame, flags_opaque);
  EXPECT_EQ(1, GetScale());
}

TEST_F(MdIconNormalizerTest, RoundIcon) {
  // Full size circle, should scale to the exact guideline size.
  canvas->DrawCircle(kCenter, kHalfSize, flags_opaque);
  ExpectScaledSize(kGuidelineSizeRound, kHalfSize * 2);

  // Add a transparent hole in the middle, should scale the same as above.
  constexpr int kInnerRadius = kHalfSize - 4;
  canvas->DrawCircle(kCenter, kInnerRadius, flags_transparent);
  ExpectScaledSize(kGuidelineSizeRound, kHalfSize * 2);

  // Smaller circle, but still larger than the guideline.
  // Should scale to the guideline size.
  ResetCanvas();
  canvas->DrawCircle(kCenter, kInnerRadius, flags_opaque);
  ExpectScaledSize(kGuidelineSizeRound, kInnerRadius * 2);

  // Half size opaque circle, too small to be scaled.
  ResetCanvas();
  constexpr int kRadiusHalf = kHalfSize / 2;
  canvas->DrawCircle(kCenter, kRadiusHalf, flags_opaque);
  EXPECT_EQ(1, GetScale());
}

TEST_F(MdIconNormalizerTest, RectangularIcon) {
  gfx::RectF rect = kIconFrame;

  // Full size square.
  canvas->DrawRect(rect, flags_opaque);
  const float scale_square = GetScale();

  // Same height rectangle, 7:8 aspect ratio, scale should be greater.
  ResetCanvas();
  rect = kIconFrame;
  rect.Inset(gfx::InsetsF::VH(0, kIconSize / 16));
  canvas->DrawRect(rect, flags_opaque);
  const float scale_7_8 = GetScale();
  EXPECT_LT(scale_square, scale_7_8);

  // 3:4 aspect ratio, scale should be greater still.
  ResetCanvas();
  rect = kIconFrame;
  rect.Inset(gfx::InsetsF::VH(0, kIconSize / 8));
  const float scale_3_4 = GetScale();
  EXPECT_LT(scale_7_8, scale_3_4);

  // 1:2 aspect ratio, should not scale.
  ResetCanvas();
  rect = kIconFrame;
  rect.Inset(gfx::InsetsF::VH(0, kIconSize / 4));
  canvas->DrawRect(rect, flags_opaque);
  EXPECT_EQ(1, GetScale());
}

TEST_F(MdIconNormalizerTest, CompareShapes) {
  // Full size square, lowest possible scale.
  canvas->DrawRect(kIconFrame, flags_opaque);
  const float scale_square = GetScale();

  // Same square with rounded corners, fills a smaller fraction of the
  // frame, the scale should be greater.
  ResetCanvas();
  canvas->DrawRoundRect(kIconFrame, kIconSize / 8, flags_opaque);
  const float scale_rounded_1 = GetScale();
  EXPECT_LT(scale_square, scale_rounded_1);

  // Same square with more rounded corners, the scale should be greater still.
  ResetCanvas();
  canvas->DrawRoundRect(kIconFrame, kIconSize / 4, flags_opaque);
  const float scale_rounded_2 = GetScale();
  EXPECT_LT(scale_rounded_1, scale_rounded_2);

  // Full size circle, greater scale.
  ResetCanvas();
  canvas->DrawCircle(kCenter, kHalfSize, flags_opaque);
  const float scale_circle = GetScale();
  EXPECT_LT(scale_rounded_2, scale_circle);

  // An octagon of these particular proportions fills a smaller fraction of the
  // frame than a circle, but still large enough to require downscaling.
  // The scale should be greater.
  ResetCanvas();
  SkPath octagon;
  constexpr int kCutoff = kIconSize / 3;
  octagon.moveTo(0, kCutoff);
  octagon.lineTo(kCutoff, 0);
  octagon.lineTo(kIconSize - kCutoff, 0);
  octagon.lineTo(kIconSize, kCutoff);
  octagon.lineTo(kIconSize, kIconSize - kCutoff);
  octagon.lineTo(kIconSize - kCutoff, kIconSize);
  octagon.lineTo(kCutoff, kIconSize);
  octagon.lineTo(0, kIconSize - kCutoff);
  octagon.lineTo(0, kCutoff);
  canvas->DrawPath(octagon, flags_opaque);
  const float scale_octagon = GetScale();
  EXPECT_LT(scale_circle, scale_octagon);

  // A diamond fills too small a fraction of the frame, should not be scaled.
  ResetCanvas();
  SkPath diamond;
  diamond.moveTo(0, kHalfSize);
  diamond.lineTo(kHalfSize, 0);
  diamond.lineTo(kIconSize, kHalfSize);
  diamond.lineTo(kHalfSize, kIconSize);
  diamond.lineTo(0, kHalfSize);
  canvas->DrawPath(diamond, flags_opaque);
  EXPECT_EQ(1, GetScale());
}

TEST_F(MdIconNormalizerTest, Opacity) {
  // Fully transparent image requires no scaling.
  EXPECT_EQ(1, GetScale());

  gfx::RectF frame = kIconFrame;
  constexpr int kInset = 4;
  frame.Inset(kInset);

  gfx::RectF shadow = frame;
  frame.Offset(kInset, kInset);

  // Draw the shadow, no opaque pixels, no scaling.
  canvas->DrawRect(shadow, flags_shadow);
  EXPECT_EQ(1, GetScale());

  // Add the opaque part (at minimum detectable opacity), should be scale
  // as a square icon.
  canvas->DrawRect(frame, flags_opaque);
  ExpectScaledSize(kGuidelineSizeSquare, frame.width());
}

class MdIconNormalizerTestWithColorType
    : public testing::TestWithParam<SkColorType> {};

TEST_P(MdIconNormalizerTestWithColorType, SquareIcon) {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::Make(kIconSize, kIconSize, GetParam(),
                                   kUnpremul_SkAlphaType));
  bitmap.allocPixels();
  const SkPixmap pixmap = bitmap.pixmap();

  ASSERT_NE(kUnknown_SkAlphaType, pixmap.alphaType());
  ASSERT_NE(kOpaque_SkAlphaType, pixmap.alphaType());

  constexpr int kInset = 4;
  constexpr int kWidth = kIconSize - kInset * 2;
  constexpr int kHeight = kWidth;

  // Transparent bitmap, no scaling.
  pixmap.erase(SK_ColorTRANSPARENT);
  EXPECT_EQ(1, ::GetScale(bitmap));

  // Add shadow, no scaling.
  pixmap.erase(SkColorSetA(kFillColor, 1),
               SkIRect::MakeXYWH(kInset * 2, kInset * 2, kWidth, kHeight));
  EXPECT_EQ(1, ::GetScale(bitmap));

  // Add opaque square, should scale correctly.
  pixmap.erase(kFillColor, SkIRect::MakeXYWH(kInset, kInset, kWidth, kHeight));
  ::ExpectScaledSize(kGuidelineSizeSquare, kWidth, bitmap);

  // Full size square, should scale correctly.
  pixmap.erase(kFillColor);
  ::ExpectScaledSize(kGuidelineSizeSquare, kIconSize, bitmap);

  // Half size square, should not scale.
  pixmap.erase(SK_ColorTRANSPARENT);
  pixmap.erase(kFillColor, SkIRect::MakeXYWH(kHalfSize / 2, kHalfSize / 2,
                                             kHalfSize, kHalfSize));
  EXPECT_EQ(1, ::GetScale(bitmap));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MdIconNormalizerTestWithColorType,
    ::testing::Values(kAlpha_8_SkColorType,
                      kARGB_4444_SkColorType,
                      kRGBA_8888_SkColorType,
                      kBGRA_8888_SkColorType,
                      kRGBA_1010102_SkColorType));

class MdIconNormalizerTestWithNoAlpha
    : public testing::TestWithParam<SkColorType> {};

TEST_P(MdIconNormalizerTestWithNoAlpha, NoScaling) {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::Make(kIconSize, kIconSize, GetParam(),
                                   kUnknown_SkAlphaType));
  bitmap.allocPixels();

  const SkPixmap pixmap = bitmap.pixmap();

  ASSERT_EQ(kOpaque_SkAlphaType, pixmap.alphaType());

  // In the absence of alpha channel, any bitmap should be treated as a solid
  // block of opaque pixels.

  // Transparent color should be treated the same as opaque.
  pixmap.erase(SK_ColorTRANSPARENT);
  ::ExpectScaledSize(kGuidelineSizeSquare, kIconSize, bitmap);

  // Draw a smaller at square at the center, should not make any difference.
  constexpr int kInset = 4;
  constexpr int kWidth = kIconSize - kInset * 2;
  constexpr int kHeight = kWidth;
  pixmap.erase(kFillColor, SkIRect::MakeXYWH(kInset, kInset, kWidth, kHeight));
  ::ExpectScaledSize(kGuidelineSizeSquare, kIconSize, bitmap);

  // Full size square, the result is the same.
  pixmap.erase(kFillColor);
  ::ExpectScaledSize(kGuidelineSizeSquare, kIconSize, bitmap);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MdIconNormalizerTestWithNoAlpha,
    ::testing::Values(kGray_8_SkColorType,
                      kRGB_565_SkColorType,
                      kRGB_888x_SkColorType,
                      kRGB_101010x_SkColorType));
