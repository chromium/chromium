// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_image_decoder.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/mojom/cbor_parser.mojom.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// A test image decoder that allows the caller to pass in arbitrary callbacks
// that generate images that will be returned in the respective Decode*Callback.
class ImageDecoderImpl : public data_decoder::mojom::ImageDecoder {
 public:
  ImageDecoderImpl(TestImageDecoder::AnimationCallback animation_callback,
                   TestImageDecoder::ImageCallback image_callback)
      : animation_callback_(std::move(animation_callback)),
        image_callback_(std::move(image_callback)) {}
  ImageDecoderImpl(const ImageDecoderImpl&) = delete;
  ImageDecoderImpl& operator=(const ImageDecoderImpl&) = delete;
  ~ImageDecoderImpl() override = default;

  // data_decoder::mojom::ImageDecoder:
  void DecodeImage(mojo_base::BigBuffer encoded_data,
                   data_decoder::mojom::ImageCodec codec,
                   bool shrink_to_fit,
                   int64_t max_size_in_bytes,
                   const gfx::Size& desired_image_frame_size,
                   DecodeImageCallback callback) override {
    std::move(callback).Run(base::TimeDelta(), image_callback_.Run());
  }

  void DecodeAnimation(mojo_base::BigBuffer encoded_data,
                       bool shrink_to_fit,
                       int64_t max_size_in_bytes,
                       DecodeAnimationCallback callback) override {
    std::move(callback).Run(animation_callback_.Run());
  }

 private:
  TestImageDecoder::AnimationCallback animation_callback_;
  TestImageDecoder::ImageCallback image_callback_;
};

}  // namespace

// Instantiates and binds the above `ImageDecoderImpl` to handle image decoding,
// passing the given callbacks along to generate test images.
// Everything not related to image decoding is left unimplemented.
class TestImageDecoder::DataDecoderServiceImpl
    : public data_decoder::mojom::DataDecoderService {
 public:
  DataDecoderServiceImpl(TestImageDecoder::AnimationCallback animation_callback,
                         TestImageDecoder::ImageCallback image_callback)
      : animation_callback_(std::move(animation_callback)),
        image_callback_(std::move(image_callback)) {}
  DataDecoderServiceImpl(const DataDecoderServiceImpl&) = delete;
  DataDecoderServiceImpl& operator=(const DataDecoderServiceImpl&) = delete;
  ~DataDecoderServiceImpl() override = default;

  // data_decoder::mojom::DataDecoderService:
  void BindImageDecoder(mojo::PendingReceiver<data_decoder::mojom::ImageDecoder>
                            receiver) override {
    mojo::MakeSelfOwnedReceiver(std::make_unique<ImageDecoderImpl>(
                                    animation_callback_, image_callback_),
                                std::move(receiver));
  }
  void BindJsonParser(mojo::PendingReceiver<data_decoder::mojom::JsonParser>
                          receiver) override {
    FAIL();
  }
  void BindStructuredHeadersParser(
      mojo::PendingReceiver<data_decoder::mojom::StructuredHeadersParser>
          receiver) override {
    FAIL();
  }
  void BindXmlParser(
      mojo::PendingReceiver<data_decoder::mojom::XmlParser> receiver) override {
    FAIL();
  }
  void BindWebBundleParserFactory(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver) override {
    FAIL();
  }
  void BindGzipper(
      mojo::PendingReceiver<data_decoder::mojom::Gzipper> receiver) override {
    FAIL();
  }
  void BindCborParser(mojo::PendingReceiver<data_decoder::mojom::CborParser>
                          receiver) override {
    FAIL();
  }
  void BindBleScanParser(
      mojo::PendingReceiver<data_decoder::mojom::BleScanParser> receiver)
      override {
    FAIL();
  }

 private:
  const TestImageDecoder::AnimationCallback animation_callback_;
  const TestImageDecoder::ImageCallback image_callback_;
};

TestImageDecoder::TestImageDecoder(AnimationCallback animation_callback,
                                   ImageCallback image_callback)
    : service_(std::make_unique<DataDecoderServiceImpl>(
          std::move(animation_callback),
          std::move(image_callback))) {
  ServiceProvider::Set(this);
}

TestImageDecoder::~TestImageDecoder() {
  ServiceProvider::Set(nullptr);
}

void TestImageDecoder::BindDataDecoderService(
    mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver) {
  receivers_.Add(service_.get(), std::move(receiver));
}

}  // namespace ash
