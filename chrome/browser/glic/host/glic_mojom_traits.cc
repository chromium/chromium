// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_mojom_traits.h"

#include "base/notreached.h"

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

// static
bool StructTraits<glic::mojom::DriveFileSelectionDataView,
                  glic::GlicDriveFileSelection>::
    Read(glic::mojom::DriveFileSelectionDataView data,
         glic::GlicDriveFileSelection* out) {
  if (!data.ReadId(&out->id) || !data.ReadTitle(&out->title) ||
      !data.ReadMimeType(&out->mime_type) ||
      !data.ReadResourceKey(&out->resource_key)) {
    return false;
  }
  out->size_bytes = data.size_bytes();
  return true;
}

// static
glic::mojom::DrivePickerErrorReason EnumTraits<
    glic::mojom::DrivePickerErrorReason,
    glic::GlicDrivePickerError>::ToMojom(glic::GlicDrivePickerError input) {
  switch (input) {
    case glic::GlicDrivePickerError::kUnknown:
      return glic::mojom::DrivePickerErrorReason::kUnknown;
    case glic::GlicDrivePickerError::kWebSessionSignedOut:
      return glic::mojom::DrivePickerErrorReason::kWebSessionSignedOut;
    case glic::GlicDrivePickerError::kChromeSignInRequired:
      return glic::mojom::DrivePickerErrorReason::kChromeSignInRequired;
    case glic::GlicDrivePickerError::kAccountMismatch:
      return glic::mojom::DrivePickerErrorReason::kAccountMismatch;
    case glic::GlicDrivePickerError::kAlreadyActive:
      return glic::mojom::DrivePickerErrorReason::kAlreadyActive;
    case glic::GlicDrivePickerError::kNoAnchorWindow:
      return glic::mojom::DrivePickerErrorReason::kNoAnchorWindow;
    case glic::GlicDrivePickerError::kLoadFailed:
      return glic::mojom::DrivePickerErrorReason::kLoadFailed;
    case glic::GlicDrivePickerError::kDisabledByPolicy:
      return glic::mojom::DrivePickerErrorReason::kDisabledByPolicy;
    case glic::GlicDrivePickerError::kUnsupported:
      return glic::mojom::DrivePickerErrorReason::kUnsupported;
  }
  NOTREACHED();
}

// static
glic::GlicDrivePickerError
EnumTraits<glic::mojom::DrivePickerErrorReason, glic::GlicDrivePickerError>::
    FromMojom(glic::mojom::DrivePickerErrorReason input) {
  switch (input) {
    case glic::mojom::DrivePickerErrorReason::kUnknown:
      return glic::GlicDrivePickerError::kUnknown;
    case glic::mojom::DrivePickerErrorReason::kWebSessionSignedOut:
      return glic::GlicDrivePickerError::kWebSessionSignedOut;
    case glic::mojom::DrivePickerErrorReason::kChromeSignInRequired:
      return glic::GlicDrivePickerError::kChromeSignInRequired;
    case glic::mojom::DrivePickerErrorReason::kAccountMismatch:
      return glic::GlicDrivePickerError::kAccountMismatch;
    case glic::mojom::DrivePickerErrorReason::kAlreadyActive:
      return glic::GlicDrivePickerError::kAlreadyActive;
    case glic::mojom::DrivePickerErrorReason::kNoAnchorWindow:
      return glic::GlicDrivePickerError::kNoAnchorWindow;
    case glic::mojom::DrivePickerErrorReason::kLoadFailed:
      return glic::GlicDrivePickerError::kLoadFailed;
    case glic::mojom::DrivePickerErrorReason::kDisabledByPolicy:
      return glic::GlicDrivePickerError::kDisabledByPolicy;
    case glic::mojom::DrivePickerErrorReason::kUnsupported:
      return glic::GlicDrivePickerError::kUnsupported;
  }
  NOTREACHED();
}

}  // namespace mojo
