// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_VR_VR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr/gvr_util.h"
#include "chrome/browser/vr/gesture_detector.h"
#include "chrome/browser/vr/platform_controller.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace gfx {
class Transform;
}

namespace gvr {
class ControllerState;
class GvrApi;
}

namespace vr {

// Angle (radians) the beam down from the controller axis, for wrist comfort.
constexpr float kErgoAngleOffset = 0.26f;

class VrController : public PlatformController {
 public:
  // Controller API entry point.
  explicit VrController(gvr::GvrApi* gvr_api);
  ~VrController() override;

  // Must be called when the Activity gets OnResume().
  void OnResume();

  // Must be called when the Activity gets OnPause().
  void OnPause();

  device::Gamepad GetGamepadData();

  // Called once per frame to update controller state.
  void UpdateState(const gfx::Transform& head_pose);

  InputEventList DetectGestures();

  gfx::Quaternion Orientation() const;
  gfx::Point3F Position() const;
  void GetTransform(gfx::Transform* out) const;
  void GetRelativePointerTransform(gfx::Transform* out) const;
  void GetPointerTransform(gfx::Transform* out) const;
  float GetOpacity() const;
  gfx::Point3F GetPointerStart() const;

  bool TouchDownHappened();

  bool TouchUpHappened();

  bool ButtonState(gvr::ControllerButton button) const;

  bool IsConnected();
  void EnableDeadzoneForTesting();

  // PlatformController
  bool IsButtonDown(PlatformController::ButtonType type) const override;
  bool ButtonUpHappened(PlatformController::ButtonType type) const override;
  bool ButtonDownHappened(PlatformController::ButtonType type) const override;
  bool IsTouchingTrackpad() const override;
  gfx::PointF GetPositionInTrackpad() const override;
  base::TimeTicks GetLastOrientationTimestamp() const override;
  base::TimeTicks GetLastTouchTimestamp() const override;
  base::TimeTicks GetLastButtonTimestamp() const override;
  ControllerModel::Handedness GetHandedness() const override;
  bool GetRecentered() const override;
  int GetBatteryLevel() const override;

 private:
  bool GetButtonLongPressFromButtonInfo();

  void UpdateTimestamps();

  void UpdateOverallVelocity();

  void UpdateAlpha();

  std::unique_ptr<gvr::ControllerApi> controller_api_;

  // The last controller state (updated once per frame).
  std::unique_ptr<gvr::ControllerState> controller_state_;

  gvr::GvrApi* gvr_api_;

  std::unique_ptr<GestureDetector> gesture_detector_;

  float last_qx_;
  bool pinch_started_;
  // TODO(https://crbug.com/824194): Remove this and associated logic once the
  // GVR-side bug is fixed and we don't need to keep track of click states
  // ourselves.
  bool previous_button_states_[GVR_CONTROLLER_BUTTON_COUNT];

  // Handedness from user prefs.
  gvr::ControllerHandedness handedness_;

  // Head offset. Keeps the controller at the user's side with 6DoF headsets.
  gfx::Point3F head_offset_;

  base::TimeTicks last_orientation_timestamp_;
  base::TimeTicks last_touch_timestamp_;
  base::TimeTicks last_button_timestamp_;

  int64_t last_timestamp_nanos_ = 0;

  float alpha_value_ = 1.0f;

  DISALLOW_COPY_AND_ASSIGN(VrController);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_CONTROLLER_H_
