// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/in_process_data_decoder.h"

#include "base/timer/elapsed_timer.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace ash {
namespace {

class ImageDecoderImpl : public data_decoder::mojom::ImageDecoder {
 public:
  ImageDecoderImpl() = default;
  ImageDecoderImpl(const ImageDecoderImpl&) = delete;
  ImageDecoderImpl& operator=(const ImageDecoderImpl&) = delete;
  ~ImageDecoderImpl() override = default;

  // data_decoder::mojom::ImageDecoder:
  //
  // |shrink_to_fit|, |max_size_in_bytes|, and |desired_image_frame_size| are
  // left unimplemented for now because there are currently no testing use
  // cases for these. They may be implemented later if required.
  void DecodeImage(mojo_base::BigBuffer encoded_data,
                   data_decoder::mojom::ImageCodec codec,
                   bool shrink_to_fit,
                   int64_t max_size_in_bytes,
                   const gfx::Size& desired_image_frame_size,
                   DecodeImageCallback callback) override {
    ASSERT_TRUE(callback);
    base::ElapsedTimer timer;
    SkBitmap output;
    if (encoded_data.size() == 0) {
      std::move(callback).Run(timer.Elapsed(), output);
      return;
    }

    switch (codec) {
      case data_decoder::mojom::ImageCodec::kDefault: {
        // Only "default" codec currently used in ash/ is jpeg. Others may be
        // added here in the future if necessary.
        std::unique_ptr<SkBitmap> decoded_jpeg =
            gfx::JPEGCodec::Decode(encoded_data.data(), encoded_data.size());
        if (decoded_jpeg) {
          output = std::move(*decoded_jpeg);
        }
        break;
      }
      case data_decoder::mojom::ImageCodec::kPng:
        gfx::PNGCodec::Decode(encoded_data.data(), encoded_data.size(),
                              &output);
        break;
    }
    std::move(callback).Run(timer.Elapsed(), output);
  }

  void DecodeAnimation(mojo_base::BigBuffer encoded_data,
                       bool shrink_to_fit,
                       int64_t max_size_in_bytes,
                       DecodeAnimationCallback callback) override {
    FAIL();
  }
};

}  // namespace

InProcessDataDecoder::InProcessDataDecoder() = default;

InProcessDataDecoder::~InProcessDataDecoder() = default;

std::unique_ptr<data_decoder::mojom::ImageDecoder>
InProcessDataDecoder::CreateCustomImageDecoder() {
  return std::make_unique<ImageDecoderImpl>();
}

}  // namespace ash
