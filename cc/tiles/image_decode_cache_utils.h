// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_

#include "base/memory/memory_pressure_listener.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPixmap.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace cc {

class ImageDecodeCacheUtils {
 public:
  static bool CanResizeF16Image(PaintFlags::FilterQuality filter_quality) {
#if BUILDFLAG(IS_ANDROID)
    // Return false on Android KitKat or lower if filter quality is medium or
    // high (hence, mipmaps are used), return true otherwise. This is because
    // of skia:8410 which causes a crash when trying to scale a f16 image on
    // these configs. crbug.com/876349
    return (base::android::BuildInfo::GetInstance()->sdk_int() >=
            base::android::SDK_VERSION_LOLLIPOP) ||
           (filter_quality < PaintFlags::FilterQuality::kMedium);
#else
    return true;
#endif
  }

  static bool ScaleToHalfFloatPixmapUsingN32Intermediate(
      const SkPixmap& source_pixmap,
      SkPixmap* scaled_pixmap,
      PaintFlags::FilterQuality filter_quality);

  static bool ShouldEvictCaches(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_H_
