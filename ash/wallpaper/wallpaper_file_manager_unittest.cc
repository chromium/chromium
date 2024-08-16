// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_file_manager.h"

#include <string>

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
          CHECK(false);
          return "Unknown";
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

}  // namespace
}  // namespace ash
