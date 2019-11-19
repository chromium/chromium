// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_METRICS_UTIL_ANDROID_H_
#define CHROME_BROWSER_ANDROID_VR_METRICS_UTIL_ANDROID_H_

#include "base/macros.h"

#include "chrome/browser/android/vr/vr_core_info.h"
#include "chrome/browser/vr/ui_suppressed_element.h"
#include "device/vr/vr_device.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace vr {

// A utility class containing static functions for metrics logging.
class MetricsUtilAndroid {
 public:
  // Ensure that this stays in sync with XRRenderPath in enums.xml. Do
  // not reuse or renumber entries.
  enum class XRRenderPath : int {
    kClientWait = 0,
    kGpuFence = 1,
    kSharedBuffer = 2,

    // This must be last.
    kCount
  };

  static void LogGvrVersionForVrViewerType(gvr::ViewerType viewer_type,
                                           const VrCoreInfo& vr_core_info);
  static void LogVrViewerType(gvr::ViewerType viewer_type);

 private:
  static device::VrViewerType GetVrViewerType(gvr::ViewerType viewer_type);

  static bool has_logged_vr_runtime_version_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MetricsUtilAndroid);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_METRICS_UTIL_ANDROID_H_
