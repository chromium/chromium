// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_images/local_image_search_provider.h"
#include "chrome/browser/ash/app_list/search/local_images/local_image_search_test_util.h"
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
    annotation_worker_ = std::make_unique<ImageAnnotationWorker>(
        test_directory_, /*use_ocr=*/false, /*use_ica=*/false);
    bar_image_path_ = test_directory_.AppendASCII("bar.jpg");
    const base::FilePath test_db = test_directory_.AppendASCII("test.db");
    storage_ = std::make_unique<AnnotationStorage>(
        std::move(test_db), /*histogram_tag=*/"test",
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

  auto jpg_path = test_directory_.AppendASCII("bar.jpg");
  auto jpeg_path = test_directory_.AppendASCII("bar1.jpeg");
  auto png_path = test_directory_.AppendASCII("bar2.png");
  auto jng_path = test_directory_.AppendASCII("bar3.jng");
  auto tjng_path = test_directory_.AppendASCII("bar4.tjng");
  auto JPG_path = test_directory_.AppendASCII("bar5.JPG");

  auto image_time = base::Time::Now();
  for (const auto& path :
       {jpg_path, jpeg_path, png_path, jng_path, tjng_path, JPG_path}) {
    base::WriteFile(path, "test");
    base::TouchFile(path, image_time, image_time);
  }

  annotation_worker_->Initialize(storage_.get());
  task_environment_.FastForwardBy(base::Seconds(1));
  task_environment_.RunUntilIdle();

  ImageInfo jpg_image({"bar"}, jpg_path, image_time, /*is_ignored=*/false);
  ImageInfo jpeg_image({"bar1"}, jpeg_path, image_time, /*is_ignored=*/false);
  ImageInfo png_image({"bar2"}, png_path, image_time, /*is_ignored=*/false);
  ImageInfo JPG_image({"bar5"}, JPG_path, image_time, /*is_ignored=*/false);

  auto annotations = storage_->GetAllAnnotations();
  EXPECT_THAT(annotations, testing::UnorderedElementsAreArray(
                               {jpg_image, jpeg_image, png_image, JPG_image}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustProcessOnNewFileTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, "test");
  auto bar_image_time = base::Time::Now();
  base::TouchFile(bar_image_path_, bar_image_time, bar_image_time);

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"bar"}, bar_image_path_, bar_image_time,
                      /*is_ignored=*/false);

  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::ElementsAreArray({bar_image}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustUpdateOnFileUpdateTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
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

  ImageInfo bar_image_updated({"bar"}, bar_image_path_, bar_image_time_updated,
                              /*is_ignored=*/false);
  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::ElementsAreArray({bar_image_updated}));

  task_environment_.RunUntilIdle();
}

TEST_F(ImageAnnotationWorkerTest, MustRemoveOnFileDeleteTest) {
  storage_->Initialize();
  annotation_worker_->Initialize(storage_.get());
  task_environment_.RunUntilIdle();

  base::WriteFile(bar_image_path_, "test");

  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  base::DeleteFile(bar_image_path_);
  annotation_worker_->TriggerOnFileChangeForTests(bar_image_path_,
                                                  /*error=*/false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(storage_->GetAllAnnotations().empty());

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace app_list
