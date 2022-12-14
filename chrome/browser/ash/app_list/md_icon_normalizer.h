// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_MD_ICON_NORMALIZER_H_
#define CHROME_BROWSER_ASH_APP_LIST_MD_ICON_NORMALIZER_H_

#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace app_list {

// Returns the padding that has to be added around the original content of the
// bitmap in order to comply with Material Design guidelines for icons.
// (https://material.io/design/iconography/product-icons.html).
gfx::Size GetMdIconPadding(const SkBitmap& bitmap,
                           const gfx::Size& required_size);

// Resize the icon and add padding. If padding is zero and the bitmap already
// has correct size, it will be left unchanged.
void MaybeResizeAndPad(const gfx::Size& required_size,
                       const gfx::Size& padding,
                       SkBitmap* bitmap_out);

// Resize the icon image to the required size and add padding to ensure
// compliance with Material Design guidelines
// (https://material.io/design/iconography/product-icons.html).
// This function combines resizing and downscaling into a single step in order
// to avoid repetitive downsampling that causes image artifacts.
// An icon with correct size and padding will be left unchanged.
void MaybeResizeAndPadIconForMd(const gfx::Size& required_size_dip,
                                gfx::ImageSkia* icon_out);

// Returns the scale to be applied to the bitmap so that it complies with the
// Material Design guidelines for icons. The returned scale is always <= 1.
float GetMdIconScaleForTest(const SkBitmap& bitmap);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_MD_ICON_NORMALIZER_H_
