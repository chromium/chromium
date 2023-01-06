// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

// The FastPairImageDecoder decodes and returns images for the device used in
// the notifications. FastPairImageDecoder can decode images from either a
// given url or from given bytes of image data.
class FastPairImageDecoder {
 public:
  using DecodeImageCallback = base::OnceCallback<void(gfx::Image)>;

  FastPairImageDecoder();
  virtual ~FastPairImageDecoder();

  virtual void DecodeImageFromUrl(
      const GURL& image_url,
      bool resize_to_notification_size,
      DecodeImageCallback on_image_decoded_callback) = 0;

  virtual void DecodeImage(const std::vector<uint8_t>& encoded_image_bytes,
                           bool resize_to_notification_size,
                           DecodeImageCallback on_image_decoded_callback) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FAST_PAIR_IMAGE_DECODER_H_
