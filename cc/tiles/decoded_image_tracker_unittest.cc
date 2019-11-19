// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/skia_common.h"
#include "cc/tiles/decoded_image_tracker.h"
#include "cc/tiles/image_controller.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestImageController : public ImageController {
 public:
  TestImageController() : ImageController(nullptr, nullptr) {}

  void UnlockImageDecode(ImageDecodeRequestId id) override {
    auto it = std::find_if(
        locked_ids_.begin(), locked_ids_.end(),
        [id](const std::pair<const ImageDecodeRequestId,
                             SoftwareImageDecodeCache::CacheKey>& item) {
          return item.first == id;
        });
    ASSERT_FALSE(it == locked_ids_.end());
    locked_ids_.erase(it);
  }

  ImageDecodeRequestId QueueImageDecode(
      const DrawImage& image,
      ImageDecodedCallback callback) override {
    auto id = next_id_++;
    locked_ids_.insert(
        std::make_pair(id, SoftwareImageDecodeCache::CacheKey::FromDrawImage(
                               image, kRGBA_8888_SkColorType)));
    std::move(callback).Run(id, ImageDecodeResult::SUCCESS);
    return id;
  }

  bool IsDrawImageLocked(const DrawImage& image) {
    SoftwareImageDecodeCache::CacheKey key =
        SoftwareImageDecodeCache::CacheKey::FromDrawImage(
            image, kRGBA_8888_SkColorType);
    return std::find_if(
               locked_ids_.begin(), locked_ids_.end(),
               [&key](
                   const std::pair<const ImageDecodeRequestId,
                                   SoftwareImageDecodeCache::CacheKey>& item) {
                 return item.second == key;
               }) != locked_ids_.end();
  }

  size_t num_locked_images() { return locked_ids_.size(); }

 private:
  ImageDecodeRequestId next_id_ = 1;
  std::unordered_map<ImageDecodeRequestId, SoftwareImageDecodeCache::CacheKey>
      locked_ids_;
};

class DecodedImageTrackerTest : public testing::Test {
 public:
  DecodedImageTrackerTest()
      : task_runner_(new base::TestMockTimeTaskRunner()),
        decoded_image_tracker_(&image_controller_, task_runner_) {
    decoded_image_tracker_.SetTickClockForTesting(
        task_runner_->GetMockTickClock());
  }

  TestImageController* image_controller() { return &image_controller_; }
  DecodedImageTracker* decoded_image_tracker() {
    return &decoded_image_tracker_;
  }
  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  TestImageController image_controller_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  DecodedImageTracker decoded_image_tracker_;
};

TEST_F(DecodedImageTrackerTest, QueueImageLocksImages) {
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      CreateDiscardablePaintImage(gfx::Size(1, 1)), gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
}

TEST_F(DecodedImageTrackerTest, Colorspace) {
  bool locked = false;
  gfx::ColorSpace decoded_color_space(
      gfx::ColorSpace::PrimaryID::XYZ_D50,
      gfx::ColorSpace::TransferID::IEC61966_2_1);
  gfx::ColorSpace srgb_color_space = gfx::ColorSpace::CreateSRGB();
  auto paint_image = CreateDiscardablePaintImage(gfx::Size(1, 1));
  decoded_image_tracker()->QueueImageDecode(
      paint_image, decoded_color_space,
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));

  // Check that the decoded color space images are locked, but if the color
  // space differs then that image is not locked. Note that we use the high
  // filter quality here, since it shouldn't matter and the checks should
  // succeed anyway.
  DrawImage locked_draw_image(
      paint_image, SkIRect::MakeWH(1, 1), kHigh_SkFilterQuality, SkMatrix::I(),
      PaintImage::kDefaultFrameIndex, decoded_color_space);
  EXPECT_TRUE(image_controller()->IsDrawImageLocked(locked_draw_image));
  DrawImage srgb_draw_image(paint_image, SkIRect::MakeWH(1, 1),
                            kHigh_SkFilterQuality, SkMatrix::I(),
                            PaintImage::kDefaultFrameIndex, srgb_color_space);
  EXPECT_FALSE(image_controller()->IsDrawImageLocked(srgb_draw_image));
}

TEST_F(DecodedImageTrackerTest, ImagesTimeOut) {
  // Add an image, this will start a 250ms timeout to release it.
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      CreateDiscardablePaintImage(gfx::Size(1, 1)), gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Advance by 150ms, the image should still be locked.
  task_runner()->FastForwardBy(base::TimeDelta::FromMilliseconds(150));
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Add an image, this will not start a new timeout, as one is pending.
  decoded_image_tracker()->QueueImageDecode(
      CreateDiscardablePaintImage(gfx::Size(1, 1)), gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Advance by 100ms, we our first image should be released.
  // Trigger a single commit, the first image should be unlocked.
  task_runner()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Advance by another 250ms, our second image should release.
  task_runner()->FastForwardBy(base::TimeDelta::FromMilliseconds(250));
  EXPECT_EQ(0u, image_controller()->num_locked_images());
}

TEST_F(DecodedImageTrackerTest, ImageUsedInDraw) {
  // Insert two images:
  bool locked = false;
  auto paint_image_1 = CreateDiscardablePaintImage(gfx::Size(1, 1));
  decoded_image_tracker()->QueueImageDecode(
      paint_image_1, gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  auto paint_image_2 = CreateDiscardablePaintImage(gfx::Size(1, 1));
  decoded_image_tracker()->QueueImageDecode(
      paint_image_2, gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Create dummy draw images for each:
  DrawImage draw_image_1(paint_image_1, SkIRect::MakeWH(1, 1),
                         kHigh_SkFilterQuality, SkMatrix::I(), 0,
                         gfx::ColorSpace());
  DrawImage draw_image_2(paint_image_2, SkIRect::MakeWH(1, 1),
                         kHigh_SkFilterQuality, SkMatrix::I(), 0,
                         gfx::ColorSpace());

  // Both should be in the cache:
  EXPECT_TRUE(image_controller()->IsDrawImageLocked(draw_image_1));
  EXPECT_TRUE(image_controller()->IsDrawImageLocked(draw_image_2));

  // Pretend we've drawn with image 2.
  decoded_image_tracker()->OnImagesUsedInDraw({draw_image_2});

  // Only image 1 should now be in the cache.
  EXPECT_TRUE(image_controller()->IsDrawImageLocked(draw_image_1));
  EXPECT_FALSE(image_controller()->IsDrawImageLocked(draw_image_2));
}

TEST_F(DecodedImageTrackerTest, UnlockAllImages) {
  // Insert two images:
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      CreateDiscardablePaintImage(gfx::Size(1, 1)), gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
  decoded_image_tracker()->QueueImageDecode(
      CreateDiscardablePaintImage(gfx::Size(1, 1)), gfx::ColorSpace(),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Unlock all images.
  decoded_image_tracker()->UnlockAllImages();
  EXPECT_EQ(0u, image_controller()->num_locked_images());
}

}  // namespace cc
