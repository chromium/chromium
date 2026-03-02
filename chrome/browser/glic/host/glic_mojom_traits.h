// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_MOJOM_TRAITS_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<
    glic::mojom::ScreenshotCollectionOptionsDataView,
    page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions> {
  static uint32_t max_width(const page_content_annotations::ScreenshotOptions::
                                ScreenshotCollectionOptions& in) {
    return in.max_width.value_or(0);
  }
  static uint32_t max_height(const page_content_annotations::ScreenshotOptions::
                                 ScreenshotCollectionOptions& in) {
    return in.max_height.value_or(0);
  }
  static page_content_annotations::ScreenshotOptions::ScreenshotImageFormat
  screenshot_image_format(const page_content_annotations::ScreenshotOptions::
                              ScreenshotCollectionOptions& in) {
    return in.screenshot_image_format.value_or(
        page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::
            kJpeg);
  }
  static page_content_annotations::ScreenshotOptions::
      ScreenshotCompressionQuality
      screenshot_compression_quality(
          const page_content_annotations::ScreenshotOptions::
              ScreenshotCollectionOptions& in) {
    return in.screenshot_compression_quality.value_or(
        page_content_annotations::ScreenshotOptions::
            ScreenshotCompressionQuality::kMedium);
  }
  static bool Read(
      const glic::mojom::ScreenshotCollectionOptionsDataView data,
      page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions*
          out);
};

template <>
struct EnumTraits<
    glic::mojom::ScreenshotImageFormat,
    page_content_annotations::ScreenshotOptions::ScreenshotImageFormat> {
  static glic::mojom::ScreenshotImageFormat ToMojom(
      page_content_annotations::ScreenshotOptions::ScreenshotImageFormat
          screenshot_image_format) {
    switch (screenshot_image_format) {
      case page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::
          kJpeg:
        return glic::mojom::ScreenshotImageFormat::kJpeg;
      case page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::
          kPng:
        return glic::mojom::ScreenshotImageFormat::kPng;
      case page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::
          kWebp:
        return glic::mojom::ScreenshotImageFormat::kWebp;
      default:
        return glic::mojom::ScreenshotImageFormat::kJpeg;
    }
    NOTREACHED();
  }

  static page_content_annotations::ScreenshotOptions::ScreenshotImageFormat
  FromMojom(glic::mojom::ScreenshotImageFormat screenshot_image_format) {
    switch (screenshot_image_format) {
      case glic::mojom::ScreenshotImageFormat::kJpeg:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotImageFormat::kJpeg;
      case glic::mojom::ScreenshotImageFormat::kPng:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotImageFormat::kPng;
      case glic::mojom::ScreenshotImageFormat::kWebp:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotImageFormat::kWebp;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<
    glic::mojom::ScreenshotCompressionQuality,
    page_content_annotations::ScreenshotOptions::ScreenshotCompressionQuality> {
  static glic::mojom::ScreenshotCompressionQuality ToMojom(
      page_content_annotations::ScreenshotOptions::ScreenshotCompressionQuality
          screenshot_compression_quality) {
    switch (screenshot_compression_quality) {
      case page_content_annotations::ScreenshotOptions::
          ScreenshotCompressionQuality::kLow:
        return glic::mojom::ScreenshotCompressionQuality::kLow;
      case page_content_annotations::ScreenshotOptions::
          ScreenshotCompressionQuality::kMedium:
        return glic::mojom::ScreenshotCompressionQuality::kMedium;
      case page_content_annotations::ScreenshotOptions::
          ScreenshotCompressionQuality::kHigh:
        return glic::mojom::ScreenshotCompressionQuality::kHigh;
      case page_content_annotations::ScreenshotOptions::
          ScreenshotCompressionQuality::kNone:
        return glic::mojom::ScreenshotCompressionQuality::kNone;
    }
    NOTREACHED();
  }

  static page_content_annotations::ScreenshotOptions::
      ScreenshotCompressionQuality
      FromMojom(glic::mojom::ScreenshotCompressionQuality
                    screenshot_compression_quality) {
    switch (screenshot_compression_quality) {
      case glic::mojom::ScreenshotCompressionQuality::kLow:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotCompressionQuality::kLow;
      case glic::mojom::ScreenshotCompressionQuality::kMedium:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotCompressionQuality::kMedium;
      case glic::mojom::ScreenshotCompressionQuality::kHigh:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotCompressionQuality::kHigh;
      case glic::mojom::ScreenshotCompressionQuality::kNone:
        return page_content_annotations::ScreenshotOptions::
            ScreenshotCompressionQuality::kNone;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_MOJOM_TRAITS_H_
