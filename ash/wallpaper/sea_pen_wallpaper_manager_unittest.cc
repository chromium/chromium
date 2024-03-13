// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <string>
#include <vector>

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

const std::string kUser1 = "user1@test.com";
const AccountId kAccountId1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

SkBitmap CreateBitmap() {
  return gfx::test::CreateBitmap(1, SkColorSetARGB(255, 31, 63, 127));
}

std::string CreateJpgBytes() {
  SkBitmap bitmap = CreateBitmap();
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

base::subtle::ScopedTimeClockOverrides CreateScopedTimeNowOverride() {
  return base::subtle::ScopedTimeClockOverrides(
      []() -> base::Time {
        base::Time fake_now;
        bool success =
            base::Time::FromString("2023-04-05T01:23:45Z", &fake_now);
        DCHECK(success);
        return fake_now;
      },
      nullptr, nullptr);
}

personalization_app::mojom::SeaPenQueryPtr MakeTemplateQuery() {
  return personalization_app::mojom::SeaPenQuery::NewTemplateQuery(
      personalization_app::mojom::SeaPenTemplateQuery::New(
          personalization_app::mojom::SeaPenTemplateId::kFlower,
          ::base::flat_map<personalization_app::mojom::SeaPenTemplateChip,
                           personalization_app::mojom::SeaPenTemplateOption>(),
          personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title")));
}

class SeaPenWallpaperManagerTest : public AshTestBase {
 public:
  SeaPenWallpaperManagerTest() = default;

  SeaPenWallpaperManagerTest(const SeaPenWallpaperManagerTest&) = delete;
  SeaPenWallpaperManagerTest& operator=(const SeaPenWallpaperManagerTest&) =
      delete;

  ~SeaPenWallpaperManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    sea_pen_wallpaper_manager()->SetStorageDirectory(GetTempFileDirectory());
  }

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

  SeaPenWallpaperManager* sea_pen_wallpaper_manager() {
    return SeaPenWallpaperManager::GetInstance();
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(SeaPenWallpaperManagerTest, DecodesImageAndReturnsId) {
  constexpr uint32_t image_id = 111;
  base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
  const base::FilePath file_path =
      sea_pen_wallpaper_manager()->GetFilePathForImageId(kAccountId1, image_id);
  ASSERT_FALSE(base::PathExists(file_path));
  sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
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

TEST_F(SeaPenWallpaperManagerTest, StoresTwelveImages) {
  // Create 12 images in the temp directory.
  for (uint32_t i = 1; i <= 12; i++) {
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
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
        sea_pen_wallpaper_manager()->GetFilePathForImageId(kAccountId1, i);
    EXPECT_TRUE(base::PathExists(file_path));
  }

  EXPECT_THAT(GetIdsFromFilePaths(GetJpgFilesForAccountId(kAccountId1)),
              testing::UnorderedElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 11u, 12u));
}

TEST_F(SeaPenWallpaperManagerTest, ThirteenthImageReplacesOldest) {
  // Create 12 images in the temp directory.
  for (uint32_t i = 1; i <= 12; i++) {
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        decode_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(decode_sea_pen_image_future.Wait());
  }

  constexpr uint32_t oldest_image_id = 5;
  // Mark image 5 as the oldest by last modified time.
  ASSERT_TRUE(
      base::TouchFile(sea_pen_wallpaper_manager()->GetFilePathForImageId(
                          kAccountId1, oldest_image_id),
                      /*last_accessed=*/base::Time::Now(),
                      /*last_modified=*/base::Time::Now() - base::Minutes(30)));

  constexpr uint32_t new_image_id = 13;

  ASSERT_FALSE(
      base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
          kAccountId1, new_image_id)));

  // Decode and save the 13th sea pen image.
  base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
  sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
      kAccountId1, {CreateJpgBytes(), new_image_id},
      personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
      decode_sea_pen_image_future.GetCallback());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));

  // The last modified image should be deleted when the 13th image is added.
  EXPECT_THAT(GetIdsFromFilePaths(GetJpgFilesForAccountId(kAccountId1)),
              testing::UnorderedElementsAre(1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u, 10u,
                                            11u, 12u, new_image_id));
  EXPECT_FALSE(
      base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
          kAccountId1, oldest_image_id)));
  EXPECT_TRUE(
      base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
          kAccountId1, new_image_id)));
}

TEST_F(SeaPenWallpaperManagerTest, GetImageIds) {
  // Create images in the temp directory.
  for (uint32_t i = 1; i <= 5; i++) {
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i * i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        decode_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(decode_sea_pen_image_future.Wait());
  }

  {
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    sea_pen_wallpaper_manager()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Take(),
                testing::UnorderedElementsAre(1, 4, 9, 16, 25));
  }

  {
    base::test::TestFuture<bool> delete_recent_sea_pen_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, 16u, delete_recent_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(delete_recent_sea_pen_image_future.Take());
  }

  {
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    sea_pen_wallpaper_manager()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Take(),
                testing::UnorderedElementsAre(1, 4, 9, 25));
  }
}

TEST_F(SeaPenWallpaperManagerTest, GetImageIdsMultipleAccounts) {
  {
    // Create an image for account 1.
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), 77},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        decode_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(decode_sea_pen_image_future.Wait());
  }

  const std::string kUser2 = "user2@test.com";
  const AccountId kAccountId2 = AccountId::FromUserEmailGaiaId(kUser2, kUser2);
  ASSERT_NE(kAccountId1.GetAccountIdKey(), kAccountId2.GetAccountIdKey());

  {
    // Create an image for account 2.
    base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId2, {CreateJpgBytes(), 987654321},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        decode_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(decode_sea_pen_image_future.Wait());
  }

  {
    // Retrieve images for account 1.
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    sea_pen_wallpaper_manager()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Take(), testing::UnorderedElementsAre(77));
  }

  {
    // Retrieve images for account 2.
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    sea_pen_wallpaper_manager()->GetImageIds(
        kAccountId2, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Take(),
                testing::UnorderedElementsAre(987654321));
  }
}

TEST_F(SeaPenWallpaperManagerTest, GetFilePathForImageId) {
  EXPECT_EQ(
      GetTempFileDirectory()
          .Append(kAccountId1.GetAccountIdKey())
          .Append("12345")
          .AddExtension(".jpg"),
      sea_pen_wallpaper_manager()->GetFilePathForImageId(kAccountId1, 12345));

  const AccountId other_account_id = AccountId::FromUserEmailGaiaId(
      "other_user@test.com", "other_user@test.com");

  ASSERT_NE(other_account_id.GetAccountIdKey(), kAccountId1.GetAccountIdKey());

  EXPECT_EQ(GetTempFileDirectory()
                .Append(other_account_id.GetAccountIdKey())
                .Append("22222")
                .AddExtension(".jpg"),
            sea_pen_wallpaper_manager()->GetFilePathForImageId(other_account_id,
                                                               22222));
}

TEST_F(SeaPenWallpaperManagerTest, GetImageAndMetadataSuccess) {
  constexpr uint32_t image_id = 88888888;
  const auto time_override = CreateScopedTimeNowOverride();

  {
    base::test::TestFuture<const gfx::ImageSkia&> save_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id}, MakeTemplateQuery(),
        save_image_future.GetCallback());

    // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
    // slightly.
    EXPECT_TRUE(gfx::test::AreBitmapsClose(
        CreateBitmap(), *save_image_future.Get<gfx::ImageSkia>().bitmap(),
        /*max_deviation=*/1));
  }

  {
    base::test::TestFuture<const gfx::ImageSkia&,
                           personalization_app::mojom::RecentSeaPenImageInfoPtr>
        get_image_and_metadata_future;
    sea_pen_wallpaper_manager()->GetImageAndMetadata(
        kAccountId1, image_id, get_image_and_metadata_future.GetCallback());

    EXPECT_TRUE(gfx::test::AreBitmapsClose(
        CreateBitmap(),
        *get_image_and_metadata_future.Get<gfx::ImageSkia>().bitmap(),
        /*max_deviation=*/1));
    EXPECT_EQ("test template query",
              get_image_and_metadata_future
                  .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                  ->user_visible_query->text);
    EXPECT_EQ("test template title",
              get_image_and_metadata_future
                  .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                  ->user_visible_query->template_title);
    // base::Time::Now is overridden to return a fixed date.
    EXPECT_EQ(base::TimeFormatShortDate(base::Time::Now()),
              get_image_and_metadata_future
                  .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                  ->creation_time.value());
  }
}

TEST_F(SeaPenWallpaperManagerTest, GetImageAndMetadataInvalidJson) {
  constexpr uint32_t image_id = 918273645;
  const auto time_override = CreateScopedTimeNowOverride();

  {
    // Create valid metadata dict.
    base::Value::Dict query_dict = SeaPenQueryToDict(MakeTemplateQuery());

    // Rename a necessary field to cause parsing failure.
    ASSERT_TRUE(query_dict.contains("user_visible_query_text"));
    query_dict.Set("user_visible_query_text_bad",
                   query_dict.Extract("user_visible_query_text").value());

    // Write the jpg with invalid metadata.
    const base::FilePath target_file_path =
        sea_pen_wallpaper_manager()->GetFilePathForImageId(kAccountId1,
                                                           image_id);
    ASSERT_TRUE(base::CreateDirectory(target_file_path.DirName()));
    gfx::ImageSkia test_image =
        gfx::ImageSkia::CreateFrom1xBitmap(CreateBitmap());
    ASSERT_TRUE(ResizeAndSaveWallpaper(
        test_image, target_file_path,
        WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED, test_image.size(),
        QueryDictToXmpString(query_dict)));
  }

  base::test::TestFuture<const gfx::ImageSkia&,
                         personalization_app::mojom::RecentSeaPenImageInfoPtr>
      get_image_and_metadata_future;
  sea_pen_wallpaper_manager()->GetImageAndMetadata(
      kAccountId1, image_id, get_image_and_metadata_future.GetCallback());

  // Image loading still succeeds.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *get_image_and_metadata_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));

  // No metadata loaded.
  EXPECT_TRUE(get_image_and_metadata_future
                  .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                  .is_null());
}

TEST_F(SeaPenWallpaperManagerTest, GetImageAndMetadataNonExistentId) {
  constexpr uint32_t image_id = 88888888;

  ASSERT_FALSE(
      base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
          kAccountId1, image_id)));

  base::test::TestFuture<const gfx::ImageSkia&,
                         personalization_app::mojom::RecentSeaPenImageInfoPtr>
      get_image_and_metadata_future;
  sea_pen_wallpaper_manager()->GetImageAndMetadata(
      kAccountId1, image_id, get_image_and_metadata_future.GetCallback());

  EXPECT_TRUE(get_image_and_metadata_future.Get<gfx::ImageSkia>().isNull());
  EXPECT_TRUE(get_image_and_metadata_future
                  .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                  .is_null());
}

TEST_F(SeaPenWallpaperManagerTest, GetImageAndMetadataOtherAccount) {
  constexpr uint32_t image_id = 8888;
  {
    // Write an image for first account.
    base::test::TestFuture<const gfx::ImageSkia&> save_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());
    ASSERT_TRUE(save_image_future.Wait());
  }

  {
    // Try to retrieve the image with another account.
    const AccountId other_account_id = AccountId::FromUserEmailGaiaId(
        "other_user@test.com", "other_user@test.com");

    base::test::TestFuture<const gfx::ImageSkia&,
                           personalization_app::mojom::RecentSeaPenImageInfoPtr>
        get_image_and_metadata_future;
    sea_pen_wallpaper_manager()->GetImageAndMetadata(
        other_account_id, image_id,
        get_image_and_metadata_future.GetCallback());

    EXPECT_TRUE(get_image_and_metadata_future.Get<gfx::ImageSkia>().isNull());
    EXPECT_TRUE(get_image_and_metadata_future
                    .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                    .is_null());
  }
}

TEST_F(SeaPenWallpaperManagerTest, DeleteNonExistentImage) {
  // File does not exist yet. Deleting it should fail.
  base::test::TestFuture<bool> delete_sea_pen_image_future;
  sea_pen_wallpaper_manager()->DeleteSeaPenImage(
      kAccountId1, 111u, delete_sea_pen_image_future.GetCallback());
  EXPECT_FALSE(delete_sea_pen_image_future.Get());
}

TEST_F(SeaPenWallpaperManagerTest, DeleteImageRemovesFromDisk) {
  constexpr uint32_t image_id = 1234u;

  {
    // Save a test image.
    base::test::TestFuture<const gfx::ImageSkia&> save_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());

    ASSERT_TRUE(save_image_future.Wait());
    ASSERT_TRUE(
        base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
            kAccountId1, image_id)));
  }

  {
    // Delete the test image.
    base::test::TestFuture<bool> delete_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, image_id, delete_image_future.GetCallback());

    EXPECT_TRUE(delete_image_future.Get());
    EXPECT_FALSE(
        base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
            kAccountId1, image_id)));
  }
}

TEST_F(SeaPenWallpaperManagerTest, DeleteImageForOtherUserFails) {
  constexpr uint32_t image_id = 999u;
  const AccountId other_account_id = AccountId::FromUserEmailGaiaId(
      "other_user@test.com", "other_user@test.com");

  // Save a test image with the same id for both users.
  for (const auto& account_id : {kAccountId1, other_account_id}) {
    base::test::TestFuture<const gfx::ImageSkia&> save_image_future;
    sea_pen_wallpaper_manager()->DecodeAndSaveSeaPenImage(
        account_id, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());

    ASSERT_TRUE(save_image_future.Wait());
    ASSERT_TRUE(
        base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
            kAccountId1, image_id)));
  }

  {
    // Delete the image for first account id.
    base::test::TestFuture<bool> delete_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, image_id, delete_image_future.GetCallback());

    EXPECT_TRUE(delete_image_future.Get());
    EXPECT_FALSE(
        base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
            kAccountId1, image_id)));
  }

  // Image still exists for other account id.
  ASSERT_TRUE(
      base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
          other_account_id, image_id)));

  {
    // Try delete the image for first account id again, should fail.
    base::test::TestFuture<bool> delete_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, image_id, delete_image_future.GetCallback());

    EXPECT_FALSE(delete_image_future.Get());
    EXPECT_FALSE(
        base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
            kAccountId1, image_id)));
  }

  // Image still exists for other account id.
  ASSERT_TRUE(
      base::PathExists(sea_pen_wallpaper_manager()->GetFilePathForImageId(
          other_account_id, image_id)));
}

}  // namespace
}  // namespace ash
