// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <string>
#include <vector>

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

const std::string kUser1 = "user1@test.com";
const AccountId kAccountId1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

SkBitmap CreateBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseARGB(255, 31, 63, 127);
  return bitmap;
}

std::string CreateJpgBytes() {
  SkBitmap bitmap = CreateBitmap();
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

}  // namespace

class SeaPenWallpaperManagerTest : public AshTestBase {
 public:
  SeaPenWallpaperManagerTest()
      : wallpaper_file_manager_(std::make_unique<WallpaperFileManager>()),
        sea_pen_wallpaper_manager_(
            SeaPenWallpaperManager(wallpaper_file_manager_.get())) {}

  SeaPenWallpaperManagerTest(const SeaPenWallpaperManagerTest&) = delete;
  SeaPenWallpaperManagerTest& operator=(const SeaPenWallpaperManagerTest&) =
      delete;

  ~SeaPenWallpaperManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    sea_pen_wallpaper_manager_.SetStorageDirectory(GetTempFileDirectory());
  }

  void TearDown() override { AshTestBase::TearDown(); }

  base::FilePath GetTempFileDirectory() { return scoped_temp_dir_.GetPath(); }

  std::vector<base::FilePath> GetJpgFilesForAccountId(
      const AccountId& account_id) {
    const auto target_directory =
        GetTempFileDirectory().Append(account_id.GetAccountIdKey());
    base::FileEnumerator enumerator(target_directory, /*recursive=*/true,
                                    base::FileEnumerator::FILES, "*.jpg");

    std::vector<base::FilePath> result;
    for (auto path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      result.push_back(path);
    }
    return result;
  }

  SeaPenWallpaperManager& sea_pen_wallpaper_manager() {
    return sea_pen_wallpaper_manager_;
  }

  WallpaperFileManager* wallpaper_file_manager() {
    return wallpaper_file_manager_.get();
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  InProcessDataDecoder in_process_data_decoder_;
  const std::unique_ptr<WallpaperFileManager> wallpaper_file_manager_;
  SeaPenWallpaperManager sea_pen_wallpaper_manager_;
};

TEST_F(SeaPenWallpaperManagerTest, DecodesImageAndReturnsId) {
  constexpr uint32_t image_id = 111;
  base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
  const base::FilePath file_path =
      sea_pen_wallpaper_manager().GetFilePathForImageId(kAccountId1, image_id);
  ASSERT_FALSE(base::PathExists(file_path));
  sea_pen_wallpaper_manager().DecodeAndSaveSeaPenImage(
      kAccountId1, {CreateJpgBytes(), image_id},
      personalization_app::mojom::SeaPenQuery::NewTextQuery("search query"),
      decode_sea_pen_image_future.GetCallback());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(SeaPenWallpaperManagerTest, StoresTenImages) {
  // Create 10 images in the temp directory.
  for (uint32_t i = 1; i <= 10; i++) {
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager().DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        decode_sea_pen_image_future.GetCallback());

    // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
    // slightly.
    EXPECT_TRUE(gfx::test::AreBitmapsClose(
        CreateBitmap(),
        *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
        /*max_deviation=*/1));

    const auto file_path =
        sea_pen_wallpaper_manager().GetFilePathForImageId(kAccountId1, i);
    EXPECT_TRUE(base::PathExists(file_path));
  }

  EXPECT_THAT(
      GetIdsFromFilePaths(GetJpgFilesForAccountId(kAccountId1)),
      testing::UnorderedElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u));
}

TEST_F(SeaPenWallpaperManagerTest, EleventhImageReplacesOldest) {
  // Create 10 images in the temp directory.
  for (uint32_t i = 1; i <= 10; i++) {
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager().DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        decode_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(decode_sea_pen_image_future.Wait());
  }

  constexpr uint32_t oldest_image_id = 5;
  // Mark image 5 as the oldest by last modified time.
  ASSERT_TRUE(
      base::TouchFile(sea_pen_wallpaper_manager().GetFilePathForImageId(
                          kAccountId1, oldest_image_id),
                      /*last_accessed=*/base::Time::Now(),
                      /*last_modified=*/base::Time::Now() - base::Minutes(30)));

  constexpr uint32_t new_image_id = 11;

  ASSERT_FALSE(
      base::PathExists(sea_pen_wallpaper_manager().GetFilePathForImageId(
          kAccountId1, new_image_id)));

  // Decode and save the 11th sea pen image.
  base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
  sea_pen_wallpaper_manager().DecodeAndSaveSeaPenImage(
      kAccountId1, {CreateJpgBytes(), new_image_id},
      personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
      decode_sea_pen_image_future.GetCallback());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));

  // The last modified image should be deleted when the 11th image is added.
  EXPECT_THAT(GetIdsFromFilePaths(GetJpgFilesForAccountId(kAccountId1)),
              testing::UnorderedElementsAre(1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u, 10u,
                                            new_image_id));
  EXPECT_FALSE(
      base::PathExists(sea_pen_wallpaper_manager().GetFilePathForImageId(
          kAccountId1, oldest_image_id)));
  EXPECT_TRUE(
      base::PathExists(sea_pen_wallpaper_manager().GetFilePathForImageId(
          kAccountId1, new_image_id)));
}

TEST_F(SeaPenWallpaperManagerTest, GetFilePathForImageId) {
  EXPECT_EQ(
      GetTempFileDirectory()
          .Append(kAccountId1.GetAccountIdKey())
          .Append("12345")
          .AddExtension(".jpg"),
      sea_pen_wallpaper_manager().GetFilePathForImageId(kAccountId1, 12345));

  const AccountId other_account_id = AccountId::FromUserEmailGaiaId(
      "other_user@test.com", "other_user@test.com");

  ASSERT_NE(other_account_id.GetAccountIdKey(), kAccountId1.GetAccountIdKey());

  EXPECT_EQ(GetTempFileDirectory()
                .Append(other_account_id.GetAccountIdKey())
                .Append("22222")
                .AddExtension(".jpg"),
            sea_pen_wallpaper_manager().GetFilePathForImageId(other_account_id,
                                                              22222));
}

}  // namespace ash
