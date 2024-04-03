// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_image_helper.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace lens {

constexpr int kImageCompressionQuality = 30;
constexpr int kImageMaxArea = 1000000;
constexpr int kImageMaxHeight = 1000;
constexpr int kImageMaxWidth = 1000;

class LensOverlayImageHelperTest : public testing::Test {
 public:
  void SetUp() override {
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {{"image-compression-quality",
          base::StringPrintf("%d", kImageCompressionQuality)},
         {"image-dimensions-max-area", base::StringPrintf("%d", kImageMaxArea)},
         {"image-dimensions-max-height",
          base::StringPrintf("%d", kImageMaxHeight)},
         {"image-dimensions-max-width",
          base::StringPrintf("%d", kImageMaxWidth)}});
  }

  const SkBitmap CreateNonEmptyBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    return bitmap;
  }

  std::string GetJpegBytesForBitmap(const SkBitmap bitmap) {
    std::vector<unsigned char> data;
    gfx::JPEGCodec::Encode(bitmap, kImageCompressionQuality, &data);
    return std::string(data.begin(), data.end());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapMaxSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapSmallSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(/*width=*/100, /*height=*/100);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(bitmap.width(), image_data.image_metadata().width());
  ASSERT_EQ(bitmap.height(), image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapLargeSize) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight * scale);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapHeightTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight * scale);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth / scale, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth / scale, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapWidthTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight / scale);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight / scale, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

}  // namespace lens
