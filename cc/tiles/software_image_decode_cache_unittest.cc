// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/software_image_decode_cache.h"

#include "cc/paint/draw_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_tile_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

gfx::ColorSpace DefaultColorSpace() {
  return gfx::ColorSpace::CreateSRGB();
}

size_t kLockedMemoryLimitBytes = 128 * 1024 * 1024;
class TestSoftwareImageDecodeCache : public SoftwareImageDecodeCache {
 public:
  TestSoftwareImageDecodeCache()
      : SoftwareImageDecodeCache(kN32_SkColorType,
                                 kLockedMemoryLimitBytes,
                                 PaintImage::kDefaultGeneratorClientId) {}
};

SkMatrix CreateMatrix(const SkSize& scale, bool is_decomposable) {
  SkMatrix matrix;
  matrix.setScale(scale.width(), scale.height());

  if (!is_decomposable) {
    // Perspective is not decomposable, add it.
    matrix[SkMatrix::kMPersp0] = 0.1f;
  }

  return matrix;
}

PaintImage CreatePaintImage(int width,
                            int height,
                            gfx::ColorSpace color_space = DefaultColorSpace()) {
  return CreateDiscardablePaintImage(gfx::Size(width, height),
                                     color_space.ToSkColorSpace());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyNoneQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kNone_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

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

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyLowQualityIncreasedToMediumIfDownscale) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kLow_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(50, key.target_size().height());
  EXPECT_EQ(50u * 50u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityDropsToLowIfMipLevel0) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kMedium_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.75f, 0.75f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, LowUnscalableFormatStaysLow) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kLow_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kARGB_4444_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, HighUnscalableFormatBecomesLow) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kHigh_SkFilterQuality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kARGB_4444_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyLowQualityKeptLowIfUpscale) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kLow_SkFilterQuality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.4f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(50, key.target_size().height());
  EXPECT_EQ(50u * 50u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityDropToLowIfEnlarging) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityDropToLowIfIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyMediumQualityDropToLowIfNearlyIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.001f, 1.001f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyMediumQualityDropToLowIfNearlyIdentity2) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.999f, 0.999f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyMediumQualityDropToLowIfNotDecomposable) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = false;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());

  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt1_5Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(500, key.target_size().width());
  EXPECT_EQ(200, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(500u * 200u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt1_0cale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(500, key.target_size().width());
  EXPECT_EQ(200, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(500u * 200u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyLowQualityAt0_75Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.75f, 0.75f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(500, key.target_size().width());
  EXPECT_EQ(200, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(500u * 200u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_5Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(250, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(250u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_49Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.49f, 0.49f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(250, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(250u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_1Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.1f, 0.1f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(63, key.target_size().width());
  EXPECT_EQ(25, key.target_size().height());
  EXPECT_EQ(63u * 25u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyMediumQualityAt0_01Scale) {
  PaintImage paint_image = CreatePaintImage(500, 200);
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.01f, 0.01f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(8, key.target_size().width());
  EXPECT_EQ(4, key.target_size().height());
  EXPECT_EQ(8u * 4u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyFullDowscalesDropsHighQualityToMedium) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.2f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(50, key.target_size().height());
  EXPECT_EQ(50u * 50u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyUpscaleIsLowQuality) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(2.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyHighQualityDropToMediumIfTooLarge) {
  // Just over 64MB when scaled.
  PaintImage paint_image = CreatePaintImage(4555, 2048);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // At least one dimension should scale down, so that medium quality doesn't
  // become low.
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.45f, 0.45f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(2278, key.target_size().width());
  EXPECT_EQ(1024, key.target_size().height());
  EXPECT_EQ(2278u * 1024u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyHighQualityDropToLowIfNotDecomposable) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = false;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyHighQualityDropToLowIfIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyHighQualityDropToLowIfNearlyIdentity) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.001f, 1.001f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest,
     ImageKeyHighQualityDropToLowIfNearlyIdentity2) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.999f, 0.999f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageKeyDownscaleMipLevelWithRounding) {
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
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kMedium_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.2f, 0.2f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(25, key.target_size().width());
  EXPECT_EQ(16, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(25u * 16u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, OriginalDecodesAreEqual) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kNone_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_TRUE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kOriginal);
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());

  DrawImage another_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.5f, 1.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

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

TEST(SoftwareImageDecodeCacheTest, ImageRectDoesNotContainSrcRect) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image,
      SkIRect::MakeXYWH(25, 35, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_FALSE(key.is_nearest_neighbor());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(gfx::Rect(25, 35, 75, 65), key.src_rect());
  EXPECT_EQ(100u * 100u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, ImageRectDoesNotContainSrcRectWithScale) {
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image,
      SkIRect::MakeXYWH(20, 30, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  auto key = SoftwareImageDecodeCache::CacheKey::FromDrawImage(
      draw_image, kN32_SkColorType);
  EXPECT_EQ(draw_image.frame_key(), key.frame_key());
  EXPECT_EQ(key.type(), SoftwareImageDecodeCache::CacheKey::kSubrectAndScale);
  EXPECT_EQ(40, key.target_size().width());
  EXPECT_EQ(35, key.target_size().height());
  EXPECT_EQ(gfx::Rect(20, 30, 80, 70), key.src_rect());
  EXPECT_EQ(40u * 35u * 4u, key.locked_bytes());
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageSameImage) {
  TestSoftwareImageDecodeCache cache;
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache.GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task.get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageProcessUnrefCancel) {
  TestSoftwareImageDecodeCache cache;
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task.get());
  cache.UnrefImage(draw_image);

  result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());
  // This is expected to pass instead of DCHECKing since we're reducing the ref
  // for an image which isn't locked to begin with.
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageSameImageDifferentQuality) {
  TestSoftwareImageDecodeCache cache;
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;

  DrawImage high_quality_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kHigh_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult high_quality_result =
      cache.GetTaskForImageAndRef(high_quality_draw_image,
                                  ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(high_quality_result.need_unref);
  EXPECT_TRUE(high_quality_result.task);

  DrawImage none_quality_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kNone_SkFilterQuality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult none_quality_result =
      cache.GetTaskForImageAndRef(none_quality_draw_image,
                                  ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(none_quality_result.need_unref);
  EXPECT_TRUE(none_quality_result.task);
  EXPECT_TRUE(high_quality_result.task.get() != none_quality_result.task.get());

  TestTileTaskRunner::ProcessTask(high_quality_result.task.get());
  TestTileTaskRunner::ProcessTask(none_quality_result.task.get());

  cache.UnrefImage(high_quality_draw_image);
  cache.UnrefImage(none_quality_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageSameImageDifferentSize) {
  TestSoftwareImageDecodeCache cache;
  PaintImage paint_image = CreatePaintImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage half_size_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult half_size_result = cache.GetTaskForImageAndRef(
      half_size_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(half_size_result.need_unref);
  EXPECT_TRUE(half_size_result.task);

  DrawImage quarter_size_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult quarter_size_result =
      cache.GetTaskForImageAndRef(quarter_size_draw_image,
                                  ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(quarter_size_result.need_unref);
  EXPECT_TRUE(quarter_size_result.task);
  EXPECT_TRUE(half_size_result.task.get() != quarter_size_result.task.get());

  TestTileTaskRunner::ProcessTask(half_size_result.task.get());
  TestTileTaskRunner::ProcessTask(quarter_size_result.task.get());

  cache.UnrefImage(half_size_draw_image);
  cache.UnrefImage(quarter_size_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageDifferentImage) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_paint_image = CreatePaintImage(100, 100);
  DrawImage first_draw_image(
      first_paint_image,
      SkIRect::MakeWH(first_paint_image.width(), first_paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  PaintImage second_paint_image = CreatePaintImage(100, 100);
  DrawImage second_draw_image(
      second_paint_image,
      SkIRect::MakeWH(second_paint_image.width(), second_paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(first_draw_image);
  cache.UnrefImage(second_draw_image);
}

// crbug.com/709341
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetTaskForImageDifferentColorSpace \
  DISABLED_GetTaskForImageDifferentColorSpace
#else
#define MAYBE_GetTaskForImageDifferentColorSpace \
  GetTaskForImageDifferentColorSpace
#endif
TEST(SoftwareImageDecodeCacheTest, MAYBE_GetTaskForImageDifferentColorSpace) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  gfx::ColorSpace color_space_a(gfx::ColorSpace::PrimaryID::XYZ_D50,
                                gfx::ColorSpace::TransferID::IEC61966_2_1);
  gfx::ColorSpace color_space_b(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                gfx::ColorSpace::TransferID::IEC61966_2_1);
  gfx::ColorSpace color_space_c = gfx::ColorSpace::CreateSRGB();

  PaintImage paint_image = CreatePaintImage(100, 100, color_space_a);
  DrawImage first_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_b);
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  DrawImage second_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_c);
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_b);
  ImageDecodeCache::TaskResult third_result = cache.GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_TRUE(first_result.task.get() == third_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(first_draw_image);
  cache.UnrefImage(second_draw_image);
  cache.UnrefImage(third_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageAlreadyDecoded) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ScheduleTask(result.task.get());
  TestTileTaskRunner::RunTask(result.task.get());

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);

  TestTileTaskRunner::CompleteTask(result.task.get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageAlreadyPrerolled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kLow_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ScheduleTask(result.task.get());
  TestTileTaskRunner::RunTask(result.task.get());

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);

  TestTileTaskRunner::CompleteTask(result.task.get());

  ImageDecodeCache::TaskResult third_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_FALSE(third_result.task);

  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetTaskForImageCanceledGetsNewTask) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, complete it (it was canceled).
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Fully cancel everything (so the raster would unref things).
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);

  // Here a new task is created.
  ImageDecodeCache::TaskResult third_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  TestTileTaskRunner::ProcessTask(third_result.task.get());

  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest,
     GetTaskForImageCanceledWhileReffedGetsNewTask) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, complete it (it was canceled).
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Note that here, everything is reffed, but a new task is created. This is
  // possible with repeated schedule/cancel operations.
  ImageDecodeCache::TaskResult third_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  TestTileTaskRunner::ProcessTask(third_result.task.get());

  // 3 Unrefs!!!
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetDecodedImageForDraw) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest,
     GetDecodedImageForDrawWithNonContainedSrcRect) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image,
      SkIRect::MakeXYWH(20, 30, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(40, decoded_draw_image.image()->width());
  EXPECT_EQ(35, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, GetDecodedImageForDrawAtRasterDecode) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest,
     GetDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());

  DecodedDrawImage another_decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.DrawWithImageFinished(draw_image, another_decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, ZeroSizedImagesAreSkipped) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.f, 0.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, NonOverlappingSrcRectImagesAreSkipped) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image,
      SkIRect::MakeXYWH(150, 150, paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, LowQualityFilterIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kLow_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // If we decoded the image and cached it, it would be stored in a different
  // SkImage object.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, LowQualityScaledSubrectIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kLow_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(paint_image, SkIRect::MakeXYWH(10, 10, 80, 80), quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // If we decoded the image and cached it, it would be stored in a different
  // SkImage object.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  // Low quality will be upgraded to medium and mip-mapped.
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, NoneQualityScaledSubrectIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kNone_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(paint_image, SkIRect::MakeXYWH(10, 10, 80, 80), quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // If we decoded the image and cached it, it would be stored in a different
  // SkImage object.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kNone_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_TRUE(decoded_draw_image.is_scale_adjustment_identity());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt01_5ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(500, decoded_draw_image.image()->width());
  EXPECT_EQ(200, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt1_0ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(500, decoded_draw_image.image()->width());
  EXPECT_EQ(200, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt0_75ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.75f, 0.75f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(500, decoded_draw_image.image()->width());
  EXPECT_EQ(200, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt0_5ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(250, decoded_draw_image.image()->width());
  EXPECT_EQ(100, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt0_49ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.49f, 0.49f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(250, decoded_draw_image.image()->width());
  EXPECT_EQ(100, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt0_1ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.1f, 0.1f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(63, decoded_draw_image.image()->width());
  EXPECT_EQ(25, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt0_01ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.01f, 0.01f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_EQ(8, decoded_draw_image.image()->width());
  EXPECT_EQ(4, decoded_draw_image.image()->height());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, MediumQualityAt0_001ScaleIsHandled) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.001f, 0.001f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest,
     MediumQualityImagesAreTheSameAt0_5And0_49Scale) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  PaintImage paint_image = CreatePaintImage(500, 200);
  DrawImage draw_image_50(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DrawImage draw_image_49(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.49f, 0.49f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result_50 = cache.GetTaskForImageAndRef(
      draw_image_50, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result_50.task);
  EXPECT_TRUE(result_50.need_unref);
  ImageDecodeCache::TaskResult result_49 = cache.GetTaskForImageAndRef(
      draw_image_49, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result_49.task);
  EXPECT_TRUE(result_49.need_unref);

  TestTileTaskRunner::ProcessTask(result_49.task.get());

  DecodedDrawImage decoded_draw_image_50 =
      cache.GetDecodedImageForDraw(draw_image_50);
  EXPECT_TRUE(decoded_draw_image_50.image());
  DecodedDrawImage decoded_draw_image_49 =
      cache.GetDecodedImageForDraw(draw_image_49);
  EXPECT_TRUE(decoded_draw_image_49.image());
  // Decoded image should not be lazy generated.
  EXPECT_FALSE(decoded_draw_image_50.image()->isLazyGenerated());
  EXPECT_FALSE(decoded_draw_image_49.image()->isLazyGenerated());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image_50.filter_quality());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image_49.filter_quality());
  EXPECT_EQ(250, decoded_draw_image_50.image()->width());
  EXPECT_EQ(250, decoded_draw_image_49.image()->width());
  EXPECT_EQ(100, decoded_draw_image_50.image()->height());
  EXPECT_EQ(100, decoded_draw_image_49.image()->height());

  EXPECT_EQ(decoded_draw_image_50.image(), decoded_draw_image_49.image());

  cache.DrawWithImageFinished(draw_image_50, decoded_draw_image_50);
  cache.UnrefImage(draw_image_50);
  cache.DrawWithImageFinished(draw_image_49, decoded_draw_image_49);
  cache.UnrefImage(draw_image_49);
}

TEST(SoftwareImageDecodeCacheTest, ClearCache) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  for (int i = 0; i < 10; ++i) {
    PaintImage paint_image = CreatePaintImage(100, 100);
    DrawImage draw_image(
        paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
        quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);
  }

  EXPECT_EQ(10u, cache.GetNumCacheEntriesForTesting());

  // Tell our cache to clear resources.
  cache.ClearCache();

  EXPECT_EQ(0u, cache.GetNumCacheEntriesForTesting());
}

TEST(SoftwareImageDecodeCacheTest, CacheDecodesExpectedFrames) {
  TestSoftwareImageDecodeCache cache;
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(5)),
  };
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB()), frames);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();

  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       1u, DefaultColorSpace());
  auto decoded_image = cache.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();
  cache.DrawWithImageFinished(draw_image, decoded_image);

  // Scaled.
  DrawImage scaled_draw_image(draw_image, 0.5f, 2u,
                              draw_image.target_color_space());
  decoded_image = cache.GetDecodedImageForDraw(scaled_draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(2u), 1u);
  generator->reset_frames_decoded();
  cache.DrawWithImageFinished(scaled_draw_image, decoded_image);

  // Subset.
  DrawImage subset_draw_image(
      image, SkIRect::MakeWH(5, 5), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable), 3u,
      DefaultColorSpace());
  decoded_image = cache.GetDecodedImageForDraw(subset_draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(3u), 1u);
  generator->reset_frames_decoded();
  cache.DrawWithImageFinished(subset_draw_image, decoded_image);
}

TEST(SoftwareImageDecodeCacheTest, SizeSubrectingIsHandled) {
  const int min_dimension = 4 * 1024 + 2;
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kLow_SkFilterQuality;

  auto paint_image =
      CreateDiscardablePaintImage(gfx::Size(min_dimension, min_dimension),
                                  DefaultColorSpace().ToSkColorSpace(), false);
  DrawImage draw_image(paint_image, SkIRect::MakeXYWH(0, 0, 10, 10), quality,
                       CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::ProcessTask(result.task.get());

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  // Since we didn't allocate any backing for the memory, we expect this to be
  // false. This test is here to ensure that we at least got to the point where
  // we tried to decode something instead of recursing infinitely.
  EXPECT_FALSE(decoded_draw_image.image());
  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST(SoftwareImageDecodeCacheTest, EmptyTargetSizeDecode) {
  // Tests that requesting an empty sized decode followed by an original sized
  // decode returns no decoded images. This is a regression test. See
  // crbug.com/802976.

  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kLow_SkFilterQuality;

  // Populate the cache with an original sized decode.
  auto paint_image = CreateDiscardablePaintImage(
      gfx::Size(100, 100), DefaultColorSpace().ToSkColorSpace());
  DrawImage draw_image(paint_image, SkIRect::MakeWH(100, 100), quality,
                       CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  cache.DrawWithImageFinished(draw_image, decoded_draw_image);

  // Ask for another decode, this time with an empty subrect.
  DrawImage empty_draw_image(
      paint_image, SkIRect::MakeEmpty(), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage empty_decoded_draw_image =
      cache.GetDecodedImageForDraw(empty_draw_image);
  EXPECT_FALSE(empty_decoded_draw_image.image());
  cache.DrawWithImageFinished(empty_draw_image, empty_decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, BitmapImageColorConverted) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  gfx::ColorSpace target_color_space = gfx::ColorSpace::CreateDisplayP3D65();

  PaintImage paint_image = CreateBitmapImage(gfx::Size(100, 100));
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, target_color_space);

  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // Expect that we allocated a new image.
  EXPECT_NE(decoded_draw_image.image().get(), paint_image.GetSkImage().get());
  // Expect that the image color space match the target color space.
  EXPECT_TRUE(decoded_draw_image.image()->colorSpace());
  EXPECT_TRUE(SkColorSpace::Equals(decoded_draw_image.image()->colorSpace(),
                                   target_color_space.ToSkColorSpace().get()));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(SoftwareImageDecodeCacheTest, BitmapImageNotColorConverted) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage paint_image = CreateBitmapImage(gfx::Size(100, 100));
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // The cache should not support this image.
  EXPECT_FALSE(cache.UseCacheForDrawImage(draw_image));
}

// TODO(ccameron): Re-enable this when the root cause of crashes is discovered.
// https://crbug.com/791828
TEST(SoftwareImageDecodeCacheTest, DISABLED_ContentIdCaching) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  PaintImage::Id stable_id = 1001;

  for (int i = 0; i < 10; ++i) {
    // Create several images with the same stable id, but new content ids.
    PaintImage paint_image = CreateDiscardablePaintImage(
        gfx::Size(100, 100), nullptr, true, stable_id);

    // Cache two entries of different scales.
    for (int j = 0; j < 2; ++j) {
      float scale = j == 0 ? 1.f : 0.5f;
      DrawImage draw_image(
          paint_image,
          SkIRect::MakeWH(paint_image.width(), paint_image.height()), quality,
          CreateMatrix(SkSize::Make(scale, scale), is_decomposable),
          PaintImage::kDefaultFrameIndex, DefaultColorSpace());
      DecodedDrawImage decoded_draw_image =
          cache.GetDecodedImageForDraw(draw_image);
      EXPECT_TRUE(decoded_draw_image.image());
      cache.DrawWithImageFinished(draw_image, decoded_draw_image);
    }

    // After the first two entries come in, we start evicting old content ids.
    if (i == 0)
      EXPECT_LE(cache.GetNumCacheEntriesForTesting(), 2u);
    else
      EXPECT_LE(cache.GetNumCacheEntriesForTesting(), 4u);
  }
}

TEST(SoftwareImageDecodeCacheTest, DecodeToScale) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultColorSpace().ToSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  // Scale to mip level 1, there should be a single entry in the cache from
  // the direct decode.
  DrawImage draw_image1(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_image1 = cache.GetDecodedImageForDraw(draw_image1);
  ASSERT_TRUE(decoded_image1.image());
  EXPECT_EQ(decoded_image1.image()->width(), 50);
  EXPECT_EQ(decoded_image1.image()->height(), 50);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 1u);

  // We should have requested a scaled decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 50);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 50);

  // Scale to mip level 2, we should be using the existing entry instead of
  // re-decoding.
  DrawImage draw_image2(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.25, 0.25), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_image2 = cache.GetDecodedImageForDraw(draw_image2);
  ASSERT_TRUE(decoded_image2.image());
  EXPECT_EQ(decoded_image2.image()->width(), 25);
  EXPECT_EQ(decoded_image2.image()->height(), 25);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 2u);

  // Since we scaled from the existing entry, no new decodes should be
  // requested from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 50);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 50);

  cache.DrawWithImageFinished(draw_image1, decoded_image1);
  cache.DrawWithImageFinished(draw_image2, decoded_image2);
}

TEST(SoftwareImageDecodeCacheTest, DecodeToScaleSubrect) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultColorSpace().ToSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  // Scale to mip level 1, there should be 2 entries in the cache, since the
  // subrect vetoes decode to scale.
  DrawImage draw_image(paint_image, SkIRect::MakeWH(50, 50), quality,
                       CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_image = cache.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  EXPECT_EQ(decoded_image.image()->width(), 25);
  EXPECT_EQ(decoded_image.image()->height(), 25);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 2u);

  // We should have requested the original decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 100);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 100);
  cache.DrawWithImageFinished(draw_image, decoded_image);
}

TEST(SoftwareImageDecodeCacheTest, DecodeToScaleNoneQuality) {
  TestSoftwareImageDecodeCache cache;
  bool is_decomposable = true;
  SkFilterQuality quality = kNone_SkFilterQuality;

  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultColorSpace().ToSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_image = cache.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  EXPECT_EQ(decoded_image.image()->width(), 100);
  EXPECT_EQ(decoded_image.image()->height(), 100);

  // We should have requested the original decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 100);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 100);
  cache.DrawWithImageFinished(draw_image, decoded_image);
}

}  // namespace
}  // namespace cc
