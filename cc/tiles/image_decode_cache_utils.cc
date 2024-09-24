// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_

#include "cc/tiles/image_decode_cache_utils.h"

#include "base/check.h"
#include "cc/paint/paint_flags.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace cc {

namespace {

BASE_FEATURE(kImageDecodeConfigurableFeature,
             "ImageDecodeConfigurableFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_INT(GetDefaultDecodedImageWorkingSetBudgetBytes,
                          kImageDecodeConfigurableFeature,
                          "DefaultDecodedImageWorkingSetBudgetBytes",
                          128 * 1024 * 1024)

#if !BUILDFLAG(IS_ANDROID)

MIRACLE_PARAMETER_FOR_INT(GetDecodedImageWorkingSetBudgetBytesForLowEndDevice,
                          kImageDecodeConfigurableFeature,
                          "DecodedImageWorkingSetBudgetBytesForLowEndDevice",
                          32 * 1024 * 1024)

MIRACLE_PARAMETER_FOR_INT(GetDecodedImageWorkingSetBudgetBytesForAboveThreshold,
                          kImageDecodeConfigurableFeature,
                          "DecodedImageWorkingSetBudgetBytesForAboveThreshold",
                          256 * 1024 * 1024)

MIRACLE_PARAMETER_FOR_INT(GetImageDecodeMemoryThresholdMB,
                          kImageDecodeConfigurableFeature,
                          "ImageDecodeMemoryThresholdMB",
                          4 * 1024)

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// static
bool ImageDecodeCacheUtils::ShouldEvictCaches(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return false;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return true;
  }
  NOTREACHED();
}

// static
size_t ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
    bool for_renderer) {
  size_t decoded_image_working_set_budget_bytes =
      GetDefaultDecodedImageWorkingSetBudgetBytes();
#if !BUILDFLAG(IS_ANDROID)
  if (for_renderer) {
    const bool using_low_memory_policy = base::SysInfo::IsLowEndDevice();
    // If there's over `kImageDecodeMemoryThresholdMB` of RAM, increase the
    // working set size to `kDecodedImageWorkingSetBudgetBytesForAboveThreshold`
    // for both gpu and software.
    if (using_low_memory_policy) {
      decoded_image_working_set_budget_bytes =
          GetDecodedImageWorkingSetBudgetBytesForLowEndDevice();
    } else if (base::SysInfo::AmountOfPhysicalMemoryMB() >=
               GetImageDecodeMemoryThresholdMB()) {
      decoded_image_working_set_budget_bytes =
          GetDecodedImageWorkingSetBudgetBytesForAboveThreshold();
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return decoded_image_working_set_budget_bytes;
}

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
