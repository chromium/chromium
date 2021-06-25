// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMAGE_UTIL_H_
#define ASH_PUBLIC_CPP_IMAGE_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace ash {
namespace image_util {

// Returns a `gfx::ImageSkia` of the specified `size` which draws nothing.
ASH_PUBLIC_EXPORT gfx::ImageSkia CreateEmptyImage(const gfx::Size& size);

}  // namespace image_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMAGE_UTIL_H_