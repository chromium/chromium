// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_GAMEPAD_DATA_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_GAMEPAD_DATA_H_

#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

// Subset of GVR controller data needed for the gamepad API. Filled in
// by vr_shell's VrController.
struct GvrGamepadData {
  GvrGamepadData()
      : timestamp(0),
        is_touching(false),
        controller_button_pressed(false),
        right_handed(true),
        connected(false) {}
  int64_t timestamp;
  gfx::Vector2dF touch_pos;
  gfx::Quaternion orientation;
  gfx::Vector3dF accel;
  gfx::Vector3dF gyro;
  bool is_touching;
  bool controller_button_pressed;
  bool right_handed;
  bool connected;
};

}  // namespace vr
#endif  // CHROME_BROWSER_ANDROID_VR_GVR_GAMEPAD_DATA_H_
