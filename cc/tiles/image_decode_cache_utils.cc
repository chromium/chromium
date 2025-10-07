// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_

#include "cc/tiles/image_decode_cache_utils.h"

#include "base/byte_count.h"
#include "base/check.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace cc {

// static
bool ImageDecodeCacheUtils::ShouldEvictCaches(
    base::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
      return false;
    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return true;
  }
  NOTREACHED();
}

// static
size_t ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
    bool for_renderer) {
  base::ByteCount decoded_image_working_set_budget = base::MiB(128);
#if !BUILDFLAG(IS_ANDROID)
  if (for_renderer) {
    const bool using_low_memory_policy = base::SysInfo::IsLowEndDevice();
    // If there's over 4GB of RAM, increase the working set size to 256MB for
    // both gpu and software.
    constexpr base::ByteCount kImageDecodeMemoryThreshold = base::GiB(4);
    if (using_low_memory_policy) {
      decoded_image_working_set_budget = base::MiB(32);
    } else if (base::SysInfo::AmountOfPhysicalMemory() >=
               kImageDecodeMemoryThreshold) {
      decoded_image_working_set_budget = base::MiB(256);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return decoded_image_working_set_budget.InBytesUnsigned();
}

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
