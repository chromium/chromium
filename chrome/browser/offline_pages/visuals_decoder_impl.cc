// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/visuals_decoder_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace offline_pages {

namespace {

const gfx::Size kPreferredFaviconSize = gfx::Size(64, 64);

gfx::Image CropSquare(const gfx::Image& image) {
  if (image.IsEmpty())
    return image;

  const gfx::ImageSkia* skimage = image.ToImageSkia();
  gfx::Rect bounds{{0, 0}, skimage->size()};
  int size = std::min(bounds.width(), bounds.height());
  bounds.ClampToCenteredSize({size, size});
  return gfx::Image(gfx::ImageSkiaOperations::CreateTiledImage(
      *skimage, bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

}  // namespace

VisualsDecoderImpl::VisualsDecoderImpl(
    std::unique_ptr<image_fetcher::ImageDecoder> decoder)
    : image_decoder_(std::move(decoder)) {
  CHECK(image_decoder_);
}

VisualsDecoderImpl::~VisualsDecoderImpl() = default;

void VisualsDecoderImpl::DecodeAndCropImage(const std::string& thumbnail_data,
                                            DecodeComplete complete_callback) {
  auto callback = base::BindOnce(
      [](VisualsDecoder::DecodeComplete complete_callback,
         const gfx::Image& image) {
        if (image.IsEmpty()) {
          std::move(complete_callback).Run(image);
          return;
        }
        std::move(complete_callback).Run(CropSquare(image));
      },
      std::move(complete_callback));

  // kPreferredFaviconSize only has an effect for images with multiple frames
  // (.ico) and shouldn't make a difference for thumbnails.
  image_decoder_->DecodeImage(thumbnail_data, kPreferredFaviconSize,
                              /*data_decoder=*/nullptr, std::move(callback));
}

}  // namespace offline_pages
