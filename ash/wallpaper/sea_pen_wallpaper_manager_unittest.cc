// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <string>
#include <vector>

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
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

class SeaPenWallpaperManagerTest : public testing::Test {
 public:
  SeaPenWallpaperManagerTest() = default;

  SeaPenWallpaperManagerTest(const SeaPenWallpaperManagerTest&) = delete;
  SeaPenWallpaperManagerTest& operator=(const SeaPenWallpaperManagerTest&) =
      delete;

  ~SeaPenWallpaperManagerTest() override = default;

  SeaPenWallpaperManager& sea_pen_wallpaper_manager() {
    return sea_pen_wallpaper_manager_;
  }

  void SetUp() override {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  InProcessDataDecoder in_process_data_decoder_;
  SeaPenWallpaperManager sea_pen_wallpaper_manager_;
};

TEST_F(SeaPenWallpaperManagerTest, DecodesImageAndReturnsId) {
  base::test::TestFuture<uint32_t, const gfx::ImageSkia&>
      decode_sea_pen_image_future;
  sea_pen_wallpaper_manager().DecodeSeaPenImage(
      {CreateJpgBytes(), /*id=*/111, /*query=*/std::string(),
       manta::proto::ImageResolution::RESOLUTION_64},
      decode_sea_pen_image_future.GetCallback());

  EXPECT_EQ(111u, decode_sea_pen_image_future.Get<uint32_t>());
  // Use `AreBitmapsClose` because JPG encoding/decoding can alter the color
  // slightly.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      CreateBitmap(),
      *decode_sea_pen_image_future.Get<gfx::ImageSkia>().bitmap(),
      /*max_deviation=*/1));
}

}  // namespace
}  // namespace ash
