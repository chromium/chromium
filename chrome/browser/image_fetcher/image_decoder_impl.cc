// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_fetcher/image_decoder_impl.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

// A request for decoding an image.
class ImageDecoderImpl::DecodeImageRequest
    : public ::ImageDecoder::ImageRequest {
 public:
  DecodeImageRequest(ImageDecoderImpl* decoder,
                     data_decoder::DataDecoder* data_decoder,
                     image_fetcher::ImageDecodedCallback callback)
      : ImageRequest(data_decoder),
        decoder_(decoder),
        callback_(std::move(callback)) {}

  DecodeImageRequest(const DecodeImageRequest&) = delete;
  DecodeImageRequest& operator=(const DecodeImageRequest&) = delete;

  ~DecodeImageRequest() override {}

 private:
  // Runs the callback and remove the request from the internal request queue.
  void RunCallbackAndRemoveRequest(const gfx::Image& image);

  // Methods inherited from ImageDecoder::ImageRequest

  void OnImageDecoded(const SkBitmap& decoded_image) override;

  void OnDecodeImageFailed() override;

  raw_ptr<ImageDecoderImpl> decoder_;

  // The callback to call after the request completed.
  image_fetcher::ImageDecodedCallback callback_;
};

void ImageDecoderImpl::DecodeImageRequest::OnImageDecoded(
    const SkBitmap& decoded_bitmap) {
  // TODO(markusheintz): Check whether high res displays that require 2x and 3x
  // image versions need to be supported here.
  gfx::Image image(gfx::Image::CreateFrom1xBitmap(decoded_bitmap));
  RunCallbackAndRemoveRequest(image);
}

// Called when decoding image failed.
void ImageDecoderImpl::DecodeImageRequest::OnDecodeImageFailed() {
  RunCallbackAndRemoveRequest(gfx::Image());
}

void ImageDecoderImpl::DecodeImageRequest::RunCallbackAndRemoveRequest(
    const gfx::Image& image) {
  std::move(callback_).Run(image);

  // This must be the last line in the method body.
  decoder_->RemoveDecodeImageRequest(this);
}

ImageDecoderImpl::ImageDecoderImpl() {}

ImageDecoderImpl::~ImageDecoderImpl() {}

void ImageDecoderImpl::DecodeImage(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    data_decoder::DataDecoder* data_decoder,
    image_fetcher::ImageDecodedCallback callback) {
  std::unique_ptr<DecodeImageRequest> decode_image_request(
      new DecodeImageRequest(this, data_decoder, std::move(callback)));

  ::ImageDecoder::StartWithOptions(
      decode_image_request.get(), image_data, ::ImageDecoder::DEFAULT_CODEC,
      /*shrink_to_fit=*/false, desired_image_frame_size);

  decode_image_requests_.push_back(std::move(decode_image_request));
}

void ImageDecoderImpl::RemoveDecodeImageRequest(DecodeImageRequest* request) {
  // Remove the finished request from the request queue.
  auto request_it =
      base::ranges::find(decode_image_requests_, request,
                         &std::unique_ptr<DecodeImageRequest>::get);
  CHECK(request_it != decode_image_requests_.end(), base::NotFatalUntil::M130);
  decode_image_requests_.erase(request_it);
}
