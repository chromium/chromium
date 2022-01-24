// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using DecodeImageCallback = base::OnceCallback<void(gfx::Image)>;

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace ash {
namespace quick_pair {

// The FastPairImageDecoder decodes and returns images for the device used in
// the notifications. FastPairImageDecoder can decode images from either a
// given url or from given bytes of image data.
class FastPairImageDecoder {
 public:
  explicit FastPairImageDecoder(
      std::unique_ptr<image_fetcher::ImageFetcher> fetcher);
  FastPairImageDecoder(const FastPairImageDecoder&) = delete;
  FastPairImageDecoder& operator=(const FastPairImageDecoder&) = delete;
  ~FastPairImageDecoder();

  void DecodeImage(const GURL& image_url,
                   DecodeImageCallback on_image_decoded_callback);

  void DecodeImage(const std::vector<uint8_t>& encoded_image_bytes,
                   DecodeImageCallback on_image_decoded_callback);

 private:
  // ImageDataFetcher callback
  void OnImageDataFetched(
      DecodeImageCallback on_image_decoded_callback,
      const std::string& image_data,
      const image_fetcher::RequestMetadata& request_metadata);

  std::unique_ptr<image_fetcher::ImageFetcher> fetcher_;
  base::WeakPtrFactory<FastPairImageDecoder> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_H_
