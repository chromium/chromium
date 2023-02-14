// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_images/local_image_search_provider.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

class ImageAnnotationWorkerTest : public testing::Test {
 protected:
  // testing::Test overrides:
  void SetUp() override {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    test_directory_ = temp_dir.GetPath();
    base::FilePath test_db = test_directory_.AppendASCII("test.db");
    annotation_worker_ =
        std::make_unique<ImageAnnotationWorker>(test_directory_);
    annotation_worker_->UseFakeAnnotatorForTests();
    storage_ = base::MakeRefCounted<AnnotationStorage>(
        std::move(test_db), /*histogram_tag=*/"test",
        /*current_version_number=*/2, /*annotation_worker=*/nullptr);
    bar_image_path_ = test_directory_.AppendASCII("bar.jpg");
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ImageAnnotationWorker> annotation_worker_;
  scoped_refptr<AnnotationStorage> storage_;
  base::FilePath test_directory_;
  base::FilePath bar_image_path_;
};

bool Matcher(std::vector<ImageInfo> arg,
             std::vector<ImageInfo> expected_images) {
  for (const auto& expect_image : expected_images) {
    for (const auto& test_image : arg) {
      if (test_image.path == expect_image.path &&
          test_image.annotations == expect_image.annotations &&
          test_image.last_modified == expect_image.last_modified) {
        return true;
      }
    }
  }
  return false;
}

MATCHER_P(OneOfImages, image1, "") {
  return Matcher(arg, {image1});
}

MATCHER_P4(OneOfImages, image1, image2, image3, image4, "") {
  return Matcher(arg, {image1, image2, image3, image4});
}

TEST_F(ImageAnnotationWorkerTest, MustProcessTheFolderAtInitTest) {
  storage_->InitializeAsync();
  task_environment_.RunUntilIdle();

  auto jpg_path = test_directory_.AppendASCII("bar.jpg");
  auto jpeg_path = test_directory_.AppendASCII("bar1.jpeg");
  auto png_path = test_directory_.AppendASCII("bar2.png");
  auto jng_path = test_directory_.AppendASCII("bar3.jng");
  auto tjng_path = test_directory_.AppendASCII("bar4.tjng");
  auto JPG_path = test_directory_.AppendASCII("bar5.JPG");

  auto image_time = base::Time::Now();
  base::WriteFile(jpg_path, "test");
  base::TouchFile(jpg_path, image_time, image_time);
  base::WriteFile(jpeg_path, "test");
  base::TouchFile(jpeg_path, image_time, image_time);
  base::WriteFile(png_path, "test");
  base::TouchFile(png_path, image_time, image_time);
  base::WriteFile(jng_path, "test");
  base::TouchFile(jng_path, image_time, image_time);
  base::WriteFile(tjng_path, "test");
  base::TouchFile(tjng_path, image_time, image_time);
  base::WriteFile(JPG_path, "test");
  base::TouchFile(JPG_path, image_time, image_time);

  annotation_worker_->Run(storage_);
  task_environment_.RunUntilIdle();

  ImageInfo jpg_image({"bar"}, jpg_path, image_time);
  ImageInfo jpeg_image({"bar1"}, jpeg_path, image_time);
  ImageInfo png_image({"bar2"}, png_path, image_time);
  ImageInfo JPG_image({"bar5"}, JPG_path, image_time);

  auto expect_all =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images,
                    OneOfImages(jpg_image, jpeg_image, png_image, JPG_image));
      });
  storage_->GetAllAnnotationsAsync(expect_all);

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustProcessOnNewFileTest) {
  storage_->InitializeAsync();
  annotation_worker_->Run(storage_);
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, "test");
  auto bar_image_time = base::Time::Now();
  base::TouchFile(bar_image_path_, bar_image_time, bar_image_time);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"bar"}, bar_image_path_, bar_image_time);
  auto expect_one =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(bar_image));
      });
  storage_->GetAllAnnotationsAsync(expect_one);

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustUpdateOnFileUpdateTest) {
  storage_->InitializeAsync();
  annotation_worker_->Run(storage_);
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, "test");

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, "test123");
  auto bar_image_time_updated = base::Time::Now();
  base::TouchFile(bar_image_path_, bar_image_time_updated,
                  bar_image_time_updated);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  ImageInfo bar_image_updated({"bar"}, bar_image_path_, bar_image_time_updated);
  auto expect_updated =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(bar_image_updated));
      });
  storage_->GetAllAnnotationsAsync(expect_updated);

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustRemoveOnFileDeleteTest) {
  storage_->InitializeAsync();
  annotation_worker_->Run(storage_);
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, "test");

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  base::DeleteFile(bar_image_path_);
  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  auto expect_empty = base::BindLambdaForTesting(
      [=](std::vector<ImageInfo> images) { EXPECT_TRUE(images.empty()); });
  storage_->GetAllAnnotationsAsync(expect_empty);

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace app_list
