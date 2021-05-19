// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

// A request for decoding an image.
class ImageDecoderImpl::DecodeImageRequest
    : public ::ImageDecoder::ImageRequest {
 public:
  DecodeImageRequest(ImageDecoderImpl* decoder,
                     image_fetcher::ImageDecodedCallback callback)
      : decoder_(decoder), callback_(std::move(callback)) {}
  ~DecodeImageRequest() override {}

 private:
  // Runs the callback and remove the request from the internal request queue.
  void RunCallbackAndRemoveRequest(const gfx::Image& image);

  // Methods inherited from ImageDecoder::ImageRequest

  void OnImageDecoded(const SkBitmap& decoded_image) override;

  void OnDecodeImageFailed() override;

  ImageDecoderImpl* decoder_;

  // The callback to call after the request completed.
  image_fetcher::ImageDecodedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DecodeImageRequest);
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
    image_fetcher::ImageDecodedCallback callback) {
  std::unique_ptr<DecodeImageRequest> decode_image_request(
      new DecodeImageRequest(this, std::move(callback)));

  ::ImageDecoder::StartWithOptions(
      decode_image_request.get(),
      std::vector<uint8_t>(image_data.begin(), image_data.end()),
      ::ImageDecoder::DEFAULT_CODEC,
      /*shrink_to_fit=*/false, desired_image_frame_size);

  decode_image_requests_.push_back(std::move(decode_image_request));
}

void ImageDecoderImpl::RemoveDecodeImageRequest(DecodeImageRequest* request) {
  // Remove the finished request from the request queue.
  auto request_it =
      std::find_if(decode_image_requests_.begin(), decode_image_requests_.end(),
                   [request](const std::unique_ptr<DecodeImageRequest>& r) {
                     return r.get() == request;
                   });
  DCHECK(request_it != decode_image_requests_.end());
  decode_image_requests_.erase(request_it);
}
