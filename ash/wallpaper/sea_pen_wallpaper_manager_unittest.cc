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
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/manta/proto/manta.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

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
  }

  void TearDown() override { AshTestBase::TearDown(); }

  base::FilePath GetTempFileDirectory() { return scoped_temp_dir_.GetPath(); }

  base::FilePath CreateFilePath(base::FilePath::StringPieceType file_name) {
    return GetTempFileDirectory().Append(file_name);
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
  base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;
  const base::FilePath file_path = CreateFilePath("111.jpg");
  ASSERT_FALSE(base::PathExists(file_path));
  sea_pen_wallpaper_manager().DecodeAndSaveSeaPenImage(
      {CreateJpgBytes(), /*id=*/111, /*query=*/std::string(),
       manta::proto::ImageResolution::RESOLUTION_64},
      GetTempFileDirectory(), decode_sea_pen_image_future.GetCallback());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(SeaPenWallpaperManagerTest, StoresOnlyTenLatestImages) {
  base::test::TestFuture<const gfx::ImageSkia&> decode_sea_pen_image_future;

  base::FilePath file_path;
  // Create 10 images in the temp directory.
  for (int i = 1; i <= 10; i++) {
    const base::Time time = base::Time::Now();
    file_path = CreateFilePath(base::NumberToString(i) + ".jpg");
    ASSERT_TRUE(base::WriteFile(file_path, CreateJpgBytes()));
    // Change file modification time.
    ASSERT_TRUE(base::TouchFile(file_path, time - base::Minutes(10 - i),
                                time - base::Minutes(10 - i)));
  }

  ASSERT_TRUE(base::PathExists(CreateFilePath("1.jpg")));
  ASSERT_FALSE(base::PathExists(CreateFilePath("11.jpg")));

  // Decode and save the 11th sea pen image in the temp directory.
  sea_pen_wallpaper_manager().DecodeAndSaveSeaPenImage(
      {CreateJpgBytes(), /*id=*/11, /*query=*/std::string(),
       manta::proto::ImageResolution::RESOLUTION_64},
      GetTempFileDirectory(), decode_sea_pen_image_future.GetCallback());

  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));

  // The last modified image should be deleted when the 11th image is added.
  EXPECT_FALSE(base::PathExists(CreateFilePath("1.jpg")));
  EXPECT_TRUE(base::PathExists(CreateFilePath("11.jpg")));
}

}  // namespace ash
