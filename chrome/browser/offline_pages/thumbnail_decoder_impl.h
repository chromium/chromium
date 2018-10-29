// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_THUMBNAIL_DECODER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_THUMBNAIL_DECODER_IMPL_H_

#include <memory>

#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "components/offline_pages/core/thumbnail_decoder.h"

namespace offline_pages {

// Decodes the downloaded JPEG image, crops it and re-encodes it as a PNG
// file to be used as the thumbnail of an offlined suggested article.
// Note: the local decoding in a separate process and local re-encoding as a PNG
// are important security measures to disarm a potential maliciously-crafted
// JPEG, which cannot maintain its evil nature after being converted to PNG.
class ThumbnailDecoderImpl : public ThumbnailDecoder {
 public:
  explicit ThumbnailDecoderImpl(
      std::unique_ptr<image_fetcher::ImageDecoder> decoder);
  ~ThumbnailDecoderImpl() override;

  void DecodeAndCropThumbnail(const std::string& thumbnail_data,
                              DecodeComplete complete_callback) override;

 private:
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_THUMBNAIL_DECODER_IMPL_H_
