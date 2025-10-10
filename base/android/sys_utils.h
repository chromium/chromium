// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SYS_UTILS_H_
#define BASE_ANDROID_SYS_UTILS_H_

#include "base/base_export.h"

namespace base::android {

// Returns the RAM thresholds below which a device is considered low-RAM,
// obtained from a feature param
BASE_EXPORT int GetCachedLowMemoryDeviceThresholdMb();

}  // namespace base::android

#endif  // BASE_ANDROID_SYS_UTILS_H_
