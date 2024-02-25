// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/checker_image_tracker.h"

#include <memory>
#include <unordered_set>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/paint_image_matchers.h"
#include "cc/test/skia_common.h"
#include "cc/tiles/image_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

// 5MB max image cache size.
const size_t kMaxImageCacheSizeBytes = 5 * 1024 * 1024;

const int kCheckerableImageDimension = 512;
// This size will result in an image just over kMaxImageCacheSizeBytes.
const int kLargeNonCheckerableImageDimension = 1145;
const int kSmallNonCheckerableImageDimension = 16;

const TargetColorParams kDefaultTargetColorParams;

class TestImageController : public ImageController {
 public:
  // We can use the same thread for the image worker because all use of it in
  // the ImageController is over-ridden here.
  TestImageController()
      : ImageController(base::SingleThreadTaskRunner::GetCurrentDefault().get(),
                        base::SingleThreadTaskRunner::GetCurrentDefault()) {
    SetMaxImageCacheLimitBytesForTesting(kMaxImageCacheSizeBytes);
  }

  ~TestImageController() override { DCHECK_EQ(locked_images_.size(), 0U); }

  int num_of_locked_images() const { return locked_images_.size(); }
  const PaintImageIdFlatSet& decodes_requested() const {
    return decodes_requested_;
  }
  const std::vector<DrawImage>& decoded_images() const {
    return decoded_images_;
  }

  void UnlockImageDecode(ImageDecodeRequestId id) override {
    DCHECK_EQ(locked_images_.count(id), 1U);
    locked_images_.erase(id);
  }

  ImageDecodeRequestId QueueImageDecode(
      const DrawImage& image,
      ImageDecodedCallback callback) override {
    ImageDecodeRequestId request_id = next_image_request_id_++;

    decoded_images_.push_back(image);
    decodes_requested_.insert(image.paint_image().stable_id());
    locked_images_.insert(request_id);

    // Post the callback asynchronously to match the behaviour in
    // ImageController.
    worker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), request_id,
                                  ImageDecodeResult::SUCCESS));

    return request_id;
  }

 private:
  ImageDecodeRequestId next_image_request_id_ = 1U;
  std::unordered_set<ImageDecodeRequestId> locked_images_;
  PaintImageIdFlatSet decodes_requested_;
  std::vector<DrawImage> decoded_images_;
};

class CheckerImageTrackerTest : public testing::Test,
                                public CheckerImageTrackerClient {
 public:
  enum class ImageType {
    CHECKERABLE,
    SMALL_NON_CHECKERABLE,
    LARGE_NON_CHECKERABLE
  };

  void SetUpTracker(bool checker_images_enabled) {
    size_t size_to_checker = 512 * 1024;
    checker_image_tracker_ = std::make_unique<CheckerImageTracker>(
        &image_controller_, this, checker_images_enabled, size_to_checker);
    checker_image_tracker_->SetMaxDecodePriorityAllowed(
        CheckerImageTracker::DecodeType::kPreDecode);
  }

  void TearDown() override { checker_image_tracker_.reset(); }

  DrawImage CreateImage(
      ImageType image_type,
      PaintImage::AnimationType animation = PaintImage::AnimationType::kStatic,
      PaintImage::CompletionState completion =
          PaintImage::CompletionState::kDone,
      bool is_multipart = false) {
    int dimension = 0;
    switch (image_type) {
      case ImageType::CHECKERABLE:
        dimension = kCheckerableImageDimension;
        break;
      case ImageType::SMALL_NON_CHECKERABLE:
        dimension = kSmallNonCheckerableImageDimension;
        break;
      case ImageType::LARGE_NON_CHECKERABLE:
        dimension = kLargeNonCheckerableImageDimension;
        break;
    }

    auto generator = CreatePaintImageGenerator(gfx::Size(dimension, dimension));
    auto id = PaintImage::GetNextId();
    checker_image_tracker_->UpdateImageDecodingHints(
        {{id, PaintImage::DecodingMode::kAsync}});
    return DrawImage(PaintImageBuilder::WithDefault()
                         .set_id(id)
                         .set_paint_image_generator(std::move(generator))
                         .set_animation_type(animation)
                         .set_completion_state(completion)
                         .set_is_multipart(is_multipart)
                         .set_decoding_mode(PaintImage::DecodingMode::kAsync)
                         .TakePaintImage(),
                     false, SkIRect::MakeWH(dimension, dimension),
                     PaintFlags::FilterQuality::kNone, SkM44(),
                     PaintImage::kDefaultFrameIndex, kDefaultTargetColorParams);
  }

  bool ShouldCheckerImage(const DrawImage& draw_image, WhichTree tree) {
    return checker_image_tracker_->ShouldCheckerImage(draw_image, tree);
  }

  CheckerImageTracker::ImageDecodeQueue BuildImageDecodeQueue(
      std::vector<DrawImage> images,
      WhichTree tree) {
    CheckerImageTracker::ImageDecodeQueue decode_queue;
    for (const auto& image : images) {
      if (ShouldCheckerImage(image, tree))
        decode_queue.push_back(CheckerImageTracker::ImageDecodeRequest(
            image.paint_image(), CheckerImageTracker::DecodeType::kRaster));
    }
    return decode_queue;
  }

  // CheckerImageTrackerClient implementation.
  void NeedsInvalidationForCheckerImagedTiles() override {
    invalidation_request_pending_ = true;
  }

 protected:
  TestImageController image_controller_;
  std::unique_ptr<CheckerImageTracker> checker_image_tracker_;

  bool invalidation_request_pending_ = false;
};

TEST_F(CheckerImageTrackerTest, CheckerImagesDisabled) {
  // Ensures that the tracker doesn't filter any images for checkering if it is
  // disabled.
  SetUpTracker(false);

  PaintImageIdFlatSet checkered_images;
  DrawImage draw_image = CreateImage(ImageType::CHECKERABLE);
  EXPECT_FALSE(ShouldCheckerImage(draw_image, WhichTree::PENDING_TREE));
  EXPECT_EQ(image_controller_.num_of_locked_images(), 0);
}

TEST_F(CheckerImageTrackerTest, UpdatesImagesAtomically) {
  // Ensures that the tracker updates images atomically for each frame.
  SetUpTracker(true);

  DrawImage checkerable_image = CreateImage(ImageType::CHECKERABLE);
  DrawImage small_non_checkerable_image =
      CreateImage(ImageType::SMALL_NON_CHECKERABLE);
  DrawImage large_non_checkerable_image =
      CreateImage(ImageType::LARGE_NON_CHECKERABLE);
  CheckerImageTracker::ImageDecodeQueue image_decode_queue;

  // First request to filter images.
  std::vector<DrawImage> draw_images = {
      checkerable_image, small_non_checkerable_image,
      large_non_checkerable_image, checkerable_image};
  image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);

  ASSERT_EQ(2u, image_decode_queue.size());
  EXPECT_TRUE(checkerable_image.paint_image().IsSameForTesting(
      image_decode_queue[0].paint_image));
  EXPECT_TRUE(checkerable_image.paint_image().IsSameForTesting(
      image_decode_queue[1].paint_image));

  checker_image_tracker_->ScheduleImageDecodeQueue(image_decode_queue);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);

  // Run pending task to indicate completion of decode request to the tracker.
  // This should send an impl-side invalidation request to the client. The
  // images must remain locked until the sync tree to which the invalidations
  // are added is activated.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invalidation_request_pending_);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);

  // Continue checkering the image until the set of images to invalidate is
  // pulled.
  EXPECT_TRUE(ShouldCheckerImage(checkerable_image, WhichTree::PENDING_TREE));

  PaintImageIdFlatSet invalidated_images =
      checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  EXPECT_EQ(invalidated_images.size(), 1U);
  EXPECT_EQ(
      invalidated_images.count(checkerable_image.paint_image().stable_id()),
      1U);

  // Use the same set of draw images to ensure that they are not checkered on
  // the pending tree now.
  EXPECT_FALSE(ShouldCheckerImage(checkerable_image, WhichTree::PENDING_TREE));
  EXPECT_FALSE(
      ShouldCheckerImage(small_non_checkerable_image, WhichTree::PENDING_TREE));
  EXPECT_FALSE(
      ShouldCheckerImage(large_non_checkerable_image, WhichTree::PENDING_TREE));

  // Use this set to make the same request from the active tree, we should
  // continue checkering this image on the active tree until activation.
  EXPECT_TRUE(ShouldCheckerImage(checkerable_image, WhichTree::ACTIVE_TREE));
  EXPECT_FALSE(
      ShouldCheckerImage(small_non_checkerable_image, WhichTree::ACTIVE_TREE));
  EXPECT_FALSE(
      ShouldCheckerImage(large_non_checkerable_image, WhichTree::ACTIVE_TREE));

  // Activate the sync tree. The images should be unlocked upon activation.
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);
  checker_image_tracker_->DidActivateSyncTree();
}

TEST_F(CheckerImageTrackerTest, NoConsecutiveCheckeringForImage) {
  // Ensures that if an image is decoded and invalidated once, it is not
  // checkered again in subsequent frames.
  SetUpTracker(true);

  DrawImage checkerable_image = CreateImage(ImageType::CHECKERABLE);
  std::vector<DrawImage> draw_images = {checkerable_image};

  CheckerImageTracker::ImageDecodeQueue image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);
  checker_image_tracker_->ScheduleImageDecodeQueue(image_decode_queue);

  // Trigger decode completion, take images to invalidate and activate the sync
  // tree.
  base::RunLoop().RunUntilIdle();
  checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  checker_image_tracker_->DidActivateSyncTree();

  // Subsequent requests for this image should not be checkered.
  EXPECT_FALSE(ShouldCheckerImage(checkerable_image, WhichTree::PENDING_TREE));
}

TEST_F(CheckerImageTrackerTest,
       TracksCheckeredImagesSeperatelyInConsecutiveFrames) {
  // Ensures that the set of images being checkered on the pending tree, and the
  // active tree are tracked correctly.
  SetUpTracker(true);

  DrawImage checkerable_image1 = CreateImage(ImageType::CHECKERABLE);
  std::vector<DrawImage> draw_images;
  CheckerImageTracker::ImageDecodeQueue image_decode_queue;

  // First request to filter images on the pending and active tree.
  draw_images.push_back(checkerable_image1);
  image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);
  checker_image_tracker_->ScheduleImageDecodeQueue(image_decode_queue);

  // The image is also checkered on the active tree while a decode request is
  // pending.
  EXPECT_TRUE(ShouldCheckerImage(checkerable_image1, WhichTree::ACTIVE_TREE));

  // Trigger decode completion and take images to invalidate on the sync tree.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invalidation_request_pending_);
  PaintImageIdFlatSet invalidated_images =
      checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  EXPECT_EQ(invalidated_images.size(), 1U);
  EXPECT_EQ(
      invalidated_images.count(checkerable_image1.paint_image().stable_id()),
      1U);

  // Second request to filter the same image on the pending and active tree. It
  // should be checkered on the active tree, but not the pending tree.
  EXPECT_TRUE(ShouldCheckerImage(checkerable_image1, WhichTree::ACTIVE_TREE));
  EXPECT_FALSE(ShouldCheckerImage(checkerable_image1, WhichTree::PENDING_TREE));

  // New checkerable image on the pending tree.
  DrawImage checkerable_image2 = CreateImage(ImageType::CHECKERABLE);
  EXPECT_TRUE(ShouldCheckerImage(checkerable_image2, WhichTree::PENDING_TREE));

  // Activate the sync tree. The initial image should no longer be checkered on
  // the active tree.
  checker_image_tracker_->DidActivateSyncTree();
  EXPECT_FALSE(ShouldCheckerImage(checkerable_image1, WhichTree::ACTIVE_TREE));
}

TEST_F(CheckerImageTrackerTest, CancelsScheduledDecodes) {
  SetUpTracker(true);

  DrawImage checkerable_image1 = CreateImage(ImageType::CHECKERABLE);
  DrawImage checkerable_image2 = CreateImage(ImageType::CHECKERABLE);
  std::vector<DrawImage> draw_images = {checkerable_image1, checkerable_image2};

  CheckerImageTracker::ImageDecodeQueue image_decode_queue;
  image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 2U);
  checker_image_tracker_->ScheduleImageDecodeQueue(
      std::move(image_decode_queue));

  // Only the first image in the queue should have been decoded.
  EXPECT_EQ(image_controller_.decodes_requested().size(), 1U);
  EXPECT_EQ(image_controller_.decodes_requested().count(
                checkerable_image1.paint_image().stable_id()),
            1U);

  // Rebuild the queue before the tracker is notified of decode completion,
  // removing the second image and adding a new one.
  DrawImage checkerable_image3 = CreateImage(ImageType::CHECKERABLE);
  draw_images = {checkerable_image1, checkerable_image3};
  image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);

  // The queue has 2 decodes because we are still checkering on the first one.
  EXPECT_EQ(image_decode_queue.size(), 2U);
  checker_image_tracker_->ScheduleImageDecodeQueue(
      std::move(image_decode_queue));

  // We still have only one decode because the tracker keeps only one decode
  // pending at a time.
  EXPECT_EQ(image_controller_.decodes_requested().size(), 1U);
  EXPECT_EQ(image_controller_.decodes_requested().count(
                checkerable_image1.paint_image().stable_id()),
            1U);

  // Trigger completion for all decodes. Only 2 images should have been decoded
  // since the second image was cancelled.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(image_controller_.decodes_requested().size(), 2U);
  EXPECT_EQ(image_controller_.decodes_requested().count(
                checkerable_image3.paint_image().stable_id()),
            1U);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 2);
}

TEST_F(CheckerImageTrackerTest, ClearsTracker) {
  SetUpTracker(true);

  DrawImage checkerable_image = CreateImage(ImageType::CHECKERABLE);
  CheckerImageTracker::ImageDecodeQueue image_decode_queue =
      BuildImageDecodeQueue({checkerable_image}, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);
  checker_image_tracker_->ScheduleImageDecodeQueue(
      std::move(image_decode_queue));
  base::RunLoop().RunUntilIdle();
  checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();

  // The image is no longer checkered on the pending tree.
  image_decode_queue =
      BuildImageDecodeQueue({checkerable_image}, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 0U);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);

  // Clear the tracker without clearing the async decode tracking. This should
  // drop the decode but the image should not be checkered.
  bool can_clear_decode_policy_tracking = false;
  checker_image_tracker_->ClearTracker(can_clear_decode_policy_tracking);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 0);
  image_decode_queue =
      BuildImageDecodeQueue({checkerable_image}, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 0U);
  checker_image_tracker_->DidActivateSyncTree();

  // Now clear the decode tracking as well. The image will be re-checkered.
  can_clear_decode_policy_tracking = true;
  checker_image_tracker_->ClearTracker(can_clear_decode_policy_tracking);
  // Re-initialize the decoding hint state. The decode policy tracking should
  // only be done when all image state will be re-created, so is safe to purge.
  checker_image_tracker_->UpdateImageDecodingHints(
      {{checkerable_image.paint_image().stable_id(),
        PaintImage::DecodingMode::kAsync}});
  image_decode_queue =
      BuildImageDecodeQueue({checkerable_image}, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);

  // If an image had been decoded and tracker was cleared after it, we should
  // continue checkering it.
  DrawImage checkerable_image2 = CreateImage(ImageType::CHECKERABLE);
  image_decode_queue =
      BuildImageDecodeQueue({checkerable_image}, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);
  checker_image_tracker_->ScheduleImageDecodeQueue(
      std::move(image_decode_queue));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);
  can_clear_decode_policy_tracking = false;
  checker_image_tracker_->ClearTracker(can_clear_decode_policy_tracking);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 0);
  image_decode_queue =
      BuildImageDecodeQueue({checkerable_image}, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);
}

TEST_F(CheckerImageTrackerTest, CheckersOnlyStaticCompletedImages) {
  SetUpTracker(true);

  DrawImage static_image = CreateImage(ImageType::CHECKERABLE);
  DrawImage animated_image =
      CreateImage(ImageType::CHECKERABLE, PaintImage::AnimationType::kAnimated);
  DrawImage partial_image =
      CreateImage(ImageType::CHECKERABLE, PaintImage::AnimationType::kStatic,
                  PaintImage::CompletionState::kPartiallyDone);
  DrawImage video_image =
      CreateImage(ImageType::CHECKERABLE, PaintImage::AnimationType::kVideo);
  std::vector<DrawImage> draw_images = {static_image, animated_image,
                                        partial_image, video_image};

  CheckerImageTracker::ImageDecodeQueue image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(image_decode_queue.size(), 1U);
  EXPECT_TRUE(image_decode_queue[0].paint_image.IsSameForTesting(
      static_image.paint_image()));

  // Change the partial image to complete and try again. It should sstill not
  // be checkered.
  gfx::Size image_size = gfx::Size(partial_image.paint_image().width(),
                                   partial_image.paint_image().height());
  DrawImage completed_paint_image = DrawImage(
      PaintImageBuilder::WithDefault()
          .set_id(partial_image.paint_image().stable_id())
          .set_paint_image_generator(CreatePaintImageGenerator(image_size))
          .TakePaintImage(),
      false, SkIRect::MakeWH(image_size.width(), image_size.height()),
      PaintFlags::FilterQuality::kNone, SkM44(), PaintImage::kDefaultFrameIndex,
      kDefaultTargetColorParams);
  EXPECT_FALSE(
      ShouldCheckerImage(completed_paint_image, WhichTree::PENDING_TREE));
}

TEST_F(CheckerImageTrackerTest, DontCheckerDisallowedImages) {
  SetUpTracker(true);

  DrawImage image = CreateImage(ImageType::CHECKERABLE);
  EXPECT_TRUE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));
  checker_image_tracker_->DisallowCheckeringForImage(image.paint_image());
  // Since the tracker already saw the image, even disallowing it would still
  // ensure that we checker it until it's completed.
  EXPECT_TRUE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));

  // Reset the tracker.
  checker_image_tracker_->ClearTracker(true);
  // If we haven't seen the image and disallow it first, then it's not
  // checkerable anymore.
  checker_image_tracker_->DisallowCheckeringForImage(image.paint_image());
  EXPECT_FALSE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));
}

TEST_F(CheckerImageTrackerTest, ChoosesMaxScaleAndQuality) {
  SetUpTracker(true);

  DrawImage image = CreateImage(ImageType::CHECKERABLE);
  DrawImage scaled_image1(image, 0.5f, PaintImage::kDefaultFrameIndex,
                          TargetColorParams());
  DrawImage scaled_image2 =
      DrawImage(image.paint_image(), false, image.src_rect(),
                PaintFlags::FilterQuality::kHigh, SkM44::Scale(1.8f, 1.8f),
                PaintImage::kDefaultFrameIndex, kDefaultTargetColorParams);

  std::vector<DrawImage> draw_images = {scaled_image1, scaled_image2};
  CheckerImageTracker::ImageDecodeQueue image_decode_queue =
      BuildImageDecodeQueue(draw_images, WhichTree::PENDING_TREE);
  checker_image_tracker_->ScheduleImageDecodeQueue(image_decode_queue);
  EXPECT_EQ(image_controller_.decoded_images().size(), 1u);
  EXPECT_EQ(image_controller_.decoded_images()[0].scale(),
            SkSize::Make(1.8f, 1.8f));
  EXPECT_EQ(image_controller_.decoded_images()[0].filter_quality(),
            PaintFlags::FilterQuality::kHigh);
}

TEST_F(CheckerImageTrackerTest, DontCheckerMultiPartImages) {
  SetUpTracker(true);

  DrawImage image = CreateImage(ImageType::CHECKERABLE);
  EXPECT_FALSE(image.paint_image().is_multipart());
  DrawImage multi_part_image =
      CreateImage(ImageType::CHECKERABLE, PaintImage::AnimationType::kStatic,
                  PaintImage::CompletionState::kDone, true);
  EXPECT_TRUE(multi_part_image.paint_image().is_multipart());

  EXPECT_TRUE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));
  EXPECT_FALSE(ShouldCheckerImage(multi_part_image, WhichTree::PENDING_TREE));
}

TEST_F(CheckerImageTrackerTest, RespectsDecodePriority) {
  SetUpTracker(true);

  DrawImage image1 = CreateImage(ImageType::CHECKERABLE);
  DrawImage image2 = CreateImage(ImageType::CHECKERABLE);
  DrawImage image3 = CreateImage(ImageType::CHECKERABLE);
  DrawImage image4 = CreateImage(ImageType::CHECKERABLE);
  CheckerImageTracker::ImageDecodeQueue image_decode_queue =
      BuildImageDecodeQueue({image1, image2, image3, image4},
                            WhichTree::PENDING_TREE);

  // Mark the last 2 images as pre-decode.
  EXPECT_EQ(image_decode_queue.size(), 4u);
  image_decode_queue[2].type = CheckerImageTracker::DecodeType::kPreDecode;
  image_decode_queue[3].type = CheckerImageTracker::DecodeType::kPreDecode;

  // No decodes allowed. Nothing should be scheduled.
  EXPECT_EQ(image_controller_.decoded_images().size(), 0u);
  checker_image_tracker_->SetNoDecodesAllowed();
  checker_image_tracker_->ScheduleImageDecodeQueue(image_decode_queue);
  EXPECT_EQ(image_controller_.decoded_images().size(), 0u);

  // Raster decodes allowed. Only those should be scheduled.
  checker_image_tracker_->SetMaxDecodePriorityAllowed(
      CheckerImageTracker::DecodeType::kRaster);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(image_controller_.decoded_images(),
              ImagesAreSame({image1, image2}));

  // All decodes allowed. The complete queue should be flushed.
  checker_image_tracker_->SetMaxDecodePriorityAllowed(
      CheckerImageTracker::DecodeType::kPreDecode);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(image_controller_.decoded_images(),
              ImagesAreSame({image1, image2, image3, image4}));
}

TEST_F(CheckerImageTrackerTest, UseSrcRectForSize) {
  SetUpTracker(true);

  // Create an image with checkerable dimensions and subrect it. It should not
  // be checkered.
  DrawImage image = CreateImage(ImageType::CHECKERABLE);
  image =
      DrawImage(image.paint_image(), false, SkIRect::MakeWH(200, 200),
                image.filter_quality(), SkM44(), PaintImage::kDefaultFrameIndex,
                image.target_color_params());
  EXPECT_FALSE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));
}

TEST_F(CheckerImageTrackerTest, DisableForSoftwareRaster) {
  SetUpTracker(true);

  // Should checker when not disabled.
  checker_image_tracker_->set_force_disabled(false);
  DrawImage image1 = CreateImage(ImageType::CHECKERABLE);
  EXPECT_TRUE(ShouldCheckerImage(image1, WhichTree::PENDING_TREE));

  // Toggle disable. If we were already checkering this image, we need to
  // continue it.
  checker_image_tracker_->set_force_disabled(true);
  EXPECT_TRUE(ShouldCheckerImage(image1, WhichTree::PENDING_TREE));

  // New image should not be checkered while disabled.
  DrawImage image2 = CreateImage(ImageType::CHECKERABLE);
  EXPECT_FALSE(ShouldCheckerImage(image2, WhichTree::PENDING_TREE));
}

TEST_F(CheckerImageTrackerTest, DecodingModeHints) {
  SetUpTracker(true);

  base::flat_map<PaintImage::Id, PaintImage::DecodingMode> hints = {
      {1, PaintImage::DecodingMode::kUnspecified},
      {2, PaintImage::DecodingMode::kSync},
      {3, PaintImage::DecodingMode::kAsync}};
  checker_image_tracker_->UpdateImageDecodingHints(hints);

  EXPECT_EQ(PaintImage::DecodingMode::kUnspecified,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(1));
  EXPECT_EQ(PaintImage::DecodingMode::kSync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(2));
  EXPECT_EQ(PaintImage::DecodingMode::kAsync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(3));

  hints = {{1, PaintImage::DecodingMode::kAsync},
           {2, PaintImage::DecodingMode::kAsync},
           {3, PaintImage::DecodingMode::kAsync}};
  checker_image_tracker_->UpdateImageDecodingHints(hints);

  // The more conservative state should persist.
  EXPECT_EQ(PaintImage::DecodingMode::kUnspecified,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(1));
  EXPECT_EQ(PaintImage::DecodingMode::kSync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(2));
  EXPECT_EQ(PaintImage::DecodingMode::kAsync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(3));

  hints = {{1, PaintImage::DecodingMode::kUnspecified},
           {2, PaintImage::DecodingMode::kUnspecified},
           {3, PaintImage::DecodingMode::kUnspecified}};
  checker_image_tracker_->UpdateImageDecodingHints(hints);

  EXPECT_EQ(PaintImage::DecodingMode::kUnspecified,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(1));
  EXPECT_EQ(PaintImage::DecodingMode::kSync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(2));
  EXPECT_EQ(PaintImage::DecodingMode::kUnspecified,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(3));

  hints = {{1, PaintImage::DecodingMode::kSync}};
  checker_image_tracker_->UpdateImageDecodingHints(hints);

  EXPECT_EQ(PaintImage::DecodingMode::kSync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(1));
  EXPECT_EQ(PaintImage::DecodingMode::kSync,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(2));
  EXPECT_EQ(PaintImage::DecodingMode::kUnspecified,
            checker_image_tracker_->get_decoding_mode_hint_for_testing(3));
}

TEST_F(CheckerImageTrackerTest, UsageHintsMakeImagesSync) {
  SetUpTracker(true);

  DrawImage image = CreateImage(ImageType::CHECKERABLE);
  EXPECT_TRUE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));

  base::flat_map<PaintImage::Id, PaintImage::DecodingMode> hints = {
      {image.paint_image().stable_id(), PaintImage::DecodingMode::kSync}};
  checker_image_tracker_->UpdateImageDecodingHints(hints);

  auto invalidated = checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  ASSERT_EQ(invalidated.size(), 1u);
  EXPECT_EQ(*invalidated.begin(), image.paint_image().stable_id());

  EXPECT_FALSE(ShouldCheckerImage(image, WhichTree::PENDING_TREE));
  // We should still continue checkering on the active tree, since we want
  // atomic updates from the pending tree.
  EXPECT_TRUE(ShouldCheckerImage(image, WhichTree::ACTIVE_TREE));
}

}  // namespace
}  // namespace cc
