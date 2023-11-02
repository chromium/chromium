// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/metrics_util_android.h"

#include "base/metrics/histogram_functions.h"
#include "device/vr/vr_device.h"

namespace {
device::VrViewerType GetVrViewerType(gvr::ViewerType viewer_type) {
  switch (viewer_type) {
    case gvr::ViewerType::GVR_VIEWER_TYPE_DAYDREAM:
      return device::VrViewerType::GVR_DAYDREAM;
    case gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD:
      return device::VrViewerType::GVR_CARDBOARD;
    default:
      NOTREACHED();
      return device::VrViewerType::GVR_UNKNOWN;
  }
}
}  // anonymous namespace

namespace vr {
void MetricsUtilAndroid::LogVrViewerType(gvr::ViewerType viewer_type) {
  base::UmaHistogramSparse("VRViewerType",
                           static_cast<int>(GetVrViewerType(viewer_type)));
}
}  // namespace vr
