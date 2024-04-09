// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_image_helper.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "components/lens/lens_features.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

bool ShouldDownscaleSize(const gfx::Size& size) {
  // This returns true if the area is larger than the max area AND one of the
  // width OR height exceeds the configured max values.
  return size.GetArea() > lens::features::GetLensOverlayImageMaxArea() &&
         (size.width() > lens::features::GetLensOverlayImageMaxWidth() ||
          size.height() > lens::features::GetLensOverlayImageMaxHeight());
}

double GetPreferredScale(const gfx::Size& original_size) {
  return std::min(
      base::ClampDiv(
          static_cast<double>(lens::features::GetLensOverlayImageMaxWidth()),
          original_size.width()),
      base::ClampDiv(
          static_cast<double>(lens::features::GetLensOverlayImageMaxHeight()),
          original_size.height()));
}

gfx::Size GetPreferredSize(const gfx::Size& original_size) {
  double scale = GetPreferredScale(original_size);
  int width = std::clamp<int>(scale * original_size.width(), 1,
                              lens::features::GetLensOverlayImageMaxWidth());
  int height = std::clamp<int>(scale * original_size.height(), 1,
                               lens::features::GetLensOverlayImageMaxHeight());
  return gfx::Size(width, height);
}

SkBitmap DownscaleImageIfNeeded(const SkBitmap& image) {
  auto size = gfx::Size(image.width(), image.height());
  if (ShouldDownscaleSize(size)) {
    auto preferred_size = GetPreferredSize(size);
    return skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, preferred_size.width(),
        preferred_size.height());
  }
  return image;
}

SkBitmap CropAndDownscaleImageIfNeeded(const SkBitmap& image,
                                       gfx::Rect region) {
  auto full_image_size = gfx::Size(image.width(), image.height());
  auto region_size = gfx::Size(region.width(), region.height());
  if (ShouldDownscaleSize(region_size)) {
    double scale = GetPreferredScale(region_size);
    auto downscaled_region_size = GetPreferredSize(region_size);
    int scaled_full_image_width =
        std::max<int>(scale * full_image_size.width(), 1);
    int scaled_full_image_height =
        std::max<int>(scale * full_image_size.height(), 1);
    int scaled_x = int(scale * region.x());
    int scaled_y = int(scale * region.y());

    SkIRect dest_subset = {scaled_x, scaled_y,
                           scaled_x + downscaled_region_size.width(),
                           scaled_y + downscaled_region_size.height()};
    return skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, scaled_full_image_width,
        scaled_full_image_height, dest_subset);
  }

  SkIRect dest_subset = {region.x(), region.y(), region.x() + region.width(),
                         region.y() + region.height()};
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, image.width(), image.height(),
      dest_subset);
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

std::optional<lens::ImageCrop> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::mojom::CenterRotatedBoxPtr region) {
  if (!region) {
    return std::nullopt;
  }

  bool use_normalized_coordinates =
      region->coordinate_type ==
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  double x_scale = use_normalized_coordinates ? image.width() : 1;
  double y_scale = use_normalized_coordinates ? image.height() : 1;
  gfx::Rect region_rect(
      static_cast<int>((region->box.x() - 0.5 * region->box.width()) * x_scale),
      static_cast<int>((region->box.y() - 0.5 * region->box.height()) *
                       y_scale),
      std::max<int>(1, region->box.width() * x_scale),
      std::max<int>(1, region->box.height() * y_scale));

  lens::ImageCrop image_crop;
  scoped_refptr<base::RefCountedBytes> data;
  auto region_bitmap = CropAndDownscaleImageIfNeeded(image, region_rect);
  if (EncodeImage(region_bitmap, &data)) {
    auto* mutable_zoomed_crop = image_crop.mutable_zoomed_crop();
    mutable_zoomed_crop->set_parent_height(image.height());
    mutable_zoomed_crop->set_parent_width(image.width());
    double scale = static_cast<double>(region_bitmap.width()) /
                   static_cast<double>(region_rect.width());
    mutable_zoomed_crop->set_zoom(scale);
    mutable_zoomed_crop->mutable_crop()->set_center_x(
        region_rect.CenterPoint().x());
    mutable_zoomed_crop->mutable_crop()->set_center_y(
        region_rect.CenterPoint().y());
    mutable_zoomed_crop->mutable_crop()->set_width(region_rect.width());
    mutable_zoomed_crop->mutable_crop()->set_height(region_rect.height());
    mutable_zoomed_crop->mutable_crop()->set_coordinate_type(
        lens::CoordinateType::IMAGE);

    image_crop.mutable_image()->mutable_image_content()->assign(data->begin(),
                                                                data->end());
  }
  return image_crop;
}

}  // namespace lens
