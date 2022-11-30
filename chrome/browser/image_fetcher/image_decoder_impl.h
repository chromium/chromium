// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_FETCHER_IMAGE_DECODER_IMPL_H_
#define CHROME_BROWSER_IMAGE_FETCHER_IMAGE_DECODER_IMPL_H_

#include <memory>
#include <vector>

#include "chrome/browser/image_decoder/image_decoder.h"
#include "components/image_fetcher/core/image_decoder.h"

// image_fetcher::ImageDecoder implementation.
class ImageDecoderImpl : public image_fetcher::ImageDecoder {
 public:
  ImageDecoderImpl();

  ImageDecoderImpl(const ImageDecoderImpl&) = delete;
  ImageDecoderImpl& operator=(const ImageDecoderImpl&) = delete;

  ~ImageDecoderImpl() override;

  void DecodeImage(const std::string& image_data,
                   const gfx::Size& desired_image_frame_size,
                   data_decoder::DataDecoder* data_decoder,
                   image_fetcher::ImageDecodedCallback callback) override;

 private:
  class DecodeImageRequest;

  // Removes the passed image decode |request| from the internal request queue.
  void RemoveDecodeImageRequest(DecodeImageRequest* request);

  // All active image decoding requests.
  std::vector<std::unique_ptr<DecodeImageRequest>> decode_image_requests_;
};

#endif  // CHROME_BROWSER_IMAGE_FETCHER_IMAGE_DECODER_IMPL_H_
