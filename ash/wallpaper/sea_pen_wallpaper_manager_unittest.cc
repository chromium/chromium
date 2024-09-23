// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/test_sea_pen_wallpaper_manager_session_delegate.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
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
constexpr std::string_view kExpectedMigrationFileContents =
    "migration_file_contents";
const AccountId kAccountId1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);
constexpr SkColor kDefaultImageColor = SkColorSetARGB(255, 31, 63, 127);

SkBitmap CreateBitmap(SkColor color = kDefaultImageColor) {
  return gfx::test::CreateBitmap(1, color);
}

std::string CreateJpgBytes(SkColor color = kDefaultImageColor) {
  SkBitmap bitmap = CreateBitmap(color);
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
                           personalization_app::mojom::SeaPenTemplateOption>(
              {{ash::personalization_app::mojom::SeaPenTemplateChip::
                    kFlowerColor,
                ash::personalization_app::mojom::SeaPenTemplateOption::
                    kFlowerColorBlue},
               {ash::personalization_app::mojom::SeaPenTemplateChip::
                    kFlowerType,
                ash::personalization_app::mojom::SeaPenTemplateOption::
                    kFlowerTypeRose}}),
          personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title")));
}

class SeaPenWallpaperManagerTest : public testing::Test {
 public:
  SeaPenWallpaperManagerTest() = default;

  SeaPenWallpaperManagerTest(const SeaPenWallpaperManagerTest&) = delete;
  SeaPenWallpaperManagerTest& operator=(const SeaPenWallpaperManagerTest&) =
      delete;

  ~SeaPenWallpaperManagerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    sea_pen_wallpaper_manager_.SetSessionDelegateForTesting(
        std::make_unique<TestSeaPenWallpaperManagerSessionDelegate>());
  }

  base::FilePath GetFilePathForImageId(const AccountId& account_id,
                                       uint32_t image_id) {
    return sea_pen_wallpaper_manager_session_delegate()
        ->GetStorageDirectory(account_id)
        .Append(base::NumberToString(image_id))
        .AddExtension(".jpg");
  }

  std::vector<base::FilePath> GetJpgFilesForAccountId(
      const AccountId& account_id) {
    const auto target_directory =
        sea_pen_wallpaper_manager_session_delegate()->GetStorageDirectory(
            account_id);
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
    return &sea_pen_wallpaper_manager_;
  }

  TestSeaPenWallpaperManagerSessionDelegate*
  sea_pen_wallpaper_manager_session_delegate() {
    return static_cast<TestSeaPenWallpaperManagerSessionDelegate*>(
        sea_pen_wallpaper_manager()->session_delegate_for_testing());
  }

  void SetUpMigrationSourceDir(const AccountId& account_id) {
    ASSERT_TRUE(migration_source_dir_.CreateUniqueTempDir());

    const base::FilePath source_subdir =
        migration_source_dir_.GetPath().Append(account_id.GetAccountIdKey());
    ASSERT_TRUE(base::CreateDirectory(source_subdir));

    const base::FilePath source_file =
        source_subdir.Append("12345").AddExtension(".jpg");
    ASSERT_TRUE(base::WriteFile(source_file, kExpectedMigrationFileContents));
  }

  base::FilePath GetMigrationSourceDir(const AccountId& account_id) {
    return migration_source_dir_.GetPath().Append(account_id.GetAccountIdKey());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir migration_source_dir_;
  InProcessDataDecoder in_process_data_decoder_;
  TestingPrefServiceSimple profile_prefs_;
  SeaPenWallpaperManager sea_pen_wallpaper_manager_;
};

TEST_F(SeaPenWallpaperManagerTest, DecodesImageAndReturnsId) {
  constexpr uint32_t image_id = 111;
  const base::FilePath file_path = GetFilePathForImageId(kAccountId1, image_id);
  ASSERT_FALSE(base::PathExists(file_path));

  base::test::TestFuture<bool> save_sea_pen_image_future;
  sea_pen_wallpaper_manager()->SaveSeaPenImage(
      kAccountId1, {CreateJpgBytes(), image_id},
      personalization_app::mojom::SeaPenQuery::NewTextQuery("search query"),
      save_sea_pen_image_future.GetCallback());
  ASSERT_TRUE(save_sea_pen_image_future.Get());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  base::test::TestFuture<const gfx::ImageSkia&> get_image_future;
  sea_pen_wallpaper_manager()->GetImage(kAccountId1, image_id,
                                        get_image_future.GetCallback());
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(), *get_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(SeaPenWallpaperManagerTest, StoresTwelveImages) {
  // Create 12 images in the temp directory.
  for (uint32_t i = 1; i <= 12; i++) {
    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());

    const auto file_path = GetFilePathForImageId(kAccountId1, i);
    EXPECT_TRUE(base::PathExists(file_path));
  }

  EXPECT_THAT(GetIdsFromFilePaths(GetJpgFilesForAccountId(kAccountId1)),
              testing::UnorderedElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 11u, 12u));
}

TEST_F(SeaPenWallpaperManagerTest, ThirteenthImageReplacesOldest) {
  // Create 12 images in the temp directory.
  for (uint32_t i = 1; i <= 12; i++) {
    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());
  }

  constexpr uint32_t oldest_image_id = 5;
  // Mark image 5 as the oldest by last modified time.
  ASSERT_TRUE(
      base::TouchFile(GetFilePathForImageId(kAccountId1, oldest_image_id),
                      /*last_accessed=*/base::Time::Now(),
                      /*last_modified=*/base::Time::Now() - base::Minutes(30)));

  constexpr uint32_t new_image_id = 13;

  ASSERT_FALSE(
      base::PathExists(GetFilePathForImageId(kAccountId1, new_image_id)));

  // Decode and save the 13th sea pen image.
  base::test::TestFuture<bool> save_sea_pen_image_future;
  sea_pen_wallpaper_manager()->SaveSeaPenImage(
      kAccountId1, {CreateJpgBytes(SK_ColorBLUE), new_image_id},
      personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
      save_sea_pen_image_future.GetCallback());
  ASSERT_TRUE(save_sea_pen_image_future.Get());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  base::test::TestFuture<const gfx::ImageSkia&> get_image_future;
  sea_pen_wallpaper_manager()->GetImage(kAccountId1, new_image_id,
                                        get_image_future.GetCallback());
  EXPECT_TRUE(gfx::test::AreBitmapsClose(CreateBitmap(SK_ColorBLUE),
                                         *get_image_future.Get().bitmap(),
                                         /*max_deviation=*/1));

  // The last modified image should be deleted when the 13th image is added.
  EXPECT_THAT(GetIdsFromFilePaths(GetJpgFilesForAccountId(kAccountId1)),
              testing::UnorderedElementsAre(1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u, 10u,
                                            11u, 12u, new_image_id));
  EXPECT_FALSE(
      base::PathExists(GetFilePathForImageId(kAccountId1, oldest_image_id)));
  EXPECT_TRUE(
      base::PathExists(GetFilePathForImageId(kAccountId1, new_image_id)));
}

TEST_F(SeaPenWallpaperManagerTest, GetImageIds) {
  // Create images in the temp directory.
  for (uint32_t i = 1; i <= 5; i++) {
    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i * i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());
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
    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), 77},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());
  }

  const std::string kUser2 = "user2@test.com";
  const AccountId kAccountId2 = AccountId::FromUserEmailGaiaId(kUser2, kUser2);
  ASSERT_NE(kAccountId1.GetAccountIdKey(), kAccountId2.GetAccountIdKey());

  {
    // Create an image for account 2.
    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId2, {CreateJpgBytes(), 987654321},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());
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

TEST_F(SeaPenWallpaperManagerTest, GetImageIdsSortedByLastModifiedTime) {
  // Create images in the temp directory.
  base::Time fake_last_modified_time;
  ASSERT_TRUE(
      base::Time::FromString("2018-03-05T16:16:16Z", &fake_last_modified_time));

  for (uint32_t i = 1; i <= 5; i++) {
    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), i * i},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());

    // File save does not respect base::Time::Now override as it will use actual
    // file system stat calls, so set it manually.
    const base::FilePath image_path =
        sea_pen_wallpaper_manager_session_delegate()
            ->GetStorageDirectory(kAccountId1)
            .Append(base::NumberToString(i * i))
            .AddExtension(".jpg");
    ASSERT_TRUE(base::TouchFile(image_path, fake_last_modified_time,
                                fake_last_modified_time));
    fake_last_modified_time += base::Seconds(1);
  }

  {
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    sea_pen_wallpaper_manager()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Get(),
                testing::ElementsAre(25, 16, 9, 4, 1));
  }

  {
    // Mark 4 as newest.
    base::test::TestFuture<bool> touch_file_future;
    sea_pen_wallpaper_manager()->TouchFile(kAccountId1, 4);
  }

  {
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    sea_pen_wallpaper_manager()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Get(),
                testing::ElementsAre(4, 25, 16, 9, 1));
  }
}

TEST_F(SeaPenWallpaperManagerTest, GetImageAndMetadataSuccess) {
  constexpr uint32_t image_id = 88888888;
  const auto time_override = CreateScopedTimeNowOverride();

  {
    base::test::TestFuture<bool> save_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id}, MakeTemplateQuery(),
        save_image_future.GetCallback());
    ASSERT_TRUE(save_image_future.Get());
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
    EXPECT_TRUE(MakeTemplateQuery().Equals(
        get_image_and_metadata_future
            .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
            ->query));
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
    const auto template_query = MakeTemplateQuery();
    // Create valid metadata dict.
    base::Value::Dict query_dict = SeaPenQueryToDict(MakeTemplateQuery());

    // Rename a necessary field to cause parsing failure.
    ASSERT_TRUE(query_dict.contains("user_visible_query_text"));
    query_dict.Set("user_visible_query_text_bad",
                   query_dict.Extract("user_visible_query_text").value());

    // Write the jpg with invalid metadata.
    const base::FilePath target_file_path =
        GetFilePathForImageId(kAccountId1, image_id);
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

  ASSERT_FALSE(base::PathExists(GetFilePathForImageId(kAccountId1, image_id)));

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
    base::test::TestFuture<bool> save_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());
    ASSERT_TRUE(save_image_future.Get());
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

TEST_F(SeaPenWallpaperManagerTest, GetTemplateIdFromFileSuccess) {
  const uint32_t image_id = 88888888;

  {
    base::test::TestFuture<bool> save_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id}, MakeTemplateQuery(),
        save_image_future.GetCallback());
    ASSERT_TRUE(save_image_future.Get());
  }

  {
    base::test::TestFuture<std::optional<int>> get_template_id_future;
    sea_pen_wallpaper_manager()->GetTemplateIdFromFile(
        kAccountId1, image_id, get_template_id_future.GetCallback());
    EXPECT_EQ(static_cast<int>(
                  ash::personalization_app::mojom::SeaPenTemplateId::kFlower),
              *get_template_id_future.Get());
  }
}

TEST_F(SeaPenWallpaperManagerTest, GetTemplateIdFromFileFailure) {
  const uint32_t image_id = 88888888;

  {
    base::test::TestFuture<bool> save_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());
    ASSERT_TRUE(save_image_future.Get());
  }

  {
    base::test::TestFuture<std::optional<int>> get_template_id_future;
    sea_pen_wallpaper_manager()->GetTemplateIdFromFile(
        kAccountId1, image_id, get_template_id_future.GetCallback());
    EXPECT_FALSE(get_template_id_future.Get());
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
    base::test::TestFuture<bool> save_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        kAccountId1, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());

    ASSERT_TRUE(save_image_future.Get());
    ASSERT_TRUE(base::PathExists(GetFilePathForImageId(kAccountId1, image_id)));
  }

  {
    // Delete the test image.
    base::test::TestFuture<bool> delete_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, image_id, delete_image_future.GetCallback());

    EXPECT_TRUE(delete_image_future.Get());
    EXPECT_FALSE(
        base::PathExists(GetFilePathForImageId(kAccountId1, image_id)));
  }
}

TEST_F(SeaPenWallpaperManagerTest, DeleteImageForOtherUserFails) {
  constexpr uint32_t image_id = 999u;
  const AccountId other_account_id = AccountId::FromUserEmailGaiaId(
      "other_user@test.com", "other_user@test.com");

  // Save a test image with the same id for both users.
  for (const auto& account_id : {kAccountId1, other_account_id}) {
    base::test::TestFuture<bool> save_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        account_id, {CreateJpgBytes(), image_id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("test query"),
        save_image_future.GetCallback());

    ASSERT_TRUE(save_image_future.Get());
    ASSERT_TRUE(base::PathExists(GetFilePathForImageId(kAccountId1, image_id)));
  }

  {
    // Delete the image for first account id.
    base::test::TestFuture<bool> delete_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, image_id, delete_image_future.GetCallback());

    EXPECT_TRUE(delete_image_future.Get());
    EXPECT_FALSE(
        base::PathExists(GetFilePathForImageId(kAccountId1, image_id)));
  }

  // Image still exists for other account id.
  ASSERT_TRUE(
      base::PathExists(GetFilePathForImageId(other_account_id, image_id)));

  {
    // Try delete the image for first account id again, should fail.
    base::test::TestFuture<bool> delete_image_future;
    sea_pen_wallpaper_manager()->DeleteSeaPenImage(
        kAccountId1, image_id, delete_image_future.GetCallback());

    EXPECT_FALSE(delete_image_future.Get());
    EXPECT_FALSE(
        base::PathExists(GetFilePathForImageId(kAccountId1, image_id)));
  }

  // Image still exists for other account id.
  ASSERT_TRUE(
      base::PathExists(GetFilePathForImageId(other_account_id, image_id)));
}

TEST_F(SeaPenWallpaperManagerTest, MigrateMovesFiles) {
  SetUpMigrationSourceDir(kAccountId1);
  ASSERT_TRUE(base::PathExists(GetMigrationSourceDir(kAccountId1)));

  base::test::TestFuture<bool> migrate_sea_pen_files_if_necessary_future;
  sea_pen_wallpaper_manager()->Migrate(
      kAccountId1, GetMigrationSourceDir(kAccountId1),
      migrate_sea_pen_files_if_necessary_future.GetCallback());
  EXPECT_TRUE(migrate_sea_pen_files_if_necessary_future.Get());

  std::string migrated_file_contents;
  EXPECT_TRUE(
      base::ReadFileToString(sea_pen_wallpaper_manager_session_delegate()
                                 ->GetStorageDirectory(kAccountId1)
                                 .Append("12345.jpg"),
                             &migrated_file_contents));
  EXPECT_EQ(kExpectedMigrationFileContents, migrated_file_contents);

  EXPECT_FALSE(base::PathExists(GetMigrationSourceDir(kAccountId1)));

  base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
  sea_pen_wallpaper_manager()->GetImageIds(kAccountId1,
                                           get_image_ids_future.GetCallback());

  EXPECT_EQ(std::vector<uint32_t>{12345}, get_image_ids_future.Get());
}

TEST_F(SeaPenWallpaperManagerTest, MigrateWritesPrefs) {
  EXPECT_EQ(
      SeaPenWallpaperManager::MigrationStatus::kNotStarted,
      static_cast<SeaPenWallpaperManager::MigrationStatus>(
          sea_pen_wallpaper_manager_session_delegate()
              ->GetPrefService(kAccountId1)
              ->GetInteger(::ash::prefs::kWallpaperSeaPenMigrationStatus)));

  base::ScopedTempDir source_dir;
  ASSERT_TRUE(source_dir.CreateUniqueTempDir());

  const base::FilePath source_subdir =
      source_dir.GetPath().Append(kAccountId1.GetAccountIdKey());
  ASSERT_TRUE(base::CreateDirectory(source_subdir));

  base::test::TestFuture<bool> migrate_sea_pen_files_if_necessary_future;

  sea_pen_wallpaper_manager()->Migrate(
      kAccountId1, source_subdir,
      migrate_sea_pen_files_if_necessary_future.GetCallback());

  EXPECT_EQ(
      SeaPenWallpaperManager::MigrationStatus::kCrashed,
      static_cast<SeaPenWallpaperManager::MigrationStatus>(
          sea_pen_wallpaper_manager_session_delegate()
              ->GetPrefService(kAccountId1)
              ->GetInteger(::ash::prefs::kWallpaperSeaPenMigrationStatus)))
      << "kCrashed should have been written as migration started";

  EXPECT_TRUE(migrate_sea_pen_files_if_necessary_future.Get());

  EXPECT_EQ(
      SeaPenWallpaperManager::MigrationStatus::kSuccess,
      static_cast<SeaPenWallpaperManager::MigrationStatus>(
          sea_pen_wallpaper_manager_session_delegate()
              ->GetPrefService(kAccountId1)
              ->GetInteger(::ash::prefs::kWallpaperSeaPenMigrationStatus)));
}

}  // namespace
}  // namespace ash
