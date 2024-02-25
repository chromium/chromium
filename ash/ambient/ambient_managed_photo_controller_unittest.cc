// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_managed_photo_controller.h"

#include <memory>

#include "ash/ambient/metrics/managed_screensaver_metrics.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/test/mock_ambient_backend_model_observer.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
using gfx::test::AreImagesEqual;
using ::testing::IsEmpty;
using ::testing::Not;

namespace {

bool AreBackedBySameImage(const PhotoWithDetails& topic_l,
                          const PhotoWithDetails& topic_r) {
  return !topic_l.photo.isNull() && !topic_r.photo.isNull() &&
         topic_l.photo.BackedBySameObjectAs(topic_r.photo);
}

MATCHER_P(BackedBySameImageAs, photo_with_details, "") {
  return AreBackedBySameImage(arg, photo_with_details);
}

// This limit is specified in the policy definition for the policies
// ScreensaverLockScreenImages and DeviceScreensaverLoginScreenImages.
constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

}  // namespace

class AmbientManagedPhotoControllerTest : public AmbientAshTestBase {
 public:
  AmbientManagedPhotoControllerTest() {
    CreateTestData();

    // Required as otherwise the PathService::CheckedGet fails in the
    // screensaver images policy handler.
    device_policy_screensaver_folder_override_ =
        std::make_unique<base::ScopedPathOverride>(
            ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA, temp_dir_.GetPath());
  }

  void CreateTestData() {
    bool success = temp_dir_.CreateUniqueTempDir();
    ASSERT_TRUE(success);
    base::FilePath image_1 =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("IMAGE_1.jpg"));
    CreateTestImageJpegFile(image_1, 4, 4, SK_ColorRED);
    base::FilePath image_2 =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("IMAGE_2.jpg"));
    CreateTestImageJpegFile(image_2, 8, 8, SK_ColorGREEN);
    base::FilePath image_3 =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("IMAGE_3.jpg"));
    CreateTestImageJpegFile(image_3, 8, 4, SK_ColorBLUE);
    base::FilePath image_4 =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("IMAGE_4.jpg"));
    CreateTestImageJpegFile(image_4, 4, 8, SK_ColorBLACK);
    image_file_paths_.push_back(image_1);
    image_file_paths_.push_back(image_2);
    image_file_paths_.push_back(image_3);
    image_file_paths_.push_back(image_4);
  }

  void CleanUpTestData() { image_file_paths_.clear(); }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kAmbientModeManagedScreensaver);

    AmbientAshTestBase::SetUp();
    photo_controller_ = std::make_unique<AmbientManagedPhotoController>(
        *ambient_controller()->ambient_view_delegate(),
        CreateAmbientManagedSlideshowPhotoConfig());
  }

  void TearDown() override {
    StopScreenUpdate();
    // Call reset before calling tear down to make sure we aren't observing
    // already freed resources
    photo_controller_.reset();
    AmbientAshTestBase::TearDown();
    CleanUpTestData();
  }

  std::vector<base::FilePath> GetImageFilePaths() { return image_file_paths_; }

  void RunUntilImagesReady() {
    if (managed_photo_controller()->ambient_backend_model()->ImagesReady()) {
      return;
    }

    base::test::TestFuture<void> future;
    testing::NiceMock<MockAmbientBackendModelObserver> mock_backend_observer;
    base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
        scoped_observation{&mock_backend_observer};
    scoped_observation.Observe(
        managed_photo_controller()->ambient_backend_model());
    ON_CALL(mock_backend_observer, OnImagesReady)
        .WillByDefault(::testing::Invoke([&future]() { future.SetValue(); }));
    ASSERT_TRUE(future.Wait()) << "Timed out waiting for OnImagesReady";
  }

  void RunUntilNextImagesAdded(size_t expected_topics) {
    size_t num_topics_added = 0;
    base::test::TestFuture<void> future;
    testing::NiceMock<MockAmbientBackendModelObserver> mock_backend_observer;
    base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
        scoped_observation{&mock_backend_observer};
    scoped_observation.Observe(
        managed_photo_controller()->ambient_backend_model());
    ON_CALL(mock_backend_observer, OnImageAdded)
        .WillByDefault(
            ::testing::Invoke([&future, &num_topics_added, &expected_topics]() {
              num_topics_added++;
              if (expected_topics == num_topics_added) {
                future.SetValue();
              }
            }));
    ASSERT_TRUE(future.Wait()) << "Timed out waiting for OnImageAdded";
  }

  void StopScreenUpdate() {
    managed_photo_controller()->StopScreenUpdate();
    EXPECT_FALSE(managed_photo_controller()->IsScreenUpdateActive());
    EXPECT_THAT(managed_photo_controller()
                    ->ambient_backend_model()
                    ->all_decoded_topics(),
                IsEmpty());
  }

  void StartScreenUpdate() {
    managed_photo_controller()->StartScreenUpdate();
    EXPECT_TRUE(managed_photo_controller()->IsScreenUpdateActive());
  }

  AmbientManagedPhotoController* managed_photo_controller() {
    return photo_controller_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  InProcessDataDecoder decoder_;
  std::vector<base::FilePath> image_file_paths_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride>
      device_policy_screensaver_folder_override_;
  std::unique_ptr<AmbientManagedPhotoController> photo_controller_;
};

TEST_F(AmbientManagedPhotoControllerTest,
       NoImagesAreShownIfThereAreNoImagesAvailable) {
  StartScreenUpdate();
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(
      managed_photo_controller()->ambient_backend_model()->ImagesReady());
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
}

TEST_F(AmbientManagedPhotoControllerTest,
       WhenImagesAreSetBackendModelHasImages) {
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
  managed_photo_controller()->UpdateImageFilePaths(GetImageFilePaths());
  StartScreenUpdate();
  RunUntilImagesReady();
  EXPECT_TRUE(
      managed_photo_controller()->ambient_backend_model()->ImagesReady());
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));
}

TEST_F(AmbientManagedPhotoControllerTest,
       WhenImagesAreSetAfterStartingUpdatedBackendModelHasImages) {
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
  StartScreenUpdate();
  EXPECT_FALSE(
      managed_photo_controller()->ambient_backend_model()->ImagesReady());
  managed_photo_controller()->UpdateImageFilePaths(GetImageFilePaths());
  RunUntilImagesReady();

  EXPECT_TRUE(
      managed_photo_controller()->ambient_backend_model()->ImagesReady());
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));
}

TEST_F(AmbientManagedPhotoControllerTest,
       UICycleEndMarkerTransitionsToTheNextImage) {
  managed_photo_controller()->UpdateImageFilePaths(GetImageFilePaths());
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
  StartScreenUpdate();
  RunUntilImagesReady();

  PhotoWithDetails next_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      nullptr, &next_image);
  managed_photo_controller()->OnMarkerHit(
      AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextImagesAdded(/*expected_topics=*/1);
  PhotoWithDetails current_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &current_image, nullptr);
  EXPECT_THAT(current_image, BackedBySameImageAs(next_image));
}

TEST_F(AmbientManagedPhotoControllerTest,
       UIStartRenderingMarkerDoesNotTransitionImages) {
  managed_photo_controller()->UpdateImageFilePaths(GetImageFilePaths());
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
  StartScreenUpdate();
  RunUntilImagesReady();
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));

  PhotoWithDetails old_current_image, next_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &old_current_image, &next_image);
  managed_photo_controller()->OnMarkerHit(
      AmbientPhotoConfig::Marker::kUiStartRendering);

  PhotoWithDetails current_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &current_image, nullptr);

  EXPECT_THAT(current_image, Not(BackedBySameImageAs(next_image)));
  EXPECT_THAT(current_image, BackedBySameImageAs(old_current_image));
}

TEST_F(AmbientManagedPhotoControllerTest, FirstImageIsLoadedAfterLastImage) {
  const std::vector<base::FilePath>& image_file_paths = GetImageFilePaths();
  managed_photo_controller()->UpdateImageFilePaths(
      {image_file_paths[0], image_file_paths[1]});
  StartScreenUpdate();
  RunUntilImagesReady();
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));

  PhotoWithDetails first_image, second_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &first_image, &second_image);
  EXPECT_FALSE(AreImagesEqual(gfx::Image(first_image.photo),
                              gfx::Image(second_image.photo)));
  managed_photo_controller()->OnMarkerHit(
      AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextImagesAdded(/*expected_topics=*/1);

  PhotoWithDetails third_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      nullptr, &third_image);

  // The third image will either be the 2nd or the 1st image in the backend
  // model the reason for this is that the order isn't guaranteed when we load
  // the first 2 images in parallel (so it can either be [1,2] or [2,1]).
  EXPECT_TRUE(AreImagesEqual(gfx::Image(first_image.photo),
                             gfx::Image(third_image.photo)) ||
              AreImagesEqual(gfx::Image(second_image.photo),
                             gfx::Image(third_image.photo)));
}

TEST_F(AmbientManagedPhotoControllerTest,
       UpdatingImagesDuringDisplayUpdatesThem) {
  const std::vector<base::FilePath>& image_file_paths = GetImageFilePaths();
  managed_photo_controller()->UpdateImageFilePaths(
      {image_file_paths[0], image_file_paths[1]});
  StartScreenUpdate();
  RunUntilImagesReady();
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));

  PhotoWithDetails first_image, second_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &first_image, &second_image);

  managed_photo_controller()->UpdateImageFilePaths(
      {image_file_paths[2], image_file_paths[3]});
  // Wait for the next images
  RunUntilNextImagesAdded(/*expected_topics=*/2);

  PhotoWithDetails third_image, fourth_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &third_image, &fourth_image);

  // The new images are different from the old images.
  EXPECT_FALSE(AreImagesEqual(gfx::Image(first_image.photo),
                              gfx::Image(third_image.photo)));
  EXPECT_FALSE(AreImagesEqual(gfx::Image(first_image.photo),
                              gfx::Image(fourth_image.photo)));
  EXPECT_FALSE(AreImagesEqual(gfx::Image(second_image.photo),
                              gfx::Image(third_image.photo)));
  EXPECT_FALSE(AreImagesEqual(gfx::Image(second_image.photo),
                              gfx::Image(fourth_image.photo)));
}

TEST_F(AmbientManagedPhotoControllerTest, CallingStartScreenAgainIsANoOp) {
  managed_photo_controller()->UpdateImageFilePaths(GetImageFilePaths());
  StartScreenUpdate();
  RunUntilImagesReady();
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));
  PhotoWithDetails first_image, second_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &first_image, &second_image);
  managed_photo_controller()->StartScreenUpdate();
  task_environment()->FastForwardBy(base::Minutes(1));

  PhotoWithDetails after_update_first_image, after_update_second_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &after_update_first_image, &after_update_second_image);
  // Note: No change happens, and we still have the same image instances at the
  // same positions in the buffer.
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      Not(IsEmpty()));
  EXPECT_THAT(first_image, BackedBySameImageAs(after_update_first_image));
  EXPECT_THAT(second_image, BackedBySameImageAs(after_update_second_image));
}

TEST_F(AmbientManagedPhotoControllerTest, InvalidFileTest) {
  managed_photo_controller()->UpdateImageFilePaths(
      {base::FilePath(FILE_PATH_LITERAL("invalid_path_1")),
       base::FilePath(FILE_PATH_LITERAL("invalid_path_2"))});
  StartScreenUpdate();
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_THAT(
      managed_photo_controller()->ambient_backend_model()->all_decoded_topics(),
      IsEmpty());
}

TEST_F(AmbientManagedPhotoControllerTest, ValidFileNotLoadedTwice) {
  const std::vector<base::FilePath>& image_file_paths = GetImageFilePaths();
  managed_photo_controller()->UpdateImageFilePaths({
      base::FilePath(FILE_PATH_LITERAL("invalid_path_1")),
      image_file_paths[0],
      base::FilePath(FILE_PATH_LITERAL("invalid_path_2")),
  });
  StartScreenUpdate();
  RunUntilNextImagesAdded(/*expected_topics=*/1);
  task_environment()->FastForwardBy(base::Minutes(1));

  EXPECT_EQ(managed_photo_controller()
                ->ambient_backend_model()
                ->all_decoded_topics()
                .size(),
            1u);

  // Case: Marker hit when max tries exceeded.
  managed_photo_controller()->OnMarkerHit(
      AmbientPhotoConfig::Marker::kUiCycleEnded);
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_EQ(managed_photo_controller()
                ->ambient_backend_model()
                ->all_decoded_topics()
                .size(),
            1u);

  // Case: Updating image file paths resets retry limit
  managed_photo_controller()->UpdateImageFilePaths(
      {image_file_paths[0], image_file_paths[1]});
  RunUntilNextImagesAdded(/*expected_topics=*/2);

  EXPECT_EQ(managed_photo_controller()
                ->ambient_backend_model()
                ->all_decoded_topics()
                .size(),
            2u);
}

TEST_F(AmbientManagedPhotoControllerTest, InvalidAndValidFileTest) {
  const std::vector<base::FilePath>& image_file_paths = GetImageFilePaths();
  managed_photo_controller()->UpdateImageFilePaths(
      {image_file_paths[0], base::FilePath(FILE_PATH_LITERAL("invalid_path_1")),
       base::FilePath(FILE_PATH_LITERAL("invalid_path_2")),
       base::FilePath(FILE_PATH_LITERAL("invalid_path_3")),
       image_file_paths[1]});
  StartScreenUpdate();
  RunUntilImagesReady();
  EXPECT_EQ(managed_photo_controller()
                ->ambient_backend_model()
                ->all_decoded_topics()
                .size(),
            2u);

  PhotoWithDetails first_image, second_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &first_image, &second_image);
  EXPECT_FALSE(AreImagesEqual(gfx::Image(first_image.photo),
                              gfx::Image(second_image.photo)));
  // Case: Marker hit in a mix of valid and invalid files.
  managed_photo_controller()->OnMarkerHit(
      AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextImagesAdded(/*expected_topics=*/1);
  PhotoWithDetails third_image, fourth_image;
  managed_photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      &third_image, &fourth_image);
  EXPECT_FALSE(AreImagesEqual(gfx::Image(third_image.photo),
                              gfx::Image(fourth_image.photo)));
}

TEST_F(AmbientManagedPhotoControllerTest, PhotoConfigTest) {
  const AmbientPhotoConfig& config =
      managed_photo_controller()->ambient_backend_model()->photo_config();
  EXPECT_EQ(2u, config.GetNumDecodedTopicsToBuffer());
  EXPECT_TRUE(config.should_split_topics);
  EXPECT_EQ(1u, config.refresh_topic_markers.size());
  EXPECT_TRUE(config.refresh_topic_markers.contains(
      AmbientPhotoConfig::Marker::kUiCycleEnded));
}

TEST_F(AmbientManagedPhotoControllerTest, AddingEmptyImagesIsANoOP) {
  managed_photo_controller()->UpdateImageFilePaths(GetImageFilePaths());
  StartScreenUpdate();
  RunUntilImagesReady();
  managed_photo_controller()->UpdateImageFilePaths({});
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_EQ(managed_photo_controller()
                ->ambient_backend_model()
                ->all_decoded_topics()
                .size(),
            2u);
}

TEST_F(AmbientManagedPhotoControllerTest, VerifyImageCountHistogram) {
  base::HistogramTester histogram_tester;
  std::vector<base::FilePath> images;

  // Update image list to empty list
  managed_photo_controller()->UpdateImageFilePaths(images);

  const std::string& histogram_name =
      GetManagedScreensaverHistogram(kManagedScreensaverImageCountUMA);

  // Update list to max - 1
  for (unsigned int i = 0; i < kMaxUrlsToProcessFromPolicy - 1; ++i) {
    images.emplace_back(FILE_PATH_LITERAL("IMAGE_1.jpg"));
  }
  managed_photo_controller()->UpdateImageFilePaths(images);

  // Update list to max
  images.emplace_back(FILE_PATH_LITERAL("IMAGE_1.jpg"));
  managed_photo_controller()->UpdateImageFilePaths(images);

  // Update list to max + 1
  images.emplace_back(FILE_PATH_LITERAL("IMAGE_1.jpg"));
  managed_photo_controller()->UpdateImageFilePaths(images);

  histogram_tester.ExpectTotalCount(histogram_name, 4);

  histogram_tester.ExpectBucketCount(histogram_name,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectBucketCount(histogram_name,
                                     /*sample=*/kMaxUrlsToProcessFromPolicy - 1,
                                     /*expected_count=*/1);

  histogram_tester.ExpectBucketCount(histogram_name,
                                     /*sample=*/kMaxUrlsToProcessFromPolicy + 1,
                                     /*expected_count=*/1);

  histogram_tester.ExpectBucketCount(histogram_name,
                                     /*sample=*/kMaxUrlsToProcessFromPolicy + 1,
                                     /*expected_count=*/1);
}

}  // namespace ash
