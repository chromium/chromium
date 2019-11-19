// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/type_converters.h"

#include "ui/gfx/transform_util.h"

namespace mojo {

device::mojom::XRPlaneOrientation
TypeConverter<device::mojom::XRPlaneOrientation, ArPlaneType>::Convert(
    ArPlaneType plane_type) {
  switch (plane_type) {
    case ArPlaneType::AR_PLANE_HORIZONTAL_DOWNWARD_FACING:
    case ArPlaneType::AR_PLANE_HORIZONTAL_UPWARD_FACING:
      return device::mojom::XRPlaneOrientation::HORIZONTAL;
    case ArPlaneType::AR_PLANE_VERTICAL:
      return device::mojom::XRPlaneOrientation::VERTICAL;
  }
}

gfx::Transform TypeConverter<gfx::Transform, device::mojom::VRPosePtr>::Convert(
    const device::mojom::VRPosePtr& pose) {
  gfx::DecomposedTransform decomposed;
  if (pose->orientation) {
    decomposed.quaternion = *pose->orientation;
  }

  if (pose->position) {
    decomposed.translate[0] = pose->position->x();
    decomposed.translate[1] = pose->position->y();
    decomposed.translate[2] = pose->position->z();
  }

  return gfx::ComposeTransform(decomposed);
}

gfx::Transform TypeConverter<gfx::Transform, device::mojom::PosePtr>::Convert(
    const device::mojom::PosePtr& pose) {
  gfx::DecomposedTransform decomposed;
  decomposed.quaternion = pose->orientation;

  decomposed.translate[0] = pose->position.x();
  decomposed.translate[1] = pose->position.y();
  decomposed.translate[2] = pose->position.z();

  return gfx::ComposeTransform(decomposed);
}

}  // namespace mojo
