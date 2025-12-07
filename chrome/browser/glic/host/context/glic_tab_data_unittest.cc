// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_data.h"

#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

base::FilePath GlicTestDataPath(std::string_view filename) {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .AppendASCII("glic")
      .AppendASCII(filename);
}

}  // namespace

namespace glic {

TEST(GlicTabDataTest, CompareIdenticalFaviconImages) {
  std::optional<std::vector<uint8_t>> png_data =
      base::ReadFileToBytes(GlicTestDataPath("youtube_favicon_32x32.png"));
  ASSERT_TRUE(png_data.has_value());
  SkBitmap bitmap_from_png = gfx::PNGCodec::Decode(png_data.value());
  std::optional<std::vector<uint8_t>> other_png_data =
      base::ReadFileToBytes(GlicTestDataPath("youtube_favicon_32x32.png"));
  ASSERT_TRUE(other_png_data.has_value());
  SkBitmap bitmap_from_other_png =
      gfx::PNGCodec::Decode(other_png_data.value());

  EXPECT_TRUE(FaviconEquals(bitmap_from_png, bitmap_from_other_png));
}

TEST(GlicTabDataTest, CompareFaviconImagesWithDifferentColorSpaces) {
  std::optional<std::vector<uint8_t>> png_data =
      base::ReadFileToBytes(GlicTestDataPath("youtube_favicon_32x32.png"));
  ASSERT_TRUE(png_data.has_value());
  SkBitmap bitmap_from_png = gfx::PNGCodec::Decode(png_data.value());
  std::optional<std::vector<uint8_t>> other_png_data = base::ReadFileToBytes(
      GlicTestDataPath("youtube_favicon_32x32_16bit_fp.png"));
  ASSERT_TRUE(other_png_data.has_value());
  SkBitmap bitmap_from_other_png =
      gfx::PNGCodec::Decode(other_png_data.value());

  EXPECT_FALSE(FaviconEquals(bitmap_from_png, bitmap_from_other_png));
}

TEST(GlicTabDataTest, CompareFaviconImagesWithDifferentSizes) {
  std::optional<std::vector<uint8_t>> png_data =
      base::ReadFileToBytes(GlicTestDataPath("youtube_favicon_32x32.png"));
  ASSERT_TRUE(png_data.has_value());
  SkBitmap bitmap_from_png = gfx::PNGCodec::Decode(png_data.value());
  std::optional<std::vector<uint8_t>> smaller_png_data =
      base::ReadFileToBytes(GlicTestDataPath("youtube_favicon_16x16.png"));
  ASSERT_TRUE(smaller_png_data.has_value());
  SkBitmap bitmap_from_smaller_png =
      gfx::PNGCodec::Decode(smaller_png_data.value());

  EXPECT_FALSE(FaviconEquals(bitmap_from_png, bitmap_from_smaller_png));
}

TEST(GlicTabDataTest, CompareFaviconImagesWithDifferentPixels) {
  std::optional<std::vector<uint8_t>> png_data =
      base::ReadFileToBytes(GlicTestDataPath("youtube_favicon_32x32.png"));
  ASSERT_TRUE(png_data.has_value());
  SkBitmap bitmap_from_png = gfx::PNGCodec::Decode(png_data.value());
  std::optional<std::vector<uint8_t>> different_png_data =
      base::ReadFileToBytes(
          GlicTestDataPath("youtube_favicon_32x32_different_pixels.png"));
  ASSERT_TRUE(different_png_data.has_value());
  SkBitmap bitmap_from_different_png =
      gfx::PNGCodec::Decode(different_png_data.value());

  EXPECT_FALSE(FaviconEquals(bitmap_from_png, bitmap_from_different_png));
}

}  // namespace glic
