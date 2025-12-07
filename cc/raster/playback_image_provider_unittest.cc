// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include <utility>
#include <vector>

#include "cc/paint/paint_image_builder.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"

namespace cc {
namespace {

sk_sp<SkImage> CreateRasterImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  return SkImages::RasterFromBitmap(bitmap);
}

DecodedDrawImage CreateDecode() {
  return DecodedDrawImage(CreateRasterImage(), nullptr, SkSize::MakeEmpty(),
                          SkSize::Make(1.0f, 1.0f),
                          PaintFlags::FilterQuality::kMedium, true);
}

class MockDecodeCache : public StubDecodeCache {
 public:
  MockDecodeCache() = default;
  ~MockDecodeCache() override { EXPECT_EQ(refed_image_count_, 0); }

  DecodedDrawImage GetDecodedImageForDraw(
      const DrawImage& draw_image) override {
    last_image_ = draw_image;
    images_decoded_++;
    refed_image_count_++;
    return CreateDecode();
  }

  void DrawWithImageFinished(
      const DrawImage& draw_image,
      const DecodedDrawImage& decoded_draw_image) override {
    refed_image_count_--;
    EXPECT_GE(refed_image_count_, 0);
  }

  bool UseCacheForDrawImage(const DrawImage& image) const override {
    return use_cache_for_draw_image_;
  }

  void set_use_cache_for_draw_image(bool use) {
    use_cache_for_draw_image_ = use;
  }
  int refed_image_count() const { return refed_image_count_; }
  int images_decoded() const { return images_decoded_; }
  const DrawImage& last_image() { return last_image_; }

 private:
  int refed_image_count_ = 0;
  int images_decoded_ = 0;
  bool use_cache_for_draw_image_ = true;
  DrawImage last_image_;
};

TEST(PlaybackImageProviderTest, SkipsAllImages) {
  MockDecodeCache cache;
  PlaybackImageProvider provider(&cache, TargetColorParams(), std::nullopt);

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkM44 matrix = SkM44();

  EXPECT_FALSE(provider.GetRasterContent(DrawImage(
      PaintImageBuilder::WithDefault()
          .set_id(PaintImage::GetNextId())
          .set_image(CreateRasterImage(), PaintImage::GetNextContentId())
          .TakePaintImage(),
      false, rect, PaintFlags::FilterQuality::kMedium, matrix)));
  EXPECT_EQ(cache.images_decoded(), 0);

  EXPECT_FALSE(provider.GetRasterContent(
      CreateDiscardableDrawImage(gfx::Size(10, 10), nullptr, SkRect::Make(rect),
                                 PaintFlags::FilterQuality::kMedium, matrix)));
  EXPECT_EQ(cache.images_decoded(), 0);
}

TEST(PlaybackImageProviderTest, SkipsSomeImages) {
  MockDecodeCache cache;
  PaintImage skip_image = CreateDiscardablePaintImage(gfx::Size(10, 10));

  std::optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  settings->images_to_skip = {skip_image.stable_id()};

  PlaybackImageProvider provider(&cache, TargetColorParams(),
                                 std::move(settings));

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkM44 matrix = SkM44();
  EXPECT_FALSE(provider.GetRasterContent(DrawImage(
      skip_image, false, rect, PaintFlags::FilterQuality::kMedium, matrix)));
  EXPECT_EQ(cache.images_decoded(), 0);
}

TEST(PlaybackImageProviderTest, RefAndUnrefDecode) {
  MockDecodeCache cache;

  std::optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  PlaybackImageProvider provider(&cache, TargetColorParams(),
                                 std::move(settings));

  {
    SkRect rect = SkRect::MakeWH(10, 10);
    SkM44 matrix = SkM44();
    auto decode = provider.GetRasterContent(
        CreateDiscardableDrawImage(gfx::Size(10, 10), nullptr, rect,
                                   PaintFlags::FilterQuality::kMedium, matrix));
    EXPECT_TRUE(decode);
    EXPECT_EQ(cache.refed_image_count(), 1);
  }

  // Destroying the decode unrefs the image from the cache.
  EXPECT_EQ(cache.refed_image_count(), 0);
}

TEST(PlaybackImageProviderTest, SwapsGivenFrames) {
  MockDecodeCache cache;
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(2)),
      FrameMetadata(true, base::Milliseconds(3))};
  PaintImage image = CreateAnimatedImage(gfx::Size(10, 10), frames);

  base::flat_map<PaintImage::Id, size_t> image_to_frame;
  image_to_frame[image.stable_id()] = 1u;
  std::optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  settings->image_to_current_frame_index = image_to_frame;

  PlaybackImageProvider provider(&cache, TargetColorParams(),
                                 std::move(settings));

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkM44 matrix = SkM44();
  DrawImage draw_image(image, false, rect, PaintFlags::FilterQuality::kMedium,
                       matrix);
  provider.GetRasterContent(draw_image);
  ASSERT_TRUE(cache.last_image().paint_image());
  ASSERT_TRUE(cache.last_image().paint_image().IsSameForTesting(image));
  ASSERT_EQ(cache.last_image().frame_index(), 1u);
}

TEST(PlaybackImageProviderTest, BitmapImages) {
  MockDecodeCache cache;

  std::optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  PlaybackImageProvider provider(&cache, TargetColorParams(),
                                 std::move(settings));

  {
    SkIRect rect = SkIRect::MakeWH(10, 10);
    SkM44 matrix = SkM44();
    auto draw_image =
        DrawImage(CreateBitmapImage(gfx::Size(10, 10)), false, rect,
                  PaintFlags::FilterQuality::kMedium, matrix);
    auto decode = provider.GetRasterContent(draw_image);
    EXPECT_TRUE(decode);
    EXPECT_EQ(cache.refed_image_count(), 1);
  }

  // Destroying the decode unrefs the image from the cache.
  EXPECT_EQ(cache.refed_image_count(), 0);
}

TEST(PlaybackImageProviderTest, IgnoresImagesNotSupportedByCache) {
  MockDecodeCache cache;
  cache.set_use_cache_for_draw_image(false);
  std::optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  PlaybackImageProvider provider(&cache, TargetColorParams(),
                                 std::move(settings));
  {
    SkIRect rect = SkIRect::MakeWH(10, 10);
    SkM44 matrix = SkM44();
    auto draw_image =
        DrawImage(CreateBitmapImage(gfx::Size(10, 10)), false, rect,
                  PaintFlags::FilterQuality::kMedium, matrix);
    auto decode = provider.GetRasterContent(draw_image);
    EXPECT_TRUE(decode);
    EXPECT_EQ(cache.refed_image_count(), 0);
  }

  EXPECT_EQ(cache.refed_image_count(), 0);
}

}  // namespace
}  // namespace cc
