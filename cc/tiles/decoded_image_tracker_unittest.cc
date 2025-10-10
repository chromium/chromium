// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/decoded_image_tracker.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/skia_common.h"
#include "cc/tiles/image_controller.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestImageController : public ImageController {
 public:
  TestImageController()
      : ImageController(nullptr, nullptr, base::DoNothing()) {}

  void UnlockImageDecode(ImageDecodeRequestId id) override {
    auto it = std::ranges::find(locked_ids_, id, &LockedIds::value_type::first);
    ASSERT_FALSE(it == locked_ids_.end());
    locked_ids_.erase(it);
  }

  ImageDecodeRequestId QueueImageDecode(const DrawImage& image,
                                        ImageDecodedCallback callback,
                                        bool speculative) override {
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
    return base::Contains(locked_ids_, key, &LockedIds::value_type::second);
  }

  size_t num_locked_images() { return locked_ids_.size(); }

 private:
  using LockedIds = std::unordered_map<ImageDecodeRequestId,
                                       SoftwareImageDecodeCache::CacheKey>;
  ImageDecodeRequestId next_id_ = 1;
  LockedIds locked_ids_;
};

class DecodedImageTrackerTest : public testing::Test {
 public:
  DecodedImageTrackerTest()
      : task_runner_(new base::TestMockTimeTaskRunner()),
        decoded_image_tracker_(&image_controller_, task_runner_) {
    decoded_image_tracker_.SetTickClockForTesting(
        task_runner_->GetMockTickClock(), task_runner_);
  }

  DrawImage DrawImageForDecoding(const PaintImage& paint_image,
                                 const TargetColorParams& color_params) const {
    return DrawImage(paint_image,
                     /*use_dark_mode=*/false,
                     SkIRect::MakeWH(paint_image.width(), paint_image.height()),
                     PaintFlags::FilterQuality::kNone, SkM44(),
                     PaintImage::kDefaultFrameIndex, color_params);
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
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
}

TEST_F(DecodedImageTrackerTest, Colorspace) {
  bool locked = false;
  gfx::ColorSpace decoded_color_space(gfx::ColorSpace::PrimaryID::XYZ_D50,
                                      gfx::ColorSpace::TransferID::SRGB);
  TargetColorParams srgb_target_color_params;
  auto paint_image = CreateDiscardablePaintImage(gfx::Size(1, 1));
  TargetColorParams target_color_params;
  target_color_params.color_space = decoded_color_space;
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(paint_image, target_color_params),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);

  // Check that the decoded color space images are locked, but if the color
  // space differs then that image is not locked. Note that we use the high
  // filter quality here, since it shouldn't matter and the checks should
  // succeed anyway.
  DrawImage locked_draw_image(paint_image, false, SkIRect::MakeWH(1, 1),
                              PaintFlags::FilterQuality::kHigh, SkM44(),
                              PaintImage::kDefaultFrameIndex,
                              target_color_params);
  EXPECT_TRUE(image_controller()->IsDrawImageLocked(locked_draw_image));
  DrawImage srgb_draw_image(paint_image, false, SkIRect::MakeWH(1, 1),
                            PaintFlags::FilterQuality::kHigh, SkM44(),
                            PaintImage::kDefaultFrameIndex,
                            srgb_target_color_params);
  EXPECT_FALSE(image_controller()->IsDrawImageLocked(srgb_draw_image));
}

TEST_F(DecodedImageTrackerTest, ImagesExpire) {
  // Add an image, then simulate commits to expire decodes.
  int frame_number = 0;
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Advance by 2 commits, the image should still be locked.
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  locked = false;
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Advance by 2 commits, our first image should be unlocked.
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  EXPECT_EQ(2u, image_controller()->num_locked_images());
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Advance by another 2 commits, our second image should release.
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
  decoded_image_tracker()->SetSyncTreeFrameNumber(frame_number++);
  EXPECT_EQ(0u, image_controller()->num_locked_images());
}

TEST_F(DecodedImageTrackerTest, ImagesTimeOut) {
  // Add an image, this will start a 4000ms timeout to release it.
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Advance by 2000ms, the image should still be locked.
  task_runner()->FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Add an image; this will not start a new timeout, as one is pending.
  locked = false;
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Advance by 2100ms; our first image should be released.
  task_runner()->FastForwardBy(base::Milliseconds(2100));
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  // Advance by another 4000ms, our second image should be released.
  task_runner()->FastForwardBy(base::Milliseconds(4000));
  EXPECT_EQ(0u, image_controller()->num_locked_images());
}

TEST_F(DecodedImageTrackerTest, ImageUsedInDraw) {
  TargetColorParams target_color_params;

  // Insert two images:
  bool locked = false;
  auto paint_image_1 = CreateDiscardablePaintImage(gfx::Size(1, 1));
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(paint_image_1, target_color_params),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  locked = false;
  auto paint_image_2 = CreateDiscardablePaintImage(gfx::Size(1, 1));
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(paint_image_2, target_color_params),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Create dummy draw images for each:
  DrawImage draw_image_1(paint_image_1, false, SkIRect::MakeWH(1, 1),
                         PaintFlags::FilterQuality::kHigh, SkM44(), 0,
                         target_color_params);
  DrawImage draw_image_2(paint_image_2, false, SkIRect::MakeWH(1, 1),
                         PaintFlags::FilterQuality::kHigh, SkM44(), 0,
                         target_color_params);

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
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
  locked = false;
  decoded_image_tracker()->QueueImageDecode(
      DrawImageForDecoding(CreateDiscardablePaintImage(gfx::Size(1, 1)),
                           TargetColorParams()),
      base::BindOnce([](bool* locked, bool success) { *locked = true; },
                     base::Unretained(&locked)),
      /*speculative*/ false);
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  // Unlock all images.
  decoded_image_tracker()->UnlockAllImages();
  EXPECT_EQ(0u, image_controller()->num_locked_images());
}

}  // namespace cc
