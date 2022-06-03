// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSE_BITMAPS_HELPER_H_
#define CHROME_BROWSER_ANDROID_COMPOSE_BITMAPS_HELPER_H_

#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"

namespace compose_bitmaps_helper {

// This method composes a list of bitmaps, up to four, into one single bitmap.
std::unique_ptr<SkBitmap> ComposeBitmaps(const std::vector<SkBitmap>& bitmaps,
                                         int desired_size_in_pixel);
SkBitmap ScaleBitmap(int icon_size, const SkBitmap& bitmap);

}  // namespace compose_bitmaps_helper

#endif  // CHROME_BROWSER_ANDROID_COMPOSE_BITMAPS_HELPER_H_
