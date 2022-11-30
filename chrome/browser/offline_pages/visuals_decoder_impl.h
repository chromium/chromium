// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_VISUALS_DECODER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_VISUALS_DECODER_IMPL_H_

#include <memory>

#include "components/offline_pages/core/visuals_decoder.h"

namespace image_fetcher {
class ImageDecoder;
}  // namespace image_fetcher

namespace offline_pages {

class VisualsDecoderImpl : public VisualsDecoder {
 public:
  explicit VisualsDecoderImpl(
      std::unique_ptr<image_fetcher::ImageDecoder> decoder);
  ~VisualsDecoderImpl() override;

  // Decodes the downloaded image, crops it and re-encodes it as a PNG
  // file to be used as the thumbnail of an offlined suggested article or as the
  // favicon. In the case of .ICO favicons, the frame whose size matches our
  // preferred favicon size (or the next larger one) will be re-encoded.
  // Note: the local decoding in a separate process and local re-encoding as a
  // PNG are important security measures to disarm a potential
  // maliciously-crafted JPEG, which cannot maintain its evil nature after being
  // converted to PNG.
  void DecodeAndCropImage(const std::string& thumbnail_data,
                          DecodeComplete complete_callback) override;

 private:
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_VISUALS_DECODER_IMPL_H_
