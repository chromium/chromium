// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_

#include "build/build_config.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkPixmap.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace cc {

class ImageDecodeCacheUtils {
 public:
  static bool CanResizeF16Image(SkFilterQuality filter_quality) {
#if defined(OS_ANDROID)
    // Return false on Android KitKat or lower if filter quality is medium or
    // high (hence, mipmaps are used), return true otherwise. This is because
    // of skia:8410 which causes a crash when trying to scale a f16 image on
    // these configs. crbug.com/876349
    return (base::android::BuildInfo::GetInstance()->sdk_int() >=
            base::android::SDK_VERSION_LOLLIPOP) ||
           (filter_quality < kMedium_SkFilterQuality);
#else
    return true;
#endif
  }

  static bool ScaleToHalfFloatPixmapUsingN32Intermediate(
      const SkPixmap& source_pixmap,
      SkPixmap* scaled_pixmap,
      SkFilterQuality filter_quality);
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_
