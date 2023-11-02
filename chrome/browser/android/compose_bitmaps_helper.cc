// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compose_bitmaps_helper.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace compose_bitmaps_helper {

// Ratio of icon size to the amount of padding between the icons.
const int kIconPaddingScale = 8;

std::unique_ptr<SkBitmap> ComposeBitmaps(const std::vector<SkBitmap>& bitmaps,
                                         int desired_size_in_pixel) {
  int num_icons = bitmaps.size();
  DVLOG(1) << "num icons: " << num_icons;
  if (num_icons == 0) {
    return nullptr;
  }

  DVLOG(1) << "desired_size_in_pixel: " << desired_size_in_pixel;
  int icon_padding_pixel_size = desired_size_in_pixel / kIconPaddingScale;

  // Offset to write icons out of frame due to padding.
  int icon_write_offset = icon_padding_pixel_size / 2;

  SkBitmap composite_bitmap;
  SkImageInfo image_info =
      bitmaps[0]
          .info()
          .makeWH(desired_size_in_pixel, desired_size_in_pixel)
          .makeAlphaType(kPremul_SkAlphaType);

  composite_bitmap.setInfo(image_info);
  composite_bitmap.allocPixels();

  int icon_size = desired_size_in_pixel / 2;

  // draw icons in correct areas
  switch (num_icons) {
    case 1: {
      // Centered.
      SkBitmap scaledBitmap = ScaleBitmap(icon_size, bitmaps[0]);
      if (scaledBitmap.empty()) {
        return nullptr;
      }
      composite_bitmap.writePixels(
          scaledBitmap.pixmap(),
          ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset,
          ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset);
      break;
    }
    case 2: {
      // Side by side.
      for (int i = 0; i < 2; i++) {
        SkBitmap scaledBitmap = ScaleBitmap(icon_size, bitmaps[i]);
        if (scaledBitmap.empty()) {
          return nullptr;
        }
        composite_bitmap.writePixels(
            scaledBitmap.pixmap(),
            (i * (icon_size + icon_padding_pixel_size)) - icon_write_offset,
            ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset);
      }
      break;
    }
    case 3: {
      // Two on top, one on bottom.
      for (int i = 0; i < 3; i++) {
        SkBitmap scaledBitmap = ScaleBitmap(icon_size, bitmaps[i]);
        if (scaledBitmap.empty()) {
          return nullptr;
        }
        switch (i) {
          case 0:
            composite_bitmap.writePixels(
                scaledBitmap.pixmap(), -icon_write_offset, -icon_write_offset);
            break;
          case 1:
            composite_bitmap.writePixels(
                scaledBitmap.pixmap(),
                (icon_size + icon_padding_pixel_size) - icon_write_offset,
                -icon_write_offset);
            break;
          default:
            composite_bitmap.writePixels(
                scaledBitmap.pixmap(),
                ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset,
                (icon_size + icon_padding_pixel_size) - icon_write_offset);
            break;
        }
      }
      break;
    }
    case 4: {
      // One in each corner.
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
          int index = i + 2 * j;
          SkBitmap scaledBitmap = ScaleBitmap(icon_size, bitmaps[index]);
          if (scaledBitmap.empty()) {
            return nullptr;
          }
          composite_bitmap.writePixels(
              scaledBitmap.pixmap(),
              (j * (icon_size + icon_padding_pixel_size)) - icon_write_offset,
              (i * (icon_size + icon_padding_pixel_size)) - icon_write_offset);
        }
      }
      break;
    }
    default:
      DLOG(ERROR) << "Invalid number of icons to combine: " << bitmaps.size();
      return nullptr;
  }

  return std::make_unique<SkBitmap>(composite_bitmap);
}

SkBitmap ScaleBitmap(int icon_size, const SkBitmap& bitmap) {
  SkBitmap temp_bitmap;
  SkImageInfo scaledIconInfo = bitmap.info().makeWH(icon_size, icon_size);
  temp_bitmap.setInfo(scaledIconInfo);
  temp_bitmap.allocPixels();
  const SkSamplingOptions mitchellCubic({1.0f/3, 1.0f/3});
  bool did_scale =
      bitmap.pixmap().scalePixels(temp_bitmap.pixmap(), mitchellCubic);
  if (!did_scale) {
    DLOG(ERROR) << "Unable to scale icon";
    return SkBitmap();
  }
  return temp_bitmap;
}

}  // namespace compose_bitmaps_helper
