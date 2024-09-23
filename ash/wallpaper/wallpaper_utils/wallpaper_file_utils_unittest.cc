// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

constexpr int kPixelMaxDeviation = 1;

class ResizeAndSaveWallpaperTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  base::FilePath CreateFilePath(base::FilePath::StringPieceType file_name) {
    return scoped_temp_dir_.GetPath().Append(file_name);
  }

  gfx::ImageSkia DecodeImageFile(base::FilePath::StringPieceType file_name) {
    base::RunLoop run_loop;
    gfx::ImageSkia image_out;
    image_util::DecodeImageFile(
        base::BindLambdaForTesting(
            [&image_out, &run_loop](const gfx::ImageSkia& image) {
              image_out = image;
              run_loop.Quit();
            }),
        CreateFilePath(file_name));
    run_loop.Run();
    return image_out;
  }

  base::test::TaskEnvironment task_environment_;
  InProcessDataDecoder decoder_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(ResizeAndSaveWallpaperTest, CenterCroppedNoResizing) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(400, 200), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {400, 200}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(400, 200), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, CenterCroppedScaledDownSameAspectRatio) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(400, 200), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {200, 100}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(200, 100), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, CenterCroppedLandscapeToPortrait) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(1000, 600), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {150, 300}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(500, 300), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, CenterCroppedPortraitToLandscape) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(600, 1000), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {300, 150}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(300, 500), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, CenterCroppedImageSmallerThanPreferred) {
  EXPECT_FALSE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(400, 200), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {1000, 500}));
  EXPECT_FALSE(base::PathExists(CreateFilePath("cached_wallpaper")));
}

TEST_F(ResizeAndSaveWallpaperTest, StretchLayout) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(gfx::test::CreateImageSkia(400, 200),
                                     CreateFilePath("cached_wallpaper"),
                                     WALLPAPER_LAYOUT_STRETCH, {100, 150}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(100, 150), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, TileLayout) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(gfx::test::CreateImageSkia(/*size=*/100),
                                     CreateFilePath("cached_wallpaper"),
                                     WALLPAPER_LAYOUT_TILE, {50, 25}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(/*size=*/100), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, CenterLayout) {
  EXPECT_FALSE(ResizeAndSaveWallpaper(gfx::test::CreateImageSkia(400, 200),
                                      CreateFilePath("cached_wallpaper"),
                                      WALLPAPER_LAYOUT_CENTER, {400, 200}));
  EXPECT_FALSE(base::PathExists(CreateFilePath("cached_wallpaper")));
}

TEST_F(ResizeAndSaveWallpaperTest, DifferentWallpapers) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(gfx::test::CreateImageSkia(400, 200),
                                     CreateFilePath("cached_wallpaper_1"),
                                     WALLPAPER_LAYOUT_CENTER_CROPPED,
                                     {400, 200}));
  ASSERT_TRUE(ResizeAndSaveWallpaper(gfx::test::CreateImageSkia(600, 300),
                                     CreateFilePath("cached_wallpaper_2"),
                                     WALLPAPER_LAYOUT_CENTER_CROPPED,
                                     {600, 300}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper_1")),
      gfx::test::CreateImage(400, 200), kPixelMaxDeviation));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper_2")),
      gfx::test::CreateImage(600, 300), kPixelMaxDeviation));
}

TEST_F(ResizeAndSaveWallpaperTest, OverwritesExistingWallpaper) {
  ASSERT_TRUE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(400, 200), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {400, 200}));
  ASSERT_TRUE(ResizeAndSaveWallpaper(
      gfx::test::CreateImageSkia(600, 300), CreateFilePath("cached_wallpaper"),
      WALLPAPER_LAYOUT_CENTER_CROPPED, {600, 300}));
  EXPECT_TRUE(gfx::test::AreImagesClose(
      gfx::Image(DecodeImageFile("cached_wallpaper")),
      gfx::test::CreateImage(600, 300), kPixelMaxDeviation));
}

}  // namespace
}  // namespace ash
