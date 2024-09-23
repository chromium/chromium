// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_test_util.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

constexpr uint8_t kBad_image[] = {
    0x40,
    0x22,
    0x23,
    0x25,
};

constexpr uint8_t kJpeg_image[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10,
                                   0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
                                   0x01, 0x00, 0x00, 0x01};

constexpr uint8_t kPng_image[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A,
                                  0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
                                  0x49, 0x48, 0x44, 0x52};

constexpr uint8_t kWebp_image[] = {
    0x52, 0x49, 0x46, 0x46, 0x6c, 0x32, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00};

constexpr uint8_t kWebp_image1[] = {
    0x52, 0x49, 0x46, 0x46, 0x74, 0x69, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x20, 0x68, 0x69, 0x00, 0x00, 0x90, 0x35, 0x03, 0x9d};

constexpr uint8_t kWebp_animation[] = {
    0x52, 0x49, 0x46, 0x46, 0xa6, 0x2f, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00};

class ImageAnnotationWorkerTest : public testing::Test {
 protected:
  // testing::Test overrides:
  void SetUp() override {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    test_directory_ = temp_dir.GetPath();
    std::vector<base::FilePath> excluded_paths = {
        test_directory_.AppendASCII("TrashBin")};
    annotation_worker_ = std::make_unique<ImageAnnotationWorker>(
        test_directory_, std::move(excluded_paths),
        /*profile=*/nullptr,
        /*use_file_watchers=*/false,
        /*use_ocr=*/false,
        /*use_ica=*/false);
    annotation_worker_->set_image_processing_delay_for_testing(
        base::Seconds(0));
    bar_image_path_ = test_directory_.AppendASCII("bar.jpg");
    const base::FilePath test_db = test_directory_.AppendASCII("test.db");
    storage_ =
        std::make_unique<AnnotationStorage>(std::move(test_db),
                                            /*annotation_worker=*/nullptr);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ImageAnnotationWorker> annotation_worker_;
  std::unique_ptr<AnnotationStorage> storage_;
  base::FilePath test_directory_;
  base::FilePath bar_image_path_;
};

TEST_F(ImageAnnotationWorkerTest, MustProcessTheFolderAtInitTest) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  base::CreateDirectory(test_directory_.AppendASCII("Images"));
  base::CreateDirectory(test_directory_.AppendASCII("TrashBin"));

  auto jpg_path = test_directory_.AppendASCII("bar.jpg");
  base::WriteFile(jpg_path, kJpeg_image);
  auto jpeg_path =
      test_directory_.AppendASCII("Images").AppendASCII("bar1.jpeg");
  base::WriteFile(jpeg_path, kJpeg_image);
  auto png_path = test_directory_.AppendASCII("bar2.png");
  base::WriteFile(png_path, kPng_image);
  auto jng_path = test_directory_.AppendASCII("bar3.jng");
  base::WriteFile(jng_path, kJpeg_image);
  auto tjng_path = test_directory_.AppendASCII("bar4.tjng");
  base::WriteFile(tjng_path, kJpeg_image);
  auto JPG_path = test_directory_.AppendASCII("bar5.JPG");
  base::WriteFile(JPG_path, kJpeg_image);
  auto webp_path = test_directory_.AppendASCII("bar6.webp");
  base::WriteFile(webp_path, kWebp_image);
  auto WEBP_path = test_directory_.AppendASCII("bar7.WEBP");
  base::WriteFile(WEBP_path, kWebp_image1);
  auto bin_path =
      test_directory_.AppendASCII("TrashBin").AppendASCII("bar8.jpg");
  base::WriteFile(bin_path, kJpeg_image);

  auto image_time = base::Time::Now();
  for (const auto& path : {jpg_path, jpeg_path, png_path, jng_path, tjng_path,
                           JPG_path, webp_path, WEBP_path, bin_path}) {
    base::TouchFile(path, image_time, image_time);
  }

  annotation_worker_->Initialize(storage_.get());
  task_environment_.FastForwardBy(base::Seconds(1));
  task_environment_.RunUntilIdle();

  ImageInfo jpg_image({"bar"}, jpg_path, image_time, /*file_size=*/16);
  ImageInfo jpeg_image({"bar1"}, jpeg_path, image_time, 16);
  ImageInfo png_image({"bar2"}, png_path, image_time, 16);
  ImageInfo JPG_image({"bar5"}, JPG_path, image_time, 16);
  ImageInfo webp_image({"bar6"}, webp_path, image_time, 24);
  ImageInfo WEBP_image({"bar7"}, WEBP_path, image_time, 24);

  auto annotations = storage_->GetAllAnnotationsForTest();
  EXPECT_THAT(annotations, testing::UnorderedElementsAreArray(
                               {jpg_image, jpeg_image, png_image, JPG_image,
                                webp_image, WEBP_image}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, ImageProcessingTimeout) {
  // Overwrite processing delay to be greater than `kMaxImageProcessingTime`. It
  // simulate the case when the dlc processing is slow and callback is return
  // after timeout.
  annotation_worker_->set_image_processing_delay_for_testing(base::Minutes(3));

  storage_->Initialize();
  task_environment_.RunUntilIdle();

  base::CreateDirectory(test_directory_.AppendASCII("Images"));
  base::CreateDirectory(test_directory_.AppendASCII("TrashBin"));

  auto jpg_path = test_directory_.AppendASCII("bar.jpg");
  base::WriteFile(jpg_path, kJpeg_image);
  auto jpeg_path =
      test_directory_.AppendASCII("Images").AppendASCII("bar1.jpeg");
  base::WriteFile(jpeg_path, kJpeg_image);
  auto png_path = test_directory_.AppendASCII("bar2.png");
  base::WriteFile(png_path, kPng_image);
  auto jng_path = test_directory_.AppendASCII("bar3.jng");
  base::WriteFile(jng_path, kJpeg_image);
  auto tjng_path = test_directory_.AppendASCII("bar4.tjng");
  base::WriteFile(tjng_path, kJpeg_image);
  auto JPG_path = test_directory_.AppendASCII("bar5.JPG");
  base::WriteFile(JPG_path, kJpeg_image);
  auto webp_path = test_directory_.AppendASCII("bar6.webp");
  base::WriteFile(webp_path, kWebp_image);
  auto WEBP_path = test_directory_.AppendASCII("bar7.WEBP");
  base::WriteFile(WEBP_path, kWebp_image1);
  auto bin_path =
      test_directory_.AppendASCII("TrashBin").AppendASCII("bar8.jpg");
  base::WriteFile(bin_path, kJpeg_image);

  auto image_time = base::Time::Now();
  for (const auto& path : {jpg_path, jpeg_path, png_path, jng_path, tjng_path,
                           JPG_path, webp_path, WEBP_path, bin_path}) {
    base::TouchFile(path, image_time, image_time);
  }

  annotation_worker_->Initialize(storage_.get());
  // We need `FastForwardUntilNoTasksRemain` here as `RunUntilIdle` excludes
  // delayed tasks.
  task_environment_.FastForwardUntilNoTasksRemain();

  ImageInfo jpg_image({"bar"}, jpg_path, image_time, /*file_size=*/16);
  ImageInfo jpeg_image({"bar1"}, jpeg_path, image_time, 16);
  ImageInfo png_image({"bar2"}, png_path, image_time, 16);
  ImageInfo JPG_image({"bar5"}, JPG_path, image_time, 16);
  ImageInfo webp_image({"bar6"}, webp_path, image_time, 24);
  ImageInfo WEBP_image({"bar7"}, WEBP_path, image_time, 24);

  auto annotations = storage_->GetAllAnnotationsForTest();
  EXPECT_THAT(annotations, testing::UnorderedElementsAreArray(
                               {jpg_image, jpeg_image, png_image, JPG_image,
                                webp_image, WEBP_image}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustIgnoreBadFiles) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  auto image_time = base::Time::Now();

  auto webp_animation_path = test_directory_.AppendASCII("bar.webp");
  base::WriteFile(webp_animation_path, kWebp_animation);

  auto bad_image_path = test_directory_.AppendASCII("bar.png");
  base::WriteFile(bad_image_path, kBad_image);

  auto jng_path = test_directory_.AppendASCII("bar.jng");
  base::WriteFile(jng_path, kJpeg_image);

  auto txt_path = test_directory_.AppendASCII("bar.txt");
  base::WriteFile(jng_path, kJpeg_image);

  for (const auto& path :
       {webp_animation_path, bad_image_path, jng_path, txt_path}) {
    base::TouchFile(path, image_time, image_time);
    annotation_worker_->TriggerOnFileChangeForTests(path,
                                                    /*error=*/false);
  }

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(storage_->GetAllAnnotationsForTest().empty());
  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustProcessOnNewFileTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, kJpeg_image);
  auto bar_image_time = base::Time::Now();
  base::TouchFile(bar_image_path_, bar_image_time, bar_image_time);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"bar"}, bar_image_path_, bar_image_time,
                      /*file_size=*/16);

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustUpdateOnFileUpdateTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, kJpeg_image);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, kJpeg_image);
  auto bar_image_time_updated = base::Time::Now();
  base::TouchFile(bar_image_path_, bar_image_time_updated,
                  bar_image_time_updated);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  ImageInfo bar_image_updated({"bar"}, bar_image_path_, bar_image_time_updated,
                              /*file_size=*/16);
  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image_updated}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustRemoveOnFileDeleteTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, kJpeg_image);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  base::DeleteFile(bar_image_path_);
  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(storage_->GetAllAnnotationsForTest().empty());

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, GetAllFilesTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(storage_->GetAllFiles().empty());

  base::WriteFile(bar_image_path_, kJpeg_image);
  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(storage_->GetAllFiles(),
              testing::ElementsAreArray({bar_image_path_}));

  base::DeleteFile(bar_image_path_);
  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(storage_->GetAllFiles().empty());

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, ProcessDirectoryTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  auto test_images = test_directory_.AppendASCII("Test Images");
  auto test_images1 = test_directory_.AppendASCII("Test1Images");
  auto new_folder = test_images.AppendASCII("New Folder");
  auto new_folder1 = test_images1.AppendASCII("New Folder");
  base::CreateDirectory(test_images);
  base::CreateDirectory(new_folder);

  auto jpg_path = test_directory_.AppendASCII("bar.jpg");
  base::WriteFile(jpg_path, kJpeg_image);
  auto jpeg_path = test_images.AppendASCII("bar1.jpeg");
  auto jpeg_path1 = test_images1.AppendASCII("bar1.jpeg");
  base::WriteFile(jpeg_path, kJpeg_image);
  auto png_path = new_folder.AppendASCII("bar2.png");
  auto png_path1 = new_folder1.AppendASCII("bar2.png");
  base::WriteFile(png_path, kPng_image);

  auto image_time = base::Time::Now();
  for (const auto& path : {jpg_path, jpeg_path, png_path}) {
    base::TouchFile(path, image_time, image_time);
    annotation_worker_->TriggerOnFileChangeForTests(path,
                                                    /*error=*/false);
  }

  task_environment_.RunUntilIdle();

  ImageInfo jpg_image({"bar"}, jpg_path, image_time, /*file_size=*/16);
  ImageInfo jpeg_image({"bar1"}, jpeg_path, image_time, 16);
  ImageInfo jpeg_image1({"bar1"}, jpeg_path1, image_time, 16);
  ImageInfo png_image({"bar2"}, png_path, image_time, 16);
  ImageInfo png_image1({"bar2"}, png_path1, image_time, 16);

  EXPECT_THAT(
      storage_->GetAllAnnotationsForTest(),
      testing::UnorderedElementsAreArray({jpg_image, jpeg_image, png_image}));

  task_environment_.RunUntilIdle();

  base::Move(test_images, test_images1);
  annotation_worker_->TriggerOnFileChangeForTests(test_images,
                                                  /*error=*/false);
  annotation_worker_->TriggerOnFileChangeForTests(test_images1,
                                                  /*error=*/false);

  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      storage_->GetAllAnnotationsForTest(),
      testing::UnorderedElementsAreArray({jpg_image, jpeg_image1, png_image1}));

  base::DeletePathRecursively(test_images1);
  annotation_worker_->TriggerOnFileChangeForTests(test_images1,
                                                  /*error=*/false);

  task_environment_.RunUntilIdle();

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::UnorderedElementsAreArray({jpg_image}));
}

TEST_F(ImageAnnotationWorkerTest, IgnoreWhenLimitReachedTest) {
  // Overwrite the indexing limit to 0, so that no indexing is allowed.
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["indexing_limit"] = "0";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      search_features::kLauncherImageSearchIndexingLimit, params);
  // Re-construct the annotation worker with the new param.
  std::vector<base::FilePath> excluded_paths = {
      test_directory_.AppendASCII("TrashBin")};
  annotation_worker_ = std::make_unique<ImageAnnotationWorker>(
      test_directory_, std::move(excluded_paths), /*profile=*/nullptr,
      /*use_file_watchers=*/false,
      /*use_ocr=*/false,
      /*use_ica=*/false);
  annotation_worker_->set_image_processing_delay_for_testing(base::Seconds(0));

  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, kJpeg_image);
  auto bar_image_time = base::Time::Now();
  base::TouchFile(bar_image_path_, bar_image_time, bar_image_time);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"bar"}, bar_image_path_, bar_image_time,
                      /*file_size=*/16);

  EXPECT_TRUE(storage_->GetAllAnnotationsForTest().empty());

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace app_list
