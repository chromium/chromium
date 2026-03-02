// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    glic::mojom::ScreenshotCollectionOptionsDataView,
    page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions>::
    Read(const glic::mojom::ScreenshotCollectionOptionsDataView data,
         page_content_annotations::ScreenshotOptions::
             ScreenshotCollectionOptions* out) {
  out->max_width = data.max_width();
  out->max_height = data.max_height();
  page_content_annotations::ScreenshotOptions::ScreenshotImageFormat
      screenshot_image_format;
  if (!data.ReadScreenshotImageFormat(&screenshot_image_format)) {
    return false;
  }
  out->screenshot_image_format = screenshot_image_format;
  page_content_annotations::ScreenshotOptions::ScreenshotCompressionQuality
      screenshot_compression_quality;
  if (!data.ReadScreenshotCompressionQuality(&screenshot_compression_quality)) {
    return false;
  }
  out->screenshot_compression_quality = screenshot_compression_quality;
  return true;
}

}  // namespace mojo
