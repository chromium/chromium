// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

using AmbientPhotoControllerTest = AmbientAshTestBase;

// Test that topics are downloaded when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldStartToDownloadTopics) {
  auto topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_TRUE(topics.empty());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_TRUE(topics.empty());

  FastForwardToNextImage();
  topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_FALSE(topics.empty());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_TRUE(topics.empty());
}

// Test that image is downloaded when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldStartToDownloadImages) {
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  FastForwardToNextImage();
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());
}

// Tests that photos are updated periodically when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldUpdatePhotoPeriodically) {
  PhotoWithDetails image1;
  PhotoWithDetails image2;
  PhotoWithDetails image3;

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  FastForwardToNextImage();
  image1 = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image1.IsNull());
  EXPECT_TRUE(image2.IsNull());

  FastForwardToNextImage();
  image2 = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image2.IsNull());
  EXPECT_FALSE(image1.photo.BackedBySameObjectAs(image2.photo));
  EXPECT_TRUE(image3.IsNull());

  FastForwardToNextImage();
  image3 = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image3.IsNull());
  EXPECT_FALSE(image1.photo.BackedBySameObjectAs(image3.photo));
  EXPECT_FALSE(image2.photo.BackedBySameObjectAs(image3.photo));

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
}

// Test that image is saved.
TEST_F(AmbientPhotoControllerTest, ShouldSaveImagesOnDisk) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  // Start to refresh images. It will download a test image and write it in
  // |ambient_image_path| in a delayed task.
  photo_controller()->StartScreenUpdate();
  FastForwardToNextImage();

  EXPECT_TRUE(base::PathExists(ambient_image_path));

  {
    // Count files and directories in root_path. There should only be one file
    // that was just created to save image files for this ambient mode session.
    base::FileEnumerator files(
        ambient_image_path, /*recursive=*/false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    int count = 0;
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      EXPECT_FALSE(files.GetInfo().IsDirectory());
      count++;
    }

    // Two image files and two attribution files.
    EXPECT_EQ(count, 4);
  }

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

// Test that image is save and will be deleted when stopping ambient mode.
TEST_F(AmbientPhotoControllerTest, ShouldNotDeleteImagesOnDisk) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  // Start to refresh images. It will download a test image and write it in
  // |ambient_image_path| in a delayed task.
  photo_controller()->StartScreenUpdate();
  FastForwardToNextImage();

  EXPECT_TRUE(base::PathExists(ambient_image_path));

  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  FastForwardToNextImage();

  EXPECT_TRUE(base::PathExists(ambient_image_path));
  EXPECT_FALSE(base::IsDirectoryEmpty(ambient_image_path));

  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  {
    // Count files and directories in root_path. There should only be one file
    // that was just created to save image files for this ambient mode session.
    base::FileEnumerator files(
        ambient_image_path, /*recursive=*/false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    int count = 0;
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      EXPECT_FALSE(files.GetInfo().IsDirectory());
      count++;
    }

    // Two image files and two attribution files.
    EXPECT_EQ(count, 4);
  }

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

// Test that image is read from disk when no more topics.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenNoMoreTopics) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  FetchImage();
  FastForwardToNextImage();
  // Topics is empty. Will read from cache, which is empty.
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  auto cached_image = ambient_image_path.Append("0.img");
  base::CreateDirectory(ambient_image_path);
  base::WriteFile(cached_image, "cached image");

  // Reset variables in photo controller.
  photo_controller()->StopScreenUpdate();
  FetchImage();
  FastForwardToNextImage();
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

// Test that will try 100 times to read image from disk when no more topics.
TEST_F(AmbientPhotoControllerTest,
       ShouldTry100TimesToReadCacheWhenNoMoreTopics) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  FetchImage();
  FastForwardToNextImage();
  // Topics is empty. Will read from cache, which is empty.
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  // The initial file name to be read is 0. Save a file with 99.img to check if
  // it gets read for display.
  auto cached_image = ambient_image_path.Append("99.img");
  base::CreateDirectory(ambient_image_path);
  base::WriteFile(cached_image, "cached image");

  // Reset variables in photo controller.
  photo_controller()->StopScreenUpdate();
  FetchImage();
  FastForwardToNextImage();
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

// Test that image is read from disk when image downloading failed.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenImageDownloadingFailed) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  SetUrlLoaderData(std::make_unique<std::string>());
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed. Downloading should
  // fail. Will read from cache, which is empty.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  auto cached_image = ambient_image_path.Append("0.img");
  base::CreateDirectory(ambient_image_path);
  base::WriteFile(cached_image, "cached image");

  // Reset variables in photo controller.
  photo_controller()->StopScreenUpdate();
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed. Downloading should
  // fail. Will read from cache.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

// Test that image is read from disk when image decoding failed.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenImageDecodingFailed) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  SeteImageDecoderImage(gfx::ImageSkia());
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed.
  // Downloading succeed and save the data to disk.
  // First decoding should fail. Will read from cache, and then succeed.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

// Test that image will refresh when have more topics.
TEST_F(AmbientPhotoControllerTest, ShouldResumWhenHaveMoreTopics) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);

  FetchImage();
  FastForwardToNextImage();
  // Topics is empty. Will read from cache, which is empty.
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed and refresh image.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Clean up.
  base::DeletePathRecursively(ambient_image_path);
}

TEST_F(AmbientPhotoControllerTest, ShouldStartToRefreshWeather) {
  auto* model = photo_controller()->ambient_backend_model();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_TRUE(model->weather_condition_icon().isNull());

  WeatherInfo info;
  info.show_celsius = true;
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  backend_controller()->SetWeatherInfo(info);

  // Start to refresh weather as screen update starts.
  photo_controller()->StartScreenUpdate();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(model->show_celsius());
  EXPECT_FALSE(model->weather_condition_icon().isNull());
  EXPECT_GT(info.temp_f, 0);

  // Refresh weather again after time passes.
  info.show_celsius = false;
  info.temp_f = -70.0f;
  backend_controller()->SetWeatherInfo(info);

  FastForwardToRefreshWeather();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_LT(info.temp_f, 0);
}

}  // namespace ash
