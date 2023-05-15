// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"

#include "base/metrics/histogram_functions.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

void RecordUma(GDMPreferCurrentTabResult result) {
  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.PreferCurrentTabFlow.UserInteraction", result);
}

gfx::ImageSkia ScaleBitmap(const SkBitmap& bitmap, gfx::Size size) {
  const gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, size.width(), size.height()),
      gfx::Size(bitmap.info().width(), bitmap.info().height()));

  // TODO(crbug.com/1246835): Consider changing to ResizeMethod::BEST after
  // verifying CPU impact isn't too high.
  const gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia::CreateFromBitmap(bitmap, 1.f),
      skia::ImageOperations::ResizeMethod::RESIZE_GOOD, scaled_rect.size());

  SkBitmap result(*resized.bitmap());

  // Set alpha channel values to 255 for all pixels.
  // TODO(crbug.com/264424): Fix screen/window capturers to capture alpha
  // channel and remove this code. Currently screen/window capturers (at least
  // some implementations) only capture R, G and B channels and set Alpha to 0.
  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3] =
          0xff;
    }
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}
