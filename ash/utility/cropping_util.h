// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_CROPPING_UTIL_H_
#define ASH_UTILITY_CROPPING_UTIL_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {

// Crops an image such that its aspect ratio matches that of a target size, but
// does not perform any "scaling". The cropping is calculated with the image
// and the target rect "center-aligned". The image dimension with the smaller
// (target_size / original_size) ratio gets cropped.
//
// A visual example with a portrait image whose dimensions exceeds a landscape
// target size:
//
// Before:
//
//         Portrait Image
// +---------------------------+
// |                           |
// |                           |
// |                           |
// |                           |
// |      Landscape Target     |
// |    +-----------------+    |
// |    |                 |    |
// |    |                 |    |
// |    |                 |    |
// |    |                 |    |
// |    |                 |    |
// |    +-----------------+    |
// |                           |
// |                           |
// |                           |
// |                           |
// |                           |
// +---------------------------+
//
// After (ok, maybe it's not the exact same aspect ratio, but you get the idea):
//
//         Cropped Image
// +---------------------------+
// |                           |
// |      Landscape Target     |
// |    +-----------------+    |
// |    |                 |    |
// |    |                 |    |
// |    |                 |    |
// |    |                 |    |
// |    |                 |    |
// |    +-----------------+    |
// |                           |
// |                           |
// +---------------------------+
//
// The ultimate result is always a cropped image whose aspect ratio matches that
// of the target size. Therefore, the cropped image can subsequently be scaled
// up or down to match the dimensions of the target size.
//
// There are no requirements for the image and target dimensions other than that
// they're non-empty. This function cannot fail; the returned SkBitmap is always
// non-null and points to ref-counted pixel memory shared with |image|.
ASH_EXPORT SkBitmap CenterCropImage(const SkBitmap& image,
                                    const gfx::Size& target_size);

}  // namespace ash

#endif  // ASH_UTILITY_CROPPING_UTIL_H_
