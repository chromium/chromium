// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_TYPE_CONVERTERS_H_

#include "chrome/browser/android/vr/arcore_device/arcore_sdk.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/transform.h"

namespace mojo {

template <>
struct TypeConverter<device::mojom::XRPlaneOrientation, ArPlaneType> {
  static device::mojom::XRPlaneOrientation Convert(ArPlaneType plane_type);
};

template <>
struct TypeConverter<gfx::Transform, device::mojom::VRPosePtr> {
  static gfx::Transform Convert(const device::mojom::VRPosePtr& pose);
};

template <>
struct TypeConverter<gfx::Transform, device::mojom::PosePtr> {
  static gfx::Transform Convert(const device::mojom::PosePtr& pose);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_TYPE_CONVERTERS_H_
