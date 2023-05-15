// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <array>
#include <memory>
#include <tuple>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "ash/ambient/test/ambient_topic_queue_test_delegate.h"
#include "ash/ambient/test/mock_ambient_backend_model_observer.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointwise;
using ::testing::SizeIs;

namespace {

bool AreBackedBySameImage(const PhotoWithDetails& topic_l,
                          const PhotoWithDetails& topic_r) {
  return !topic_l.photo.isNull() && !topic_r.photo.isNull() &&
         topic_l.photo.BackedBySameObjectAs(topic_r.photo);
}

MATCHER(BackedBySameImage, "") {
  return AreBackedBySameImage(std::get<0>(arg), std::get<1>(arg));
}

MATCHER_P(BackedBySameImageAs, photo_with_details, "") {
  return AreBackedBySameImage(arg, photo_with_details);
}

}  // namespace

class AmbientPhotoControllerTest : public AmbientAshTestBase {
 protected:
  void SetUp() override {
    AmbientAshTestBase::SetUp();
    // This is common to all AmbientPhotoConfigs and mimics real-world behavior:
    // When OnImagesReady() is called, the UI synchronously starts rendering.
    ON_CALL(images_ready_observer_, OnImagesReady)
        .WillByDefault(::testing::Invoke([this]() {
          photo_controller()->OnMarkerHit(
              AmbientPhotoConfig::Marker::kUiStartRendering);
        }));
    images_ready_observation_.Observe(
        photo_controller()->ambient_backend_model());
  }

  void TearDown() override {
    images_ready_observation_.Reset();
    AmbientAshTestBase::TearDown();
  }

  std::vector<int> GetSavedCacheIndices(bool backup = false) {
    std::vector<int> result;
    const auto& map = backup ? GetBackupCachedFiles() : GetCachedFiles();
    for (auto& it : map) {
      result.push_back(it.first);
    }
    return result;
  }

  const ::ambient::PhotoCacheEntry* GetCacheEntryAtIndex(int cache_index,
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
                              const std::string* related_image = nullptr,
                              const std::string* related_details = nullptr,
                              bool is_portrait = false) {
    ::ambient::PhotoCacheEntry cache_entry;
    cache_entry.mutable_primary_photo()->set_image(*image);

    if (details)
      cache_entry.mutable_primary_photo()->set_details(*details);

    cache_entry.mutable_primary_photo()->set_is_portrait(is_portrait);

    if (related_image) {
      cache_entry.mutable_related_photo()->set_image(*related_image);
      cache_entry.mutable_related_photo()->set_is_portrait(is_portrait);
    }

    if (related_details)
      cache_entry.mutable_related_photo()->set_details(*related_details);

    base::RunLoop loop;
    photo_cache()->WritePhotoCache(/*cache_index=*/cache_index, cache_entry,
                                   loop.QuitClosure());
    loop.Run();
  }

  void ScheduleFetchBackupImages() {
    photo_controller()->ScheduleFetchBackupImages();
  }

  void Init() {
    photo_controller()->Init(std::make_unique<AmbientTopicQueueTestDelegate>());
  }

  void RunUntilImagesReady() {
    if (photo_controller()->ambient_backend_model()->ImagesReady())
      return;

    static constexpr base::TimeDelta kTimeout = base::Seconds(3);
    base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, kTimeout);
    base::RunLoop loop;
    base::RepeatingClosure quit_closure = loop.QuitClosure();
    testing::NiceMock<MockAmbientBackendModelObserver> mock_backend_observer;
    base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
        scoped_observation{&mock_backend_observer};
    scoped_observation.Observe(photo_controller()->ambient_backend_model());
    ON_CALL(mock_backend_observer, OnImagesReady)
        .WillByDefault(
            ::testing::Invoke([quit_closure]() { quit_closure.Run(); }));
    loop.Run();
  }

  void RunUntilNextTopicsAdded(int num_expected_topics) {
    static constexpr base::TimeDelta kTimeout = base::Seconds(3);
    base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, kTimeout);
    base::RunLoop loop;
    base::RepeatingClosure quit_closure = loop.QuitClosure();
    int num_topics_added = 0;
    testing::NiceMock<MockAmbientBackendModelObserver> mock_backend_observer;
    base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
        scoped_observation{&mock_backend_observer};
    scoped_observation.Observe(photo_controller()->ambient_backend_model());
    ON_CALL(mock_backend_observer, OnImageAdded)
        .WillByDefault(::testing::Invoke(
            [quit_closure, num_expected_topics, &num_topics_added]() {
              ++num_topics_added;
              if (num_topics_added >= num_expected_topics)
                quit_closure.Run();
            }));
    loop.Run();
  }

  testing::NiceMock<MockAmbientBackendModelObserver> images_ready_observer_;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      images_ready_observation_{&images_ready_observer_};
};

// Has 2 positions in the animation for photos and 2 dynamic assets per
// position.
class AmbientPhotoControllerAnimationTest : public AmbientPhotoControllerTest {
 protected:
  void SetUp() override {
    AmbientPhotoControllerTest::SetUp();

    cc::SkottieResourceMetadataMap resource_metadata;
    std::array<std::string, 4> all_dynamic_asset_ids = {
        GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/2),
        GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/2)};
    for (const std::string& asset_id : all_dynamic_asset_ids) {
      CHECK(resource_metadata.RegisterAsset("test-path", "test-name", asset_id,
                                            /*size=*/absl::nullopt));
    }

    photo_controller()->ambient_backend_model()->SetPhotoConfig(
        CreateAmbientAnimationPhotoConfig(resource_metadata));
    CHECK_EQ(photo_config().GetNumDecodedTopicsToBuffer(), 4u);
    CHECK_EQ(photo_config().topic_set_size, 2u);
  }

  const AmbientPhotoConfig& photo_config() {
    return photo_controller()->ambient_backend_model()->photo_config();
  }
};

// No topics should be prepared at all; the screensaver doesn't have photos in
// it. AmbientPhotoController should be completely idle and
// AmbientBackendModel::ImagesReady() should be true immediately.
class AmbientPhotoControllerEmptyConfigTest
    : public AmbientPhotoControllerTest {
 protected:
  void SetUp() override {
    AmbientPhotoControllerTest::SetUp();
    photo_controller()->ambient_backend_model()->SetPhotoConfig(
        AmbientPhotoConfig());
  }
};

// Test that topics are downloaded when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldStartToDownloadTopics) {
  auto topics =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_TRUE(topics.empty());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  topics = photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_TRUE(topics.empty());

  RunUntilImagesReady();
  topics = photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_FALSE(topics.empty());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  topics = photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_TRUE(topics.empty());
}

// Test that image is downloaded when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldStartToDownloadImages) {
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_TRUE(image.IsNull());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_TRUE(image.IsNull());
}

// Tests that photos are updated when OnMarkerHit() is called.
TEST_F(AmbientPhotoControllerTest, OnMarkerHitShouldUpdatePhoto) {
  PhotoWithDetails image1;
  PhotoWithDetails image2;
  PhotoWithDetails image3;

  // Start to refresh images.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image1);
  EXPECT_FALSE(image1.IsNull());
  EXPECT_TRUE(image2.IsNull());

  photo_controller()->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image2);
  EXPECT_FALSE(image2.IsNull());
  EXPECT_FALSE(image1.photo.BackedBySameObjectAs(image2.photo));
  EXPECT_TRUE(image3.IsNull());

  photo_controller()->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image3);
  EXPECT_FALSE(image3.IsNull());
  EXPECT_FALSE(image1.photo.BackedBySameObjectAs(image3.photo));
  EXPECT_FALSE(image2.photo.BackedBySameObjectAs(image3.photo));

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
}

TEST_F(AmbientPhotoControllerTest,
       ShouldLoadSavedTopicsFromDiskWithoutInternet) {
  // Start ambient mode and run until ImagesReady(). At this point, the
  // controller should have saved 2 topics to disk.
  PhotoWithDetails image;
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  ASSERT_FALSE(image.IsNull());

  // Stop ambient mode. That should clear the decoded topics in the model but
  // not clear the saved topics on disk.
  photo_controller()->StopScreenUpdate();

  // Simulate internet connection down.
  backend_controller()->SetFetchScreenUpdateInfoResponseSize(
      /*num_topics_to_return=*/0);

  // Restart ambient mode, and it should load previously saved topics from disk.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_FALSE(image.IsNull());
}

// Tests that image details is correctly set.
TEST_F(AmbientPhotoControllerTest, ShouldSetDetailsCorrectly) {
  SetPhotoOrientation(/*portrait=*/true);
  // Start to refresh images.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_FALSE(image.IsNull());

  // Fake details defined in fake_ambient_backend_controller_impl.cc.
  EXPECT_EQ(image.details, "fake-photo-attribution");

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
}

// Test that image is saved.
TEST_F(AmbientPhotoControllerTest, ShouldSaveImagesOnDisk) {
  // Start to refresh images. It will download two images immediately and write
  // them in |ambient_image_path|. It will also download one more image after
  // OnMarkerHit(). It will also download the related images and not cache
  // them.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  photo_controller()->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);

  // Count number of writes to cache. There should be three cache writes during
  // this ambient mode session.
  auto file_paths = GetSavedCacheIndices();
  EXPECT_EQ(file_paths.size(), 3u);
}

// Test that image is save and will not be deleted when stopping ambient mode.
TEST_F(AmbientPhotoControllerTest, ShouldNotDeleteImagesOnDisk) {
  // Start to refresh images. It will download two images immediately and write
  // them in |ambient_image_path|. It will also download one more image after
  // OnMarkerHit(). It will also download the related images and not cache
  // them.
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  photo_controller()->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);

  EXPECT_EQ(GetSavedCacheIndices().size(), 3u);

  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  FastForwardByPhotoRefreshInterval();

  EXPECT_EQ(GetSavedCacheIndices().size(), 3u);

  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_TRUE(image.IsNull());
}

// Test that image is read from disk when no more topics.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenNoMoreTopics) {
  Init();
  FetchImage();
  FastForwardByPhotoRefreshInterval();
  // Topics is empty. Will read from cache, which is empty.
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  std::string data("cached image");
  WriteCacheDataBlocking(/*cache_index=*/0, &data);

  // Reset variables in photo controller.
  Init();
  FetchImage();
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_FALSE(image.IsNull());
}

// Test that will try 100 times to read image from disk when no more topics.
TEST_F(AmbientPhotoControllerTest,
       ShouldTry100TimesToReadCacheWhenNoMoreTopics) {
  Init();
  FetchImage();
  FastForwardByPhotoRefreshInterval();
  // Topics is empty. Will read from cache, which is empty.
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_TRUE(image.IsNull());

  // The initial file name to be read is 0. Save a file with index 99 to check
  // if it gets read for display.
  std::string data("cached image");
  WriteCacheDataBlocking(/*cache_index=*/99, &data);

  // Reset variables in photo controller.
  Init();
  FetchImage();
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_FALSE(image.IsNull());
}

// Test that image is read from disk when image downloading failed.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenImageDownloadingFailed) {
  SetDownloadPhotoData("");
  Init();
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed. Downloading should
  // fail. Will read from cache, which is empty.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  std::string data("cached image");
  WriteCacheDataBlocking(/*cache_index=*/0, &data);

  // Reset variables in photo controller.
  Init();
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed. Downloading should
  // fail. Will read from cache.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_FALSE(image.IsNull());
}

// Test that image details is read from disk.
TEST_F(AmbientPhotoControllerTest, ShouldPopulateDetailsWhenReadFromCache) {
  Init();
  FetchImage();
  FastForwardByPhotoRefreshInterval();
  // Topics is empty. Will read from cache, which is empty.
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_TRUE(image.IsNull());

  // Save a file to check if it gets read for display.
  std::string data("cached image");
  std::string details("image details");
  WriteCacheDataBlocking(/*cache_index=*/0, &data, &details);

  // Reset variables in photo controller.
  Init();
  FetchImage();
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/&image,
      /*next_image=*/nullptr);
  EXPECT_FALSE(image.IsNull());
  EXPECT_EQ(image.details, details);
}

// Test that image is read from disk when image decoding failed.
TEST_F(AmbientPhotoControllerTest, ShouldReadCacheWhenImageDecodingFailed) {
  Init();
  SetDecodePhotoImage(gfx::ImageSkia());
  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed.
  // Downloading succeed and save the data to disk.
  // First decoding should fail. Will read from cache, and then succeed.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_FALSE(image.IsNull());
}

// Test that image will refresh when have more topics.
TEST_F(AmbientPhotoControllerTest, ShouldResumWhenHaveMoreTopics) {
  Init();
  FetchImage();
  FastForwardByPhotoRefreshInterval();
  // Topics is empty. Will read from cache, which is empty.
  PhotoWithDetails image;
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_TRUE(image.IsNull());

  FetchTopics();
  // Forward a little bit time. FetchTopics() will succeed and refresh image.
  task_environment()->FastForwardBy(0.2 * kTopicFetchInterval);
  photo_controller()->ambient_backend_model()->GetCurrentAndNextImages(
      /*current_image=*/nullptr,
      /*next_image=*/&image);
  EXPECT_FALSE(image.IsNull());
}

TEST_F(AmbientPhotoControllerTest, ShouldDownloadBackupImagesWhenScheduled) {
  std::string expected_data = "backup data";
  SetBackupDownloadPhotoData(expected_data);

  ScheduleFetchBackupImages();

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
    EXPECT_EQ(i.second.primary_photo().image(), expected_data);
    EXPECT_TRUE(i.second.primary_photo().details().empty());
    EXPECT_TRUE(i.second.related_photo().image().empty());
    EXPECT_TRUE(i.second.related_photo().details().empty());
  }
}

TEST_F(AmbientPhotoControllerTest, ShouldResetTimerWhenBackupImagesFail) {
  ScheduleFetchBackupImages();

  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  // Simulate an error in DownloadToFile.
  SetBackupDownloadPhotoData("");
  task_environment()->FastForwardBy(kBackupPhotoRefreshDelay);

  EXPECT_TRUE(GetBackupCachedFiles().empty());

  // Timer should have restarted.
  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());
}

TEST_F(AmbientPhotoControllerTest,
       ShouldStartDownloadBackupImagesOnAmbientModeStart) {
  ScheduleFetchBackupImages();

  EXPECT_TRUE(
      photo_controller()->backup_photo_refresh_timer_for_testing().IsRunning());

  SetBackupDownloadPhotoData("image data");

  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());

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
    EXPECT_EQ(i.second.primary_photo().image(), "image data");
    EXPECT_TRUE(i.second.primary_photo().details().empty());
    EXPECT_TRUE(i.second.related_photo().image().empty());
    EXPECT_TRUE(i.second.related_photo().details().empty());
  }
}

TEST_F(AmbientPhotoControllerTest, UsesBackupCacheAfterPrimaryCacheCleared) {
  ScheduleFetchBackupImages();

  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  task_environment()->RunUntilIdle();

  photo_controller()->StopScreenUpdate();

  // At this point, both the primary and backup cache should be filled with
  // photos from the last "screen update". ClearCache() should only clear the
  // primary cache, leaving photos in the backup cache to use.
  ASSERT_FALSE(GetBackupCachedFiles().empty());
  ambient_controller()->ambient_photo_cache()->Clear();
  // Simulate an IMAX failure to leave the photo controller no choice but to
  // resort to the backup cache.
  backend_controller()->SetFetchScreenUpdateInfoResponseSize(0);

  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  // Running until OnImagesReady() ensures the backup photos were loaded and
  // ambient UI can successfully start.
  RunUntilImagesReady();
}

TEST_F(AmbientPhotoControllerTest, ShouldNotLoadDuplicateImages) {
  testing::NiceMock<MockAmbientBackendModelObserver> mock_backend_observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_observation{&mock_backend_observer};

  scoped_observation.Observe(photo_controller()->ambient_backend_model());

  // All images downloaded will be identical.
  SetDownloadPhotoData("image data");

  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilNextTopicsAdded(/*num_expected_topics=*/1);

  // Should contain hash of downloaded data.
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->IsHashDuplicate(
      base::SHA1HashString("image data")));
  // Only one image should have been loaded.
  EXPECT_FALSE(photo_controller()->ambient_backend_model()->ImagesReady());

  // Now expect a call because second image is loaded.
  EXPECT_CALL(mock_backend_observer, OnImagesReady).Times(1);
  SetDownloadPhotoData("image data 2");
  RunUntilImagesReady();

  // Second image should have been loaded.
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->IsHashDuplicate(
      base::SHA1HashString("image data 2")));
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->ImagesReady());
}

TEST_F(AmbientPhotoControllerTest, IsScreenUpdateActive) {
  ASSERT_FALSE(photo_controller()->IsScreenUpdateActive());
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  EXPECT_TRUE(photo_controller()->IsScreenUpdateActive());
  photo_controller()->StopScreenUpdate();
  EXPECT_FALSE(photo_controller()->IsScreenUpdateActive());
}

TEST_F(AmbientPhotoControllerAnimationTest, AnimationPreparesInitialTopicSet) {
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  EXPECT_THAT(photo_controller()->ambient_backend_model()->all_decoded_topics(),
              SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
}

TEST_F(AmbientPhotoControllerAnimationTest,
       AnimationRefreshesTopicSetEachCycle) {
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  // Animation starts rendering. This should trigger an image refresh.
  RunUntilImagesReady();
  base::circular_deque<PhotoWithDetails> old_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  RunUntilNextTopicsAdded(photo_config().topic_set_size);
  base::circular_deque<PhotoWithDetails> new_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_THAT(new_photos, SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
  ASSERT_THAT(old_photos.size(), Eq(new_photos.size()));

  // Verify that the new set actually has 2 new photos and 2 photos from the
  // old set.
  EXPECT_THAT(new_photos[0], BackedBySameImageAs(old_photos[2]));
  EXPECT_THAT(new_photos[1], BackedBySameImageAs(old_photos[3]));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[2]))));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[3]))));
  old_photos = new_photos;

  // Animation cycle ends and another image refresh starts.
  photo_controller()->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextTopicsAdded(photo_config().topic_set_size);
  new_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_THAT(new_photos, SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
  ASSERT_THAT(old_photos.size(), Eq(new_photos.size()));
  EXPECT_THAT(new_photos[0], BackedBySameImageAs(old_photos[2]));
  EXPECT_THAT(new_photos[1], BackedBySameImageAs(old_photos[3]));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[2]))));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[3]))));
}

TEST_F(AmbientPhotoControllerAnimationTest,
       StopsRefreshingImagesAfterTargetAmountBuffered) {
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  RunUntilNextTopicsAdded(photo_config().topic_set_size);

  // Fast forward time to make sure no more images are prepared after
  // |kNumDynamicAssets| has been added.
  base::circular_deque<PhotoWithDetails> old_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  task_environment()->FastForwardBy(base::Minutes(1));
  base::circular_deque<PhotoWithDetails> new_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_THAT(photo_controller()->ambient_backend_model()->all_decoded_topics(),
              SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
  EXPECT_THAT(new_photos, Pointwise(BackedBySameImage(), old_photos));
}

TEST_F(AmbientPhotoControllerAnimationTest,
       AnimationRefreshesAfterIncompleteTopicSet) {
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  base::circular_deque<PhotoWithDetails> old_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();

  RunUntilNextTopicsAdded(photo_config().topic_set_size / 2);
  base::circular_deque<PhotoWithDetails> new_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_THAT(new_photos, SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
  ASSERT_THAT(old_photos.size(), Eq(new_photos.size()));

  EXPECT_THAT(new_photos[0], BackedBySameImageAs(old_photos[1]));
  EXPECT_THAT(new_photos[1], BackedBySameImageAs(old_photos[2]));
  EXPECT_THAT(new_photos[2], BackedBySameImageAs(old_photos[3]));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[3]))));
  old_photos = new_photos;

  // Cycle ends when only half of target amount refreshed.
  photo_controller()->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  RunUntilNextTopicsAdded(photo_config().topic_set_size);
  // Fast forward time to make sure no more images are prepared after
  // |photo_config().topic_set_size| has been added.
  task_environment()->FastForwardBy(base::Minutes(1));
  new_photos =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_THAT(new_photos, SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
  ASSERT_THAT(old_photos.size(), Eq(new_photos.size()));
  EXPECT_THAT(new_photos[0], BackedBySameImageAs(old_photos[2]));
  EXPECT_THAT(new_photos[1], BackedBySameImageAs(old_photos[3]));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[2]))));
  EXPECT_THAT(old_photos, Not(Contains(BackedBySameImageAs(new_photos[3]))));
}

TEST_F(AmbientPhotoControllerAnimationTest,
       HandlesMarkerWhenInitialTopicSetIncomplete) {
  constexpr base::TimeDelta kPhotoDownloadDelay = base::Seconds(5);
  constexpr base::TimeDelta kTimeoutAfterFirstPhoto = base::Seconds(10);
  SetPhotoDownloadDelay(kPhotoDownloadDelay);
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  task_environment()->FastForwardBy(kPhotoDownloadDelay +
                                    kTimeoutAfterFirstPhoto);
  ASSERT_TRUE(photo_controller()->ambient_backend_model()->ImagesReady());
  ASSERT_THAT(photo_controller()->ambient_backend_model()->all_decoded_topics(),
              SizeIs(3));

  // UI starts rendering when only 3/4 of the initial topic set is prepared.
  // The controller should immediately start preparing another 2 topics (the
  // size of 1 topic set).
  task_environment()->FastForwardBy(kPhotoDownloadDelay * 2);
  ASSERT_THAT(photo_controller()->ambient_backend_model()->all_decoded_topics(),
              SizeIs(photo_config().GetNumDecodedTopicsToBuffer()));
  // Fast forward time to make sure no more images are prepared after
  // |photo_config().topic_set_size| has been added.
  base::circular_deque<PhotoWithDetails> photos_before =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  task_environment()->FastForwardBy(base::Minutes(1));
  base::circular_deque<PhotoWithDetails> photos_after =
      photo_controller()->ambient_backend_model()->all_decoded_topics();
  EXPECT_THAT(photos_after, Pointwise(BackedBySameImage(), photos_before));
}

TEST_F(AmbientPhotoControllerEmptyConfigTest, CallsOnImagesReadyImmediately) {
  photo_controller()->StartScreenUpdate(
      std::make_unique<AmbientTopicQueueTestDelegate>());
  RunUntilImagesReady();
  EXPECT_THAT(photo_controller()->ambient_backend_model()->all_decoded_topics(),
              IsEmpty());
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_TRUE(photo_controller()->ambient_backend_model()->ImagesReady());
  EXPECT_THAT(photo_controller()->ambient_backend_model()->all_decoded_topics(),
              IsEmpty());
}

}  // namespace ash
