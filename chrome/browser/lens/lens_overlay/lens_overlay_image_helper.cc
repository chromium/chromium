// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_image_helper.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "components/lens/lens_features.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

bool ShouldDownscaleImage(const SkBitmap& image) {
  auto size = gfx::Size(image.width(), image.height());
  // This returns true if the area is larger than the max area AND one of the
  // width OR height exceeds the configured max values.
  return size.GetArea() > lens::features::GetLensOverlayImageMaxArea() &&
         (size.width() > lens::features::GetLensOverlayImageMaxWidth() ||
          size.height() > lens::features::GetLensOverlayImageMaxHeight());
}

gfx::Size GetPreferredImageSize(const SkBitmap& image) {
  double scale = std::min(
      base::ClampDiv(
          static_cast<double>(lens::features::GetLensOverlayImageMaxWidth()),
          image.width()),
      base::ClampDiv(
          static_cast<double>(lens::features::GetLensOverlayImageMaxHeight()),
          image.height()));
  int width = std::clamp<int>(scale * image.width(), 1,
                              lens::features::GetLensOverlayImageMaxWidth());
  int height = std::clamp<int>(scale * image.height(), 1,
                               lens::features::GetLensOverlayImageMaxHeight());
  return gfx::Size(width, height);
}

SkBitmap DownscaleImageIfNeeded(const SkBitmap& image) {
  if (ShouldDownscaleImage(image)) {
    auto preferred_size = GetPreferredImageSize(image);
    return skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, preferred_size.width(),
        preferred_size.height());
  }
  return image;
}

bool EncodeImage(const SkBitmap& image,
                 scoped_refptr<base::RefCountedBytes>* output) {
  *output = base::MakeRefCounted<base::RefCountedBytes>();
  return gfx::JPEGCodec::Encode(
      image, lens::features::GetLensOverlayImageCompressionQuality(),
      &(*output)->data());
}

}  // namespace

namespace lens {

lens::ImageData DownscaleAndEncodeBitmap(const SkBitmap& image) {
  lens::ImageData image_data;
  scoped_refptr<base::RefCountedBytes> data;
  auto resized_bitmap = DownscaleImageIfNeeded(image);
  if (EncodeImage(resized_bitmap, &data)) {
    image_data.mutable_image_metadata()->set_height(resized_bitmap.height());
    image_data.mutable_image_metadata()->set_width(resized_bitmap.width());

    image_data.mutable_payload()->mutable_image_bytes()->assign(data->begin(),
                                                                data->end());
  }
  return image_data;
}

}  // namespace lens
