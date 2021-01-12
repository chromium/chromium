// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
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
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {
class MockAmbientBackendModelObserver : public AmbientBackendModelObserver {
 public:
  MockAmbientBackendModelObserver() = default;
  ~MockAmbientBackendModelObserver() override = default;

  // AmbientBackendModelObserver:
  MOCK_METHOD(void, OnImageAdded, (), (override));
  MOCK_METHOD(void, OnImagesReady, (), (override));
};

}  // namespace

class AmbientPhotoControllerTest : public AmbientAshTestBase {
 public:
  std::vector<int> GetSavedCacheIndices(bool backup = false) {
    std::vector<int> result;
    const auto& map = backup ? GetBackupCachedFiles() : GetCachedFiles();
    for (auto& it : map) {
      result.push_back(it.first);
    }
    return result;
  }

  const PhotoCacheEntry* GetCacheEntryAtIndex(int cache_index,
                                              bool backup = false) {
    const auto& files = backup ? GetBackupCachedFiles() : GetCachedFiles();
    auto it = files.find(cache_index);
    if (it == files.end())
      return nullptr;
    else
      return &(it->second);
  }

  void WriteCacheDataBlocking(int cache_index,
                              const std::string* image = nullptr,
                              const std::string* details = nullptr,
                              const std::string* related_image = nullptr) {
    base::RunLoop loop;
    photo_cache()->WriteFiles(/*cache_index=*/cache_index, /*image=*/image,
                              /*details=*/details,
                              /*related_image=*/related_image,
                              loop.QuitClosure());
    loop.Run();
  }
};

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
  // Start to refresh images. It will download two images immediately and write
  // them in |ambient_image_path|. It will also download one more image after
  // fast forward. It will also download the related images and not cache them.
  photo_controller()->StartScreenUpdate();
  FastForwardToNextImage();

  // Count number of writes to cache. There should be three cache writes during
  // this ambient mode session.
  auto file_paths = GetSavedCacheIndices();
  EXPECT_EQ(file_paths.size(), 3u);
}

// Test that image is save and will not be deleted when stopping ambient mode.
TEST_F(AmbientPhotoControllerTest, ShouldNotDeleteImagesOnDisk) {
  // Start to refresh images. It will download two images immediately and write
  // them in |ambient_image_path|. It will also download one more image after
  // fast forward. It will also download the related images and not cache them.
  photo_controller()->StartScreenUpdate();
  FastForwardToNextImage();

  EXPECT_EQ(GetSavedCacheIndices().size(), 3u);

  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  FastForwardToNextImage();

  EXPECT_EQ(GetSavedCacheIndices().size(), 3u);

  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());
}

// Test that image is read from disk when no more topics.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenNoMoreTopics) {
  FetchImage();
  FastForwardToNextImage();
  // Topics is empty. Will read from cache, which is empty.
  auto image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  std::string data("cached image");
  WriteCacheDataBlocking(/*cache_index=*/0, &data);

  // Reset variables in photo controller.
  photo_controller()->StopScreenUpdate();
  FetchImage();
  FastForwardToNextImage();
  image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image.IsNull());
}

// Test that will try 100 times to read image from disk when no more topics.
TEST_F(AmbientPhotoControllerTest,
       ShouldTry100TimesToReadCacheWhenNoMoreTopics) {
  FetchImage();
  FastForwardToNextImage();
  // Topics is empty. Will read from cache, which is empty.
  auto image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_TRUE(image.IsNull());

  // The initial file name to be read is 0. Save a file with 99.img to check
  // if it gets read for display.
  std::string data("cached image");
  WriteCacheDataBlocking(/*cache_index=*/99, &data);

  // Reset variables in photo controller.
  photo_controller()->StopScreenUpdate();
  FetchImage();
  FastForwardToNextImage();
  image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image.IsNull());
}

// Test that image is read from disk when image downloading failed.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenImageDownloadingFailed) {
  SetDownloadPhotoData("");
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed. Downloading should
  // fail. Will read from cache, which is empty.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  auto image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  std::string data("cached image");
  WriteCacheDataBlocking(/*cache_index=*/0, &data);

  // Reset variables in photo controller.
  photo_controller()->StopScreenUpdate();
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed. Downloading should
  // fail. Will read from cache.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image.IsNull());
}

// Test that image is read from disk when image decoding failed.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenImageDecodingFailed) {
  SetDecodePhotoImage(gfx::ImageSkia());
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed.
  // Downloading succeed and save the data to disk.
  // First decoding should fail. Will read from cache, and then succeed.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());
}

// Test that image will refresh when have more topics.
TEST_F(AmbientPhotoControllerTest, ShouldResumWhenHaveMoreTopics) {
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
}

TEST_F(AmbientPhotoControllerTest, ShouldDownloadBackupImagesWhenScheduled) {
  std::string expected_data = "backup data";
  SetBackupDownloadPhotoData(expected_data);

  photo_controller()->ScheduleFetchBackupImages();

  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  // Timer is running but download has not started yet.
  EXPECT_TRUE(GetSavedCacheIndices(/*backup=*/true).empty());
  task_environment()->FastForwardBy(kBackupPhotoRefreshDelay);

  // Timer should have stopped.
  EXPECT_FALSE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  // Should have been two cache writes to backup data.
  const auto& backup_data = GetBackupCachedFiles();
  EXPECT_EQ(backup_data.size(), 2u);
  EXPECT_TRUE(base::Contains(backup_data, 0));
  EXPECT_TRUE(base::Contains(backup_data, 1));
  for (const auto& i : backup_data) {
    EXPECT_EQ(*(i.second.image), expected_data);
    EXPECT_FALSE(i.second.details);
    EXPECT_FALSE(i.second.related_image);
  }
}

TEST_F(AmbientPhotoControllerTest, ShouldResetTimerWhenBackupImagesFail) {
  photo_controller()->ScheduleFetchBackupImages();

  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  // Simulate an error in DownloadToFile.
  ClearDownloadPhotoData();
  task_environment()->FastForwardBy(kBackupPhotoRefreshDelay);

  EXPECT_TRUE(GetBackupCachedFiles().empty());

  // Timer should have restarted.
  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());
}

TEST_F(AmbientPhotoControllerTest,
       ShouldStartDownloadBackupImagesOnAmbientModeStart) {
  photo_controller()->ScheduleFetchBackupImages();

  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  SetBackupDownloadPhotoData("image data");

  photo_controller()->StartScreenUpdate();

  // Download should have started immediately.
  EXPECT_FALSE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  task_environment()->RunUntilIdle();

  // Download has triggered and backup cache directory is created. Should be
  // two cache writes to backup cache.
  const auto& backup_data = GetBackupCachedFiles();
  EXPECT_EQ(backup_data.size(), 2u);
  EXPECT_TRUE(base::Contains(backup_data, 0));
  EXPECT_TRUE(base::Contains(backup_data, 1));
  for (const auto& i : backup_data) {
    EXPECT_EQ(*(i.second.image), "image data");
    EXPECT_FALSE(i.second.details);
    EXPECT_FALSE(i.second.related_image);
  }
}

TEST_F(AmbientPhotoControllerTest, ShouldNotLoadDuplicateImages) {
  testing::NiceMock<MockAmbientBackendModelObserver> mock_backend_observer;
  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_observer{&mock_backend_observer};

  scoped_observer.Add(photo_controller()->ambient_backend_model());

  // All images downloaded will be identical.
  SetDownloadPhotoData("image data");

  photo_controller()->StartScreenUpdate();
  // Run the clock so the first photo is loaded.
  FastForwardTiny();

  // Should contain hash of downloaded data.
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->IsHashDuplicate(
      base::SHA1HashString("image data")));
  // Only one image should have been loaded.
  EXPECT_FALSE(photo_controller()->ambient_backend_model()->ImagesReady());

  // Now expect a call because second image is loaded.
  EXPECT_CALL(mock_backend_observer, OnImagesReady).Times(1);
  SetDownloadPhotoData("image data 2");
  FastForwardToNextImage();

  // Second image should have been loaded.
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->IsHashDuplicate(
      base::SHA1HashString("image data 2")));
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->ImagesReady());
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
