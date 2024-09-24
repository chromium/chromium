// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_filter.h"

#include "cc/paint/image_provider.h"
#include "cc/paint/paint_op.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"

namespace cc {
namespace {

class MockImageProvider : public ImageProvider {
 public:
  MockImageProvider() = default;
  ~MockImageProvider() override = default;

  ScopedResult GetRasterContent(const DrawImage& draw_image) override {
    DCHECK(!draw_image.paint_image().IsPaintWorklet());
    image_count_++;
    return ScopedResult(
        DecodedDrawImage(CreateBitmapImage(gfx::Size(10, 10)).GetSwSkImage(),
                         nullptr, SkSize::MakeEmpty(), SkSize::Make(1.0f, 1.0f),
                         draw_image.filter_quality(), true));
  }
  int image_count_ = 0;
};

sk_sp<PaintFilter> CreateTestFilter(PaintFilter::Type filter_type,
                                    bool has_discardable_images) {
  PaintImage image;
  if (has_discardable_images)
    image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  else
    image = CreateNonDiscardablePaintImage(gfx::Size(100, 100));

  auto image_filter = sk_make_sp<ImagePaintFilter>(
      image, SkRect::MakeWH(100.f, 100.f), SkRect::MakeWH(100.f, 100.f),
      PaintFlags::FilterQuality::kNone);
  PaintOpBuffer buffer;
  buffer.push<DrawImageOp>(image, 0.f, 0.f);
  auto record_filter = sk_make_sp<RecordPaintFilter>(
      buffer.ReleaseAsRecord(), SkRect::MakeWH(100.f, 100.f));

  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));

  switch (filter_type) {
    case PaintFilter::Type::kNullFilter:
      NOTREACHED();
    case PaintFilter::Type::kColorFilter:
      return sk_make_sp<ColorFilterPaintFilter>(ColorFilter::MakeLuma(),
                                                image_filter, &crop_rect);
    case PaintFilter::Type::kBlur:
      return sk_make_sp<BlurPaintFilter>(0.1f, 0.2f, SkTileMode::kClamp,
                                         record_filter, &crop_rect);
    case PaintFilter::Type::kDropShadow:
      return sk_make_sp<DropShadowPaintFilter>(
          0.1, 0.2f, 0.3f, 0.4f, SkColors::kWhite,
          DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, image_filter,
          &crop_rect);
    case PaintFilter::Type::kMagnifier:
      return sk_make_sp<MagnifierPaintFilter>(SkRect::MakeWH(100.f, 100.f), 2.f,
                                              0.1f, record_filter, &crop_rect);
    case PaintFilter::Type::kCompose:
      return sk_make_sp<ComposePaintFilter>(image_filter, record_filter);
    case PaintFilter::Type::kAlphaThreshold:
      return sk_make_sp<AlphaThresholdPaintFilter>(
          SkRegion(SkIRect::MakeWH(100, 100)), image_filter, &crop_rect);
    case PaintFilter::Type::kXfermode:
      return sk_make_sp<XfermodePaintFilter>(SkBlendMode::kSrc, image_filter,
                                             record_filter, &crop_rect);
    case PaintFilter::Type::kArithmetic:
      return sk_make_sp<ArithmeticPaintFilter>(0.1f, 0.2f, 0.3f, 0.4f, true,
                                               image_filter, record_filter,
                                               &crop_rect);
    case PaintFilter::Type::kMatrixConvolution: {
      SkScalar scalars[9] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f};
      return sk_make_sp<MatrixConvolutionPaintFilter>(
          SkISize::Make(3, 3), scalars, 0.1f, 0.2f, SkIPoint::Make(2, 2),
          SkTileMode::kRepeat, false, image_filter, &crop_rect);
    }
    case PaintFilter::Type::kDisplacementMapEffect:
      return sk_make_sp<DisplacementMapEffectPaintFilter>(
          SkColorChannel::kR, SkColorChannel::kR, 0.1f, image_filter,
          record_filter, &crop_rect);
    case PaintFilter::Type::kImage:
      return image_filter;
    case PaintFilter::Type::kPaintRecord:
      return record_filter;
    case PaintFilter::Type::kMerge: {
      sk_sp<PaintFilter> filters[2] = {image_filter, record_filter};
      return sk_make_sp<MergePaintFilter>(filters, 2, &crop_rect);
    }
    case PaintFilter::Type::kMorphology:
      return sk_make_sp<MorphologyPaintFilter>(
          MorphologyPaintFilter::MorphType::kDilate, 1, 2, image_filter,
          &crop_rect);
    case PaintFilter::Type::kOffset:
      return sk_make_sp<OffsetPaintFilter>(0.1f, 0.2f, image_filter,
                                           &crop_rect);
    case PaintFilter::Type::kTile:
      return sk_make_sp<TilePaintFilter>(SkRect::MakeWH(100.f, 100.f),
                                         SkRect::MakeWH(200.f, 200.f),
                                         record_filter);
    case PaintFilter::Type::kTurbulence:
      return sk_make_sp<TurbulencePaintFilter>(
          TurbulencePaintFilter::TurbulenceType::kTurbulence, 0.1f, 0.2f, 2,
          0.3f, nullptr, &crop_rect);
    case PaintFilter::Type::kShader: {
      return sk_make_sp<ShaderPaintFilter>(
          PaintShader::MakeImage(image, SkTileMode::kClamp, SkTileMode::kClamp,
                                 nullptr),
          /*alpha=*/1.0f, PaintFlags::FilterQuality::kNone,
          SkImageFilters::Dither::kNo, &crop_rect);
    }
    case PaintFilter::Type::kMatrix:
      return sk_make_sp<MatrixPaintFilter>(
          SkMatrix::I(), PaintFlags::FilterQuality::kNone, record_filter);
    case PaintFilter::Type::kLightingDistant:
      return sk_make_sp<LightingDistantPaintFilter>(
          PaintFilter::LightingType::kDiffuse, SkPoint3::Make(0.1f, 0.2f, 0.3f),
          SkColors::kWhite, 0.1f, 0.2f, 0.3f, image_filter, &crop_rect);
    case PaintFilter::Type::kLightingPoint:
      return sk_make_sp<LightingPointPaintFilter>(
          PaintFilter::LightingType::kDiffuse, SkPoint3::Make(0.1f, 0.2f, 0.3f),
          SkColors::kWhite, 0.1f, 0.2f, 0.3f, record_filter, &crop_rect);
    case PaintFilter::Type::kLightingSpot:
      return sk_make_sp<LightingSpotPaintFilter>(
          PaintFilter::LightingType::kDiffuse, SkPoint3::Make(0.1f, 0.2f, 0.3f),
          SkPoint3::Make(0.4f, 0.5f, 0.6f), 0.1f, 0.2f, SkColors::kWhite, 0.4f,
          0.5f, 0.6f, image_filter, &crop_rect);
  }
  NOTREACHED();
}

}  // namespace

class PaintFilterTest : public ::testing::TestWithParam<uint8_t> {
 public:
  PaintFilter::Type GetParamType() const {
    return static_cast<PaintFilter::Type>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    P,
    PaintFilterTest,
    ::testing::Range(static_cast<uint8_t>(PaintFilter::Type::kColorFilter),
                     static_cast<uint8_t>(PaintFilter::Type::kMaxValue)));

TEST_P(PaintFilterTest, HasDiscardableImagesYes) {
  // TurbulencePaintFilter can not embed images.
  if (GetParamType() == PaintFilter::Type::kTurbulence)
    return;

  EXPECT_TRUE(CreateTestFilter(GetParamType(), true)->has_discardable_images())
      << PaintFilter::TypeToString(GetParamType());
}

TEST_P(PaintFilterTest, HasDiscardableImagesNo) {
  EXPECT_FALSE(
      CreateTestFilter(GetParamType(), false)->has_discardable_images())
      << PaintFilter::TypeToString(GetParamType());
}

TEST_P(PaintFilterTest, SnapshotWithImages) {
  auto filter = CreateTestFilter(GetParamType(), true);
  MockImageProvider image_provider;
  auto snapshot_filter = filter->SnapshotWithImages(&image_provider);
  if (GetParamType() != PaintFilter::Type::kTurbulence) {
    // TurbulencePaintFilter can not embed images.
    EXPECT_GT(image_provider.image_count_, 0)
        << PaintFilter::TypeToString(GetParamType());
  }
  EXPECT_TRUE(filter->EqualsForTesting(*snapshot_filter))
      << PaintFilter::TypeToString(GetParamType());
}

TEST(PaintFilterTest, ImageAnalysisState) {
  auto filter = CreateTestFilter(PaintFilter::Type::kImage, true);
  EXPECT_EQ(filter->image_analysis_state(), ImageAnalysisState::kNoAnalysis);
  filter->set_has_animated_images(true);
  EXPECT_EQ(filter->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
  filter->set_has_animated_images(false);
  EXPECT_EQ(filter->image_analysis_state(),
            ImageAnalysisState::kNoAnimatedImages);
}

}  // namespace cc
