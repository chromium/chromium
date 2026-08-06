// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

gfx::ImageSkia ScaleBitmap(const SkBitmap& bitmap, gfx::Size size) {
  const gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, size.width(), size.height()),
      gfx::Size(bitmap.info().width(), bitmap.info().height()));

  // TODO(crbug.com/40789487): Consider changing to ResizeMethod::BEST after
  // verifying CPU impact isn't too high.
  const gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia::CreateFromBitmap(bitmap, 1.f),
      skia::ImageOperations::ResizeMethod::RESIZE_GOOD, scaled_rect.size());

  SkBitmap result(*resized.bitmap());

  // Set alpha channel values to 255 for all pixels.
  // TODO(crbug.com/41029106): Fix screen/window capturers to capture alpha
  // channel and remove this code. Currently screen/window capturers (at least
  // some implementations) only capture R, G and B channels and set Alpha to 0.
  // ImageSkiaOperations::CreateResizedImage delegates to
  // ImageOperations::Resize, which either returns an empty bitmap on failure
  // or guarantees kN32_SkColorType (4 bytes per pixel).
  CHECK(result.drawsNothing() || result.bytesPerPixel() == 4);
  const size_t bytes_per_pixel = static_cast<size_t>(result.bytesPerPixel());
  constexpr size_t kAlphaChannelByteOffset = 3;

  // SAFETY: SkBitmap::computeByteSize() returns the total allocated byte size
  // of the pixel memory buffer returned by getPixels().
  auto pixels_span =
      UNSAFE_BUFFERS(base::span(reinterpret_cast<uint8_t*>(result.getPixels()),
                                result.computeByteSize()));
  for (size_t i = kAlphaChannelByteOffset; i < pixels_span.size();
       i += bytes_per_pixel) {
    pixels_span[i] = 0xff;
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

content::DesktopMediaID::Type AsDesktopMediaIdType(
    DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return content::DesktopMediaID::Type::TYPE_SCREEN;
    case DesktopMediaList::Type::kWindow:
      return content::DesktopMediaID::Type::TYPE_WINDOW;
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      return content::DesktopMediaID::Type::TYPE_WEB_CONTENTS;
    case DesktopMediaList::Type::kNone:
      return content::DesktopMediaID::Type::TYPE_NONE;
  }
  NOTREACHED();
}

DesktopMediaList::Type AsDesktopMediaListType(
    content::DesktopMediaID::Type type) {
  switch (type) {
    case content::DesktopMediaID::Type::TYPE_SCREEN:
      return DesktopMediaList::Type::kScreen;
    case content::DesktopMediaID::Type::TYPE_WINDOW:
      return DesktopMediaList::Type::kWindow;
    case content::DesktopMediaID::Type::TYPE_WEB_CONTENTS:
      return DesktopMediaList::Type::kWebContents;
    case content::DesktopMediaID::Type::TYPE_NONE:
      return DesktopMediaList::Type::kNone;
  }
  NOTREACHED();
}
