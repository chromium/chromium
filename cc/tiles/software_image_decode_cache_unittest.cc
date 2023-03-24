// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/software_image_decode_cache.h"

#include "cc/paint/draw_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_tile_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {
namespace {

TargetColorParams DefaultTargetColorParams() {
  return TargetColorParams();
}

sk_sp<SkColorSpace> DefaultSkColorSpace() {
  return DefaultTargetColorParams().color_space.ToSkColorSpace();
}

size_t kLockedMemoryLimitBytes = 128 * 1024 * 1024;
class TestSoftwareImageDecodeCache : public SoftwareImageDecodeCache {
 public:
  TestSoftwareImageDecodeCache()
      : SoftwareImageDecodeCache(kN32_SkColorType, kLockedMemoryLimitBytes) {}
};

SkM44 CreateMatrix(const SkSize& scale, bool is_decomposable) {
  SkM44 matrix = SkM44::Scale(scale.width(), scale.height());

  if (!is_decomposable) {
    // Perspective is not decomposable, add it.
    matrix.setRC(3, 0, 0.1f);
  }

  return matrix;
}

PaintImage CreatePaintImage(int width, int height) {
  return CreateDiscardablePaintImage(gfx::Size(width, height),
                                     DefaultSkColorSpace());
}

PaintImage CreatePaintImage(int width,
                            int height,
                            const TargetColorParams& target_color_params) {
  return CreateDiscardablePaintImage(
      gfx::Size(width, height),
      target_color_params.color_space.ToSkColorSpace());
}

class SoftwareImageDecodeCacheTest : public testing::Test {
 public:
  SoftwareImageDecodeCacheTest()
      : cache_client_id_(cache_.GenerateClientId()) {}

 protected:
  TestSoftwareImageDecodeCache cache_;
  const ImageDecodeCache::ClientId cache_client_id_;
};

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyNoneQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kNone,
      CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_TRUE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  // Since the original decode will be used, the locked_bytes is that of the
  // original image.
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyLowQualityIncreasedToMediumIfDownscale) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kLow,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(50, key.target_size().height());
  EXPECT_EQ(50u * 50u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyMediumQualityDropsToLowIfMipLevel0) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kMedium,
      CreateMatrix(SkSize::Make(0.75f, 0.75f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, LowUnscalableFormatStaysLow) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kLow,
      CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kARGB_4444_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, HighUnscalableFormatBecomesLow) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kHigh,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kARGB_4444_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyLowQualityKeptLowIfUpscale) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kLow,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.4f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(50, key.target_size().height());
  EXPECT_EQ(50u * 50u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyMediumQualityDropToLowIfEnlarging) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityDropToLowIfIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyMediumQualityDropToLowIfNearlyIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.001f, 1.001f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyMediumQualityDropToLowIfNearlyIdentity2) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.999f, 0.999f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyMediumQualityDropToLowIfNotDecomposable) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = false;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());

  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt1_5Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(500, key.target_size().width());
  EXPECT_EQ(200, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(500u * 200u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt1_0cale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(500, key.target_size().width());
  EXPECT_EQ(200, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(500u * 200u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyLowQualityAt0_75Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.75f, 0.75f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(500, key.target_size().width());
  EXPECT_EQ(200, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(500u * 200u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_5Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(250, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(250u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_49Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.49f, 0.49f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(250, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(250u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_1Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.1f, 0.1f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(63, key.target_size().width());
  EXPECT_EQ(25, key.target_size().height());
  EXPECT_EQ(63u * 25u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_01Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.01f, 0.01f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(8, key.target_size().width());
  EXPECT_EQ(4, key.target_size().height());
  EXPECT_EQ(8u * 4u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyFullDowscalesDropsHighQualityToMedium) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.2f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(50, key.target_size().height());
  EXPECT_EQ(50u * 50u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyUpscaleIsLowQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(2.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyHighQualityDropToMediumIfTooLarge) {
  // Just over 64MB when scaled.
  PaintImage paint_image = CreatePaintImage(4555, 2048);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  // At least one dimension should scale down, so that medium quality doesn't
  // become low.
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.45f, 0.45f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(2278, key.target_size().width());
  EXPECT_EQ(1024, key.target_size().height());
  EXPECT_EQ(2278u * 1024u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyHighQualityDropToLowIfNotDecomposable) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = false;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyHighQualityDropToLowIfIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyHighQualityDropToLowIfNearlyIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.001f, 1.001f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest,
       ImageKeyHighQualityDropToLowIfNearlyIdentity2) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.999f, 0.999f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageKeyDownscaleMipLevelWithRounding) {
  // Tests that, when using a non-zero mip level, the final target size (which
  // is the size of the chosen mip level) is as expected if rounding is
  // required.
  //
  // The 97x61 dimensions and the (0.2f, 0.2f) scaling were chosen specifically
  // so that:
  //
  // - The starting target size is 19x12 which means that 2 is the chosen mip
  //   level.
  //
  // - Attempting to get the final target size by simply multiplying the
  //   dimensions of the |src_rect| (97x61) times
  //   MipMapUtil::GetScaleAdjustmentForLevel() yields 24x15 if we attempt to
  //   store the result as integers. This is inconsistent with the rounding
  //   behavior introduced in https://crrev.com/c/1107049 and was the cause of
  //   https://crbug.com/891316.
  PaintImage paint_image = CreatePaintImage(97, 61);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kMedium,
      CreateMatrix(SkSize::Make(0.2f, 0.2f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(25, key.target_size().width());
  EXPECT_EQ(16, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(25u * 16u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, OriginalDecodesAreEqual) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kNone;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_TRUE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());

  DrawImage another_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto another_key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      another_draw_image, kN32_SkColorType);
  EXPECT_EQ(another_draw_image.frame_key(), another_key.frame_key());
  EXPECT_TRUE(another_key.is_nearest_neighbor());
  EXPECT_EQ(100, another_key.target_size().width());
  EXPECT_EQ(100, another_key.target_size().height());
  EXPECT_EQ(another_key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, another_key.locked_bytes());

  EXPECT_TRUE(key == another_key);
}

TEST_F(SoftwareImageDecodeCacheTest, ImageRectDoesNotContainSrcRect) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeXYWH(25, 35, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(gfx::Rect(25, 35, 75, 65), key.src_rect());
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, ImageRectDoesNotContainSrcRectWithScale) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeXYWH(20, 30, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(40, key.target_size().width());
  EXPECT_EQ(35, key.target_size().height());
  EXPECT_EQ(gfx::Rect(20, 30, 80, 70), key.src_rect());
  EXPECT_EQ(40u * 35u * 4u, key.locked_bytes());
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageSameImage) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult another_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task.get());

  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageProcessUnrefCancel) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task.get());
  cache_.UnrefImage(draw_image);

  result = cache_.GetTaskForImageAndRef(cache_client_id_, draw_image,
                                        ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());
  // This is expected to pass instead of DCHECKing since we're reducing the ref
  // for an image which isn't locked to begin with.
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageSameImageDifferentQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;

  DrawImage high_quality_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kHigh,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult high_quality_result =
      cache_.GetTaskForImageAndRef(cache_client_id_, high_quality_draw_image,
                                   ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(high_quality_result.need_unref);
  EXPECT_TRUE(high_quality_result.task);

  DrawImage none_quality_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      PaintFlags::FilterQuality::kNone,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult none_quality_result =
      cache_.GetTaskForImageAndRef(cache_client_id_, none_quality_draw_image,
                                   ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(none_quality_result.need_unref);
  EXPECT_TRUE(none_quality_result.task);
  EXPECT_TRUE(high_quality_result.task.get() != none_quality_result.task.get());

  TestTileTaskRunner::ProcessTask(high_quality_result.task.get());
  TestTileTaskRunner::ProcessTask(none_quality_result.task.get());

  cache_.UnrefImage(high_quality_draw_image);
  cache_.UnrefImage(none_quality_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageSameImageDifferentSize) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  DrawImage half_size_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult half_size_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, half_size_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(half_size_result.need_unref);
  EXPECT_TRUE(half_size_result.task);

  DrawImage quarter_size_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult quarter_size_result =
      cache_.GetTaskForImageAndRef(cache_client_id_, quarter_size_draw_image,
                                   ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(quarter_size_result.need_unref);
  EXPECT_TRUE(quarter_size_result.task);
  EXPECT_TRUE(half_size_result.task.get() != quarter_size_result.task.get());

  TestTileTaskRunner::ProcessTask(half_size_result.task.get());
  TestTileTaskRunner::ProcessTask(quarter_size_result.task.get());

  cache_.UnrefImage(half_size_draw_image);
  cache_.UnrefImage(quarter_size_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageDifferentImage) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage first_paint_image = CreatePaintImage(100, 100);
  DrawImage first_draw_image(
      first_paint_image, false,
      SkIRect::MakeWH(first_paint_image.width(), first_paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult first_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  PaintImage second_paint_image = CreatePaintImage(100, 100);
  DrawImage second_draw_image(
      second_paint_image, false,
      SkIRect::MakeWH(second_paint_image.width(), second_paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult second_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache_.UnrefImage(first_draw_image);
  cache_.UnrefImage(second_draw_image);
}

// crbug.com/709341
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetTaskForImageDifferentColorSpace \
  DISABLED_GetTaskForImageDifferentColorSpace
#else
#define MAYBE_GetTaskForImageDifferentColorSpace \
  GetTaskForImageDifferentColorSpace
#endif
TEST_F(SoftwareImageDecodeCacheTest, MAYBE_GetTaskForImageDifferentColorSpace) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  TargetColorParams target_color_params_a(gfx::ColorSpace(
      gfx::ColorSpace::PrimaryID::XYZ_D50, gfx::ColorSpace::TransferID::SRGB));

  TargetColorParams target_color_params_b(
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                      gfx::ColorSpace::TransferID::SRGB));

  TargetColorParams target_color_params_c(gfx::ColorSpace::CreateSRGB());

  PaintImage paint_image = CreatePaintImage(100, 100, target_color_params_a);
  DrawImage first_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, target_color_params_b);
  ImageDecodeCache::TaskResult first_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  DrawImage second_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, target_color_params_c);
  ImageDecodeCache::TaskResult second_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, target_color_params_b);
  ImageDecodeCache::TaskResult third_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_TRUE(first_result.task.get() == third_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache_.UnrefImage(first_draw_image);
  cache_.UnrefImage(second_draw_image);
  cache_.UnrefImage(third_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageAlreadyDecoded) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ScheduleTask(result.task.get());
  TestTileTaskRunner::RunTask(result.task.get());

  ImageDecodeCache::TaskResult another_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);

  TestTileTaskRunner::CompleteTask(result.task.get());

  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageAlreadyPrerolled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kLow;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ScheduleTask(result.task.get());
  TestTileTaskRunner::RunTask(result.task.get());

  ImageDecodeCache::TaskResult another_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);

  TestTileTaskRunner::CompleteTask(result.task.get());

  ImageDecodeCache::TaskResult third_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_FALSE(third_result.task);

  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetTaskForImageCanceledGetsNewTask) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  ImageDecodeCache::TaskResult another_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, complete it (it was canceled).
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Fully cancel everything (so the raster would unref things).
  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);

  // Here a new task is created.
  ImageDecodeCache::TaskResult third_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  TestTileTaskRunner::ProcessTask(third_result.task.get());

  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest,
       GetTaskForImageCanceledWhileReffedGetsNewTask) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  ImageDecodeCache::TaskResult another_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, complete it (it was canceled).
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Note that here, everything is reffed, but a new task is created. This is
  // possible with repeated schedule/cancel operations.
  ImageDecodeCache::TaskResult third_result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  TestTileTaskRunner::ProcessTask(third_result.task.get());

  // 3 Unrefs!!!
  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetDecodedImageForDraw) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest,
       GetDecodedImageForDrawWithNonContainedSrcRect) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeXYWH(20, 30, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(40, decoded_draw_image.image()->width());
  EXPECT_EQ(35, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, GetDecodedImageForDrawAtRasterDecode) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest,
       GetDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  DecodedDrawImage another_decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.DrawWithImageFinished(draw_image, another_decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, ZeroSizedImagesAreSkipped) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.f, 0.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, NonOverlappingSrcRectImagesAreSkipped) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeXYWH(150, 150, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, LowQualityFilterIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kLow;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // If we decoded the image and cached it, it would be stored in a different
  // SkImage object.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, LowQualityScaledSubrectIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kLow;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false, SkIRect::MakeXYWH(10, 10, 80, 80), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // If we decoded the image and cached it, it would be stored in a different
  // SkImage object.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  // Low quality will be upgraded to medium and mip-mapped.
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, NoneQualityScaledSubrectIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kNone;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, false, SkIRect::MakeXYWH(10, 10, 80, 80), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // If we decoded the image and cached it, it would be stored in a different
  // SkImage object.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kNone,
            decoded_draw_image.filter_quality());
  EXPECT_TRUE(decoded_draw_image.is_scale_adjustment_identity());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt01_5ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(500, decoded_draw_image.image()->width());
  EXPECT_EQ(200, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt1_0ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(500, decoded_draw_image.image()->width());
  EXPECT_EQ(200, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt0_75ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.75f, 0.75f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(500, decoded_draw_image.image()->width());
  EXPECT_EQ(200, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt0_5ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(250, decoded_draw_image.image()->width());
  EXPECT_EQ(100, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt0_49ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.49f, 0.49f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(250, decoded_draw_image.image()->width());
  EXPECT_EQ(100, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt0_1ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.1f, 0.1f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(63, decoded_draw_image.image()->width());
  EXPECT_EQ(25, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt0_01ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.01f, 0.01f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image.filter_quality());
  EXPECT_EQ(8, decoded_draw_image.image()->width());
  EXPECT_EQ(4, decoded_draw_image.image()->height());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, MediumQualityAt0_001ScaleIsHandled) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.001f, 0.001f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest,
       MediumQualityImagesAreTheSameAt0_5And0_49Scale) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image_50(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  DrawImage draw_image_49(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.49f, 0.49f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result_50 = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image_50, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result_50.task);
  EXPECT_TRUE(result_50.need_unref);
  ImageDecodeCache::TaskResult result_49 = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image_49, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result_49.task);
  EXPECT_TRUE(result_49.need_unref);

  TestTileTaskRunner::ProcessTask(result_49.task.get());

  DecodedDrawImage decoded_draw_image_50 =
      cache_.GetDecodedImageForDraw(draw_image_50);
  EXPECT_TRUE(decoded_draw_image_50.image());
  DecodedDrawImage decoded_draw_image_49 =
      cache_.GetDecodedImageForDraw(draw_image_49);
  EXPECT_TRUE(decoded_draw_image_49.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image_50.image()->isLazyGenerated());
  EXPECT_FALSE(decoded_draw_image_49.image()->isLazyGenerated());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image_50.filter_quality());
  EXPECT_EQ(PaintFlags::FilterQuality::kLow,
            decoded_draw_image_49.filter_quality());
  EXPECT_EQ(250, decoded_draw_image_50.image()->width());
  EXPECT_EQ(250, decoded_draw_image_49.image()->width());
  EXPECT_EQ(100, decoded_draw_image_50.image()->height());
  EXPECT_EQ(100, decoded_draw_image_49.image()->height());

  EXPECT_EQ(decoded_draw_image_50.image(), decoded_draw_image_49.image());

  cache_.DrawWithImageFinished(draw_image_50, decoded_draw_image_50);
  cache_.UnrefImage(draw_image_50);
  cache_.DrawWithImageFinished(draw_image_49, decoded_draw_image_49);
  cache_.UnrefImage(draw_image_49);
}

TEST_F(SoftwareImageDecodeCacheTest, ClearCache) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  for (int i = 0; i < 10; ++i) {
    PaintImage paint_image = CreatePaintImage(100, 100);
    DrawImage draw_image(
        paint_image, false,
        SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
    ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
        cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache_.UnrefImage(draw_image);
  }

  EXPECT_EQ(10u, cache_.GetNumCacheEntriesForTesting());

  // Tell our cache to clear resources.
  cache_.ClearCache();

  EXPECT_EQ(0u, cache_.GetNumCacheEntriesForTesting());
}

TEST_F(SoftwareImageDecodeCacheTest, CacheDecodesExpectedFrames) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(2)),
      FrameMetadata(true, base::Milliseconds(3)),
      FrameMetadata(true, base::Milliseconds(4)),
      FrameMetadata(true, base::Milliseconds(5)),
  };
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB()), frames);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();

  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;
  DrawImage draw_image(image, false,
                       SkIRect::MakeWH(image.width(), image.height()), quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       1u, DefaultTargetColorParams());
  auto decoded_image = cache_.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();
  cache_.DrawWithImageFinished(draw_image, decoded_image);

  // Scaled.
  DrawImage scaled_draw_image(draw_image, 0.5f, 2u,
                              draw_image.target_color_params());
  decoded_image = cache_.GetDecodedImageForDraw(scaled_draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(2u), 1u);
  generator->reset_frames_decoded();
  cache_.DrawWithImageFinished(scaled_draw_image, decoded_image);

  // Subset.
  DrawImage subset_draw_image(
      image, false, SkIRect::MakeWH(5, 5), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable), 3u,
      DefaultTargetColorParams());
  decoded_image = cache_.GetDecodedImageForDraw(subset_draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(3u), 1u);
  generator->reset_frames_decoded();
  cache_.DrawWithImageFinished(subset_draw_image, decoded_image);
}

TEST_F(SoftwareImageDecodeCacheTest, SizeSubrectingIsHandled) {
  const int min_dimension = 4 * 1024 + 2;
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kLow;

  auto paint_image = CreateDiscardablePaintImage(
      gfx::Size(min_dimension, min_dimension), DefaultSkColorSpace(), false);
  DrawImage draw_image(
      paint_image, false, SkIRect::MakeXYWH(0, 0, 10, 10), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
      cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  // Since we didn't allocate any backing for the memory, we expect this to be
  // false. This test is here to ensure that we at least got to the point where
  // we tried to decode something instead of recursing infinitely.
  EXPECT_FALSE(decoded_draw_image.image());
  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache_.UnrefImage(draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, EmptyTargetSizeDecode) {
  // Tests that requesting an empty sized decode followed by an original sized
  // decode returns no decoded images. This is a regression test. See
  // crbug.com/802976.

  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kLow;

  // Populate the cache with an original sized decode.
  auto paint_image =
      CreateDiscardablePaintImage(gfx::Size(100, 100), DefaultSkColorSpace());
  DrawImage draw_image(paint_image, false, SkIRect::MakeWH(100, 100), quality,
                       CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
                       PaintImage::kDefaultFrameIndex,
                       DefaultTargetColorParams());
  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);

  // Ask for another decode, this time with an empty subrect.
  DrawImage empty_draw_image(
      paint_image, false, SkIRect::MakeEmpty(), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  DecodedDrawImage empty_decoded_draw_image =
      cache_.GetDecodedImageForDraw(empty_draw_image);
  EXPECT_FALSE(empty_decoded_draw_image.image());
  cache_.DrawWithImageFinished(empty_draw_image, empty_decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, BitmapImageColorConverted) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;
  const TargetColorParams target_color_params(
      gfx::ColorSpace::CreateDisplayP3D65());

  PaintImage paint_image = CreateBitmapImage(gfx::Size(100, 100));
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, target_color_params);

  DecodedDrawImage decoded_draw_image =
      cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Expect that we allocated a new image.
  EXPECT_NE(decoded_draw_image.image().get(), paint_image.GetSwSkImage().get());
  // Expect that the image color space match the target color space.
  EXPECT_TRUE(decoded_draw_image.image()->colorSpace());
  EXPECT_TRUE(SkColorSpace::Equals(
      decoded_draw_image.image()->colorSpace(),
      target_color_params.color_space.ToSkColorSpace().get()));

  cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_F(SoftwareImageDecodeCacheTest, BitmapImageNotColorConverted) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  PaintImage paint_image = CreateBitmapImage(gfx::Size(100, 100));
  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

  // The cache should not support this image.
  EXPECT_FALSE(cache_.UseCacheForDrawImage(draw_image));
}

// TODO(ccameron): Re-enable this when the root cause of crashes is discovered.
// https://crbug.com/791828
TEST_F(SoftwareImageDecodeCacheTest, DISABLED_ContentIdCaching) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;
  PaintImage::Id stable_id = 1001;

  for (int i = 0; i < 10; ++i) {
    // Create several images with the same stable id, but new content ids.
    PaintImage paint_image = CreateDiscardablePaintImage(
        gfx::Size(100, 100), nullptr, true, stable_id);

    // Cache two entries of different scales.
    for (int j = 0; j < 2; ++j) {
      float scale = j == 0 ? 1.f : 0.5f;
      DrawImage draw_image(
          paint_image, false,
          SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
          CreateMatrix(SkSize::Make(scale, scale), is_decomposable),
          PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
      DecodedDrawImage decoded_draw_image =
          cache_.GetDecodedImageForDraw(draw_image);
      EXPECT_TRUE(decoded_draw_image.image());
      cache_.DrawWithImageFinished(draw_image, decoded_draw_image);
    }

    // After the first two entries come in, we start evicting old content ids.
    if (i == 0)
      EXPECT_LE(cache_.GetNumCacheEntriesForTesting(), 2u);
    else
      EXPECT_LE(cache_.GetNumCacheEntriesForTesting(), 4u);
  }
}

TEST_F(SoftwareImageDecodeCacheTest, DecodeToScale) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  // Scale to mip level 1, there should be a single entry in the cache from
  // the direct decode.
  DrawImage draw_image1(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  DecodedDrawImage decoded_image1 = cache_.GetDecodedImageForDraw(draw_image1);
  ASSERT_TRUE(decoded_image1.image());
  EXPECT_EQ(decoded_image1.image()->width(), 50);
  EXPECT_EQ(decoded_image1.image()->height(), 50);
  EXPECT_EQ(cache_.GetNumCacheEntriesForTesting(), 1u);

  // We should have requested a scaled decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 50);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 50);

  // Scale to mip level 2, we should be using the existing entry instead of
  // re-decoding.
  DrawImage draw_image2(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.25, 0.25), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  DecodedDrawImage decoded_image2 = cache_.GetDecodedImageForDraw(draw_image2);
  ASSERT_TRUE(decoded_image2.image());
  EXPECT_EQ(decoded_image2.image()->width(), 25);
  EXPECT_EQ(decoded_image2.image()->height(), 25);
  EXPECT_EQ(cache_.GetNumCacheEntriesForTesting(), 2u);

  // Since we scaled from the existing entry, no new decodes should be
  // requested from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 50);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 50);

  cache_.DrawWithImageFinished(draw_image1, decoded_image1);
  cache_.DrawWithImageFinished(draw_image2, decoded_image2);
}

TEST_F(SoftwareImageDecodeCacheTest, DecodeToScaleSubrect) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kMedium;

  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  // Scale to mip level 1, there should be 2 entries in the cache, since the
  // subrect vetoes decode to scale.
  DrawImage draw_image(paint_image, false, SkIRect::MakeWH(50, 50), quality,
                       CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
                       PaintImage::kDefaultFrameIndex,
                       DefaultTargetColorParams());
  DecodedDrawImage decoded_image = cache_.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  EXPECT_EQ(decoded_image.image()->width(), 25);
  EXPECT_EQ(decoded_image.image()->height(), 25);
  EXPECT_EQ(cache_.GetNumCacheEntriesForTesting(), 2u);

  // We should have requested the original decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 100);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 100);
  cache_.DrawWithImageFinished(draw_image, decoded_image);
}

TEST_F(SoftwareImageDecodeCacheTest, DecodeToScaleNoneQuality) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kNone;

  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  DrawImage draw_image(
      paint_image, false,
      SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
      CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
  DecodedDrawImage decoded_image = cache_.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  EXPECT_EQ(decoded_image.image()->width(), 100);
  EXPECT_EQ(decoded_image.image()->height(), 100);

  // We should have requested the original decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 100);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 100);
  cache_.DrawWithImageFinished(draw_image, decoded_image);
}

TEST_F(SoftwareImageDecodeCacheTest, HdrDecodeToHdr) {
  const TargetColorParams target_color_params(gfx::ColorSpace::CreateHDR10());
  auto size = SkISize::Make(100, 100);
  auto info = SkImageInfo::Make(
      size.width(), size.height(), kRGBA_F16_SkColorType, kPremul_SkAlphaType,
      target_color_params.color_space.ToSkColorSpace());
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::kInvalidId)
                         .set_is_high_bit_depth(true)
                         .set_image(SkImages::RasterFromBitmap(bitmap),
                                    PaintImage::GetNextContentId())
                         .TakePaintImage();

  DrawImage draw_image(image, false,
                       SkIRect::MakeWH(image.width(), image.height()),
                       PaintFlags::FilterQuality::kMedium,
                       CreateMatrix(SkSize::Make(0.5, 0.5), true),
                       PaintImage::kDefaultFrameIndex, target_color_params);

  DecodedDrawImage decoded_image = cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_image.image()->colorType(), kRGBA_F16_SkColorType);
  cache_.DrawWithImageFinished(draw_image, decoded_image);
}

TEST_F(SoftwareImageDecodeCacheTest, HdrDecodeToSdr) {
  auto image_color_space = gfx::ColorSpace::CreateHDR10();
  auto size = SkISize::Make(100, 100);
  auto info = SkImageInfo::Make(size.width(), size.height(),
                                kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                                image_color_space.ToSkColorSpace());
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::kInvalidId)
                         .set_is_high_bit_depth(true)
                         .set_image(SkImages::RasterFromBitmap(bitmap),
                                    PaintImage::GetNextContentId())
                         .TakePaintImage();

  // Note: We use P3 here since software cache shouldn't be used when conversion
  // to SRGB is needed.
  auto raster_color_space = gfx::ColorSpace::CreateDisplayP3D65();
  DrawImage draw_image(
      image, false, SkIRect::MakeWH(image.width(), image.height()),
      PaintFlags::FilterQuality::kMedium,
      CreateMatrix(SkSize::Make(0.5, 0.5), true),
      PaintImage::kDefaultFrameIndex, TargetColorParams(raster_color_space));

  DecodedDrawImage decoded_image = cache_.GetDecodedImageForDraw(draw_image);
  EXPECT_NE(decoded_image.image()->colorType(), kRGBA_F16_SkColorType);
  cache_.DrawWithImageFinished(draw_image, decoded_image);
}

TEST_F(SoftwareImageDecodeCacheTest, ReduceCacheOnUnrefWithTasks) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  for (size_t i = 0; i < 2 * cache_.GetMaxNumCacheEntriesForTesting(); ++i) {
    PaintImage paint_image = CreatePaintImage(100, 100);
    DrawImage draw_image(
        paint_image, false,
        SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());
    ImageDecodeCache::TaskResult result = cache_.GetTaskForImageAndRef(
        cache_client_id_, draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache_.UnrefImage(draw_image);
  }

  EXPECT_EQ(cache_.GetNumCacheEntriesForTesting(),
            cache_.GetMaxNumCacheEntriesForTesting());
}

TEST_F(SoftwareImageDecodeCacheTest, ReduceCacheOnUnrefWithDraw) {
  bool is_decomposable = true;
  PaintFlags::FilterQuality quality = PaintFlags::FilterQuality::kHigh;

  for (size_t i = 0; i < 2 * cache_.GetMaxNumCacheEntriesForTesting(); ++i) {
    PaintImage paint_image = CreatePaintImage(100, 100);
    DrawImage draw_image(
        paint_image, false,
        SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultTargetColorParams());

    DecodedDrawImage decoded_image = cache_.GetDecodedImageForDraw(draw_image);
    cache_.DrawWithImageFinished(draw_image, decoded_image);
  }

  EXPECT_EQ(cache_.GetNumCacheEntriesForTesting(),
            cache_.GetMaxNumCacheEntriesForTesting());
}

}  // namespace
}  // namespace cc
