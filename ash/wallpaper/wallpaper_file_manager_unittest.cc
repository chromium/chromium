// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_file_manager.h"

#include <string>

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

// jpg encoding/decoding sometimes changes the pixels slightly.
constexpr int kMaxPixelDeviation = 1;

class WallpaperFileManagerTest
    : public ::testing::Test,
      public testing::WithParamInterface<WallpaperType> {
 public:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  WallpaperType wallpaper_type() const { return GetParam(); }

  WallpaperFileManager& wallpaper_file_manager() {
    return wallpaper_file_manager_;
  }

  base::FilePath scoped_temp_dir_path() { return scoped_temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  InProcessDataDecoder decoder_;
  WallpaperFileManager wallpaper_file_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // empty to simplify gtest output
    ,
    WallpaperFileManagerTest,
    testing::Values(WallpaperType::kOnline,
                    WallpaperType::kDaily,
                    WallpaperType::kOnceGooglePhotos,
                    WallpaperType::kDailyGooglePhotos,
                    WallpaperType::kCustomized),
    [](const testing::TestParamInfo<WallpaperFileManagerTest::ParamType>& info)
        -> std::string {
      switch (info.param) {
        case WallpaperType::kOnline:
          return "Online";
        case WallpaperType::kDaily:
          return "Daily";
        case WallpaperType::kOnceGooglePhotos:
          return "OnceGooglePhotos";
        case WallpaperType::kDailyGooglePhotos:
          return "DailyGooglePhotos";
        case WallpaperType::kCustomized:
          return "Customized";
        default:
          NOTREACHED();
      }
    });

TEST_P(WallpaperFileManagerTest, LoadMissingWallpaper) {
  base::test::TestFuture<const gfx::ImageSkia&> load_wallpaper_future;

  wallpaper_file_manager().LoadWallpaper(
      wallpaper_type(), scoped_temp_dir_path(), "test_location",
      load_wallpaper_future.GetCallback());

  EXPECT_TRUE(load_wallpaper_future.Get().isNull());
}

TEST_P(WallpaperFileManagerTest, SaveAndLoadSameWallpaper) {
  const gfx::ImageSkia test_image =
      gfx::test::CreateImageSkia(10, SK_ColorYELLOW);

  base::test::TestFuture<const base::FilePath&> save_wallpaper_future;

  wallpaper_file_manager().SaveWallpaperToDisk(
      wallpaper_type(), scoped_temp_dir_path(), "test_file_name.jpg",
      WALLPAPER_LAYOUT_CENTER_CROPPED, test_image,
      save_wallpaper_future.GetCallback(), "wallpaper_files_id");

  ASSERT_FALSE(save_wallpaper_future.Get().empty());

  std::string location;
  switch (wallpaper_type()) {
    case WallpaperType::kOnline:
    case WallpaperType::kDaily:
      location = "https://example.com/test_file_name.jpg";
      break;
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
      location = "test_file_name.jpg";
      break;
    case WallpaperType::kCustomized:
      location = "original/wallpaper_files_id/test_file_name.jpg";
      break;
    case WallpaperType::kSeaPen:
    case WallpaperType::kDefault:
    case WallpaperType::kDevice:
    case WallpaperType::kPolicy:
    case WallpaperType::kOobe:
    case WallpaperType::kThirdParty:
    case WallpaperType::kOneShot:
    case WallpaperType::kCount:
      NOTREACHED();
  }

  base::test::TestFuture<const gfx::ImageSkia&> load_wallpaper_future;

  wallpaper_file_manager().LoadWallpaper(wallpaper_type(),
                                         scoped_temp_dir_path(), location,
                                         load_wallpaper_future.GetCallback());

  EXPECT_TRUE(gfx::test::AreImagesClose(gfx::Image(test_image),
                                        gfx::Image(load_wallpaper_future.Get()),
                                        kMaxPixelDeviation));
}

class WallpaperFileManagerLocationTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    wallpaper_dir_ = scoped_temp_dir_.GetPath()
                         .Append("wallpapers")
                         .Append("google_photos")
                         .Append("account_key");
    other_dir_ = scoped_temp_dir_.GetPath().Append("other");
    ASSERT_TRUE(base::CreateDirectory(wallpaper_dir_));
    ASSERT_TRUE(base::CreateDirectory(other_dir_));
  }

  base::FilePath wallpaper_dir_;
  base::FilePath other_dir_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  InProcessDataDecoder decoder_;
  WallpaperFileManager wallpaper_file_manager_;
};

TEST_F(WallpaperFileManagerLocationTest, SaveRejectsFileNameReferencingParent) {
  const base::Time past = base::Time::Now() - base::Days(1);
  ASSERT_TRUE(base::TouchFile(other_dir_, past, past));
  base::File::Info initial_info;
  ASSERT_TRUE(base::GetFileInfo(other_dir_, &initial_info));

  base::test::TestFuture<const base::FilePath&> save_wallpaper_future;
  wallpaper_file_manager_.SaveWallpaperToDisk(
      WallpaperType::kOnceGooglePhotos, wallpaper_dir_,
      "../../../other/file.jpg", WALLPAPER_LAYOUT_CENTER_CROPPED,
      gfx::test::CreateImageSkia(10, SK_ColorBLUE),
      save_wallpaper_future.GetCallback());

  EXPECT_TRUE(save_wallpaper_future.Get().empty());
  EXPECT_TRUE(base::IsDirectoryEmpty(other_dir_));

  base::File::Info final_info;
  ASSERT_TRUE(base::GetFileInfo(other_dir_, &final_info));
  EXPECT_EQ(initial_info.last_modified, final_info.last_modified);
}

TEST_F(WallpaperFileManagerLocationTest, LoadRejectsLocationReferencingParent) {
  base::test::TestFuture<const base::FilePath&> save_wallpaper_future;
  wallpaper_file_manager_.SaveWallpaperToDisk(
      WallpaperType::kOnceGooglePhotos, other_dir_, "file.jpg",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      gfx::test::CreateImageSkia(10, SK_ColorBLUE),
      save_wallpaper_future.GetCallback());
  ASSERT_FALSE(save_wallpaper_future.Get().empty());

  base::test::TestFuture<const gfx::ImageSkia&> load_wallpaper_future;
  wallpaper_file_manager_.LoadWallpaper(
      WallpaperType::kOnceGooglePhotos, wallpaper_dir_,
      "../../../other/file.jpg", load_wallpaper_future.GetCallback());

  EXPECT_TRUE(load_wallpaper_future.Get().isNull());
}

}  // namespace
}  // namespace ash
