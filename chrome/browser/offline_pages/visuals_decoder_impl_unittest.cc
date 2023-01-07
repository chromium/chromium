// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/visuals_decoder_impl.h"

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "components/image_fetcher/core/mock_image_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace offline_pages {
namespace {
using testing::_;
using testing::Invoke;

const char kImageData[] = "abc123";

class VisualsDecoderImplTest : public testing::Test {
 public:
  void SetUp() override {
    auto decoder = std::make_unique<image_fetcher::MockImageDecoder>();
    image_decoder = decoder.get();
    visuals_decoder = std::make_unique<VisualsDecoderImpl>(std::move(decoder));
  }
  raw_ptr<image_fetcher::MockImageDecoder> image_decoder;
  std::unique_ptr<VisualsDecoderImpl> visuals_decoder;
};

TEST_F(VisualsDecoderImplTest, Success) {
  const int kImageWidth = 4;
  const int kImageHeight = 2;
  const gfx::Image kDecodedImage =
      gfx::test::CreateImage(kImageWidth, kImageHeight);
  EXPECT_CALL(*image_decoder, DecodeImage(kImageData, _, _, _))
      .WillOnce(testing::WithArg<3>(
          Invoke([&](image_fetcher::ImageDecodedCallback callback) {
            std::move(callback).Run(kDecodedImage);
          })));
  base::MockCallback<VisualsDecoder::DecodeComplete> complete_callback;
  EXPECT_CALL(complete_callback, Run(_))
      .WillOnce(Invoke([&](const gfx::Image& image) {
        EXPECT_EQ(kImageHeight, image.Width());
        EXPECT_EQ(kImageHeight, image.Height());
      }));
  visuals_decoder->DecodeAndCropImage(kImageData, complete_callback.Get());
}

TEST_F(VisualsDecoderImplTest, DecodeFail) {
  const gfx::Image kDecodedImage = gfx::Image();
  EXPECT_CALL(*image_decoder, DecodeImage(kImageData, _, _, _))
      .WillOnce(testing::WithArg<3>(
          Invoke([&](image_fetcher::ImageDecodedCallback callback) {
            std::move(callback).Run(kDecodedImage);
          })));
  base::MockCallback<VisualsDecoder::DecodeComplete> complete_callback;
  EXPECT_CALL(complete_callback, Run(_))
      .WillOnce(Invoke(
          [](const gfx::Image& image) { EXPECT_TRUE(image.IsEmpty()); }));
  visuals_decoder->DecodeAndCropImage(kImageData, complete_callback.Get());
}

}  // namespace
}  // namespace offline_pages
