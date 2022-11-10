// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_METRICS_UTIL_ANDROID_H_
#define CHROME_BROWSER_ANDROID_VR_METRICS_UTIL_ANDROID_H_

#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace vr {

// A utility class containing static functions for metrics logging.
class MetricsUtilAndroid {
 public:
  static void LogVrViewerType(gvr::ViewerType viewer_type);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_METRICS_UTIL_ANDROID_H_
