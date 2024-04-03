// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_IMAGE_HELPER_H_
#define CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_IMAGE_HELPER_H_

#include "base/memory/ref_counted_memory.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"

class SkBitmap;

namespace lens {

// Downscales and encodes the provided bitmap and then stores it in a
// lens::ImageData object. Returns an empty object if encoding fails.
// Downscaling only occurs if the bitmap dimensions exceed configured flag
// values.
lens::ImageData DownscaleAndEncodeBitmap(const SkBitmap& image);
}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_IMAGE_HELPER_H_
