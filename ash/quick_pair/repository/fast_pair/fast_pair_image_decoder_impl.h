// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_IMPL_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_IMPL_H_

#include <memory>

#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "base/functional/callback.h"
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

// The FastPairImageDecoderImpl decodes and returns images for the device used
// in the notifications. FastPairImageDecoderImpl can decode images from either
// a given url or from given bytes of image data.
class FastPairImageDecoderImpl : public FastPairImageDecoder {
 public:
  FastPairImageDecoderImpl();
  FastPairImageDecoderImpl(const FastPairImageDecoderImpl&) = delete;
  FastPairImageDecoderImpl& operator=(const FastPairImageDecoderImpl&) = delete;
  ~FastPairImageDecoderImpl() override;

  void DecodeImageFromUrl(
      const GURL& image_url,
      bool resize_to_notification_size,
      DecodeImageCallback on_image_decoded_callback) override;

  void DecodeImage(const std::vector<uint8_t>& encoded_image_bytes,
                   bool resize_to_notification_size,
                   DecodeImageCallback on_image_decoded_callback) override;

 private:
  // ImageDataFetcher callback
  void OnImageDataFetched(
      DecodeImageCallback on_image_decoded_callback,
      bool resize_to_notification_size,
      const std::string& image_data,
      const image_fetcher::RequestMetadata& request_metadata);

  bool LoadImageFetcher();

  std::unique_ptr<image_fetcher::ImageFetcher> fetcher_;
  base::WeakPtrFactory<FastPairImageDecoderImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_IMPL_H_
