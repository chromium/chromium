// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_backup_photo_downloader.h"

#include <utility>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/ambient_photo_cache_settings.h"
#include "ash/ambient/test/test_ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/test/ash_test_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_status_code.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kMaxPixelDeviation = 1;

constexpr char kTestUrl[] = "https://test.com";

gfx::Image ReadImageFromCache(int cache_index) {
  base::test::TestFuture<::ambient::PhotoCacheEntry> read_cache_future;
  ambient_photo_cache::ReadPhotoCache(ambient_photo_cache::Store::kBackup,
                                      cache_index,
                                      read_cache_future.GetCallback());
  base::test::TestFuture<const gfx::ImageSkia&> decode_image_future;
  image_util::DecodeImageData(decode_image_future.GetCallback(),
                              data_decoder::mojom::ImageCodec::kDefault,
                              read_cache_future.Get().primary_photo().image());
  return gfx::Image(decode_image_future.Get());
}

class AmbientBackupPhotoDownloaderTest : public ::testing::Test {
 protected:
  AmbientBackupPhotoDownloaderTest() : ambient_client_(&wake_lock_provider_) {
    ambient_photo_cache::SetFileTaskRunner(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }

  // ::testing::Test
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ambient_client_.SetAutomaticalyIssueToken(true);
    SetAmbientBackupPhotoCacheRootDirForTesting(temp_dir_.GetPath());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  InProcessDataDecoder decoder_;
  device::TestWakeLockProvider wake_lock_provider_;
  TestAmbientClient ambient_client_;
  AmbientAccessTokenController token_controller_;
};

TEST_F(AmbientBackupPhotoDownloaderTest, Success) {
  gfx::ImageSkia test_image;
  ambient_client_.test_url_loader_factory().AddResponse(
      kTestUrl, CreateEncodedImageForTesting(
                    gfx::Size(100, 100), SK_ColorBLACK,
                    data_decoder::mojom::ImageCodec::kDefault, &test_image));
  base::test::TestFuture<bool> downloader_future;
  AmbientBackupPhotoDownloader downloader(token_controller_, 0,
                                          gfx::Size(100, 100), kTestUrl,
                                          downloader_future.GetCallback());
  ASSERT_TRUE(downloader_future.Get());
  EXPECT_TRUE(gfx::test::AreImagesClose(
      ReadImageFromCache(0), gfx::Image(test_image), kMaxPixelDeviation));
}

TEST_F(AmbientBackupPhotoDownloaderTest, SuccessWithResizeSameAspectRatio) {
  gfx::ImageSkia test_image;
  ambient_client_.test_url_loader_factory().AddResponse(
      kTestUrl, CreateEncodedImageForTesting(
                    gfx::Size(100, 100), SK_ColorGREEN,
                    data_decoder::mojom::ImageCodec::kDefault, &test_image));
  base::test::TestFuture<bool> downloader_future;
  AmbientBackupPhotoDownloader downloader(token_controller_, 0,
                                          gfx::Size(50, 50), kTestUrl,
                                          downloader_future.GetCallback());
  ASSERT_TRUE(downloader_future.Get());
  gfx::ImageSkia expected_image =
      CreateSolidColorTestImage(gfx::Size(50, 50), SK_ColorGREEN);
  EXPECT_TRUE(gfx::test::AreImagesClose(
      ReadImageFromCache(0), gfx::Image(expected_image), kMaxPixelDeviation));
}

TEST_F(AmbientBackupPhotoDownloaderTest, SuccessWithResizeLandscapeToPortrait) {
  gfx::ImageSkia test_image;
  ambient_client_.test_url_loader_factory().AddResponse(
      kTestUrl, CreateEncodedImageForTesting(
                    gfx::Size(200, 100), SK_ColorGREEN,
                    data_decoder::mojom::ImageCodec::kDefault, &test_image));
  base::test::TestFuture<bool> downloader_future;
  AmbientBackupPhotoDownloader downloader(token_controller_, 0,
                                          gfx::Size(25, 50), kTestUrl,
                                          downloader_future.GetCallback());
  ASSERT_TRUE(downloader_future.Get());
  gfx::ImageSkia expected_image =
      CreateSolidColorTestImage(gfx::Size(100, 50), SK_ColorGREEN);
  EXPECT_TRUE(gfx::test::AreImagesClose(
      ReadImageFromCache(0), gfx::Image(expected_image), kMaxPixelDeviation));
}

TEST_F(AmbientBackupPhotoDownloaderTest, SuccessWithResizePortraitToLandscape) {
  gfx::ImageSkia test_image;
  ambient_client_.test_url_loader_factory().AddResponse(
      kTestUrl, CreateEncodedImageForTesting(
                    gfx::Size(100, 200), SK_ColorGREEN,
                    data_decoder::mojom::ImageCodec::kDefault, &test_image));
  base::test::TestFuture<bool> downloader_future;
  AmbientBackupPhotoDownloader downloader(token_controller_, 0,
                                          gfx::Size(50, 25), kTestUrl,
                                          downloader_future.GetCallback());
  ASSERT_TRUE(downloader_future.Get());
  gfx::ImageSkia expected_image =
      CreateSolidColorTestImage(gfx::Size(50, 100), SK_ColorGREEN);
  EXPECT_TRUE(gfx::test::AreImagesClose(
      ReadImageFromCache(0), gfx::Image(expected_image), kMaxPixelDeviation));
}

TEST_F(AmbientBackupPhotoDownloaderTest, FailedServerResponse) {
  ambient_client_.test_url_loader_factory().AddResponse(kTestUrl, "",
                                                        net::HTTP_BAD_REQUEST);
  base::test::TestFuture<bool> downloader_future;
  AmbientBackupPhotoDownloader downloader(token_controller_, 0,
                                          gfx::Size(100, 100), kTestUrl,
                                          downloader_future.GetCallback());
  EXPECT_FALSE(downloader_future.Get());
}

}  // namespace
}  // namespace ash
