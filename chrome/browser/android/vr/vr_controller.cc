// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_controller.h"

#include <algorithm>
#include <utility>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/input_event.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_controller.h"

namespace vr {

namespace {

constexpr float kNanoSecondsPerSecond = 1.0e9f;

// Distance from the center of the controller to start rendering the laser.
constexpr float kLaserStartDisplacement = 0.045;

constexpr float kFadeDistanceFromFace = 0.34f;
constexpr float kDeltaAlpha = 3.0f;

void ClampTouchpadPosition(gfx::PointF* position) {
  position->set_x(base::clamp(position->x(), 0.0f, 1.0f));
  position->set_y(base::clamp(position->y(), 0.0f, 1.0f));
}

float DeltaTimeSeconds(int64_t last_timestamp_nanos) {
  return (gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos -
          last_timestamp_nanos) /
         kNanoSecondsPerSecond;
}

gvr::ControllerButton PlatformToGvrButton(PlatformController::ButtonType type) {
  switch (type) {
    case PlatformController::kButtonHome:
      return gvr::kControllerButtonHome;
    case PlatformController::kButtonMenu:
      return gvr::kControllerButtonApp;
    case PlatformController::kButtonSelect:
      return gvr::kControllerButtonClick;
    default:
      return gvr::kControllerButtonNone;
  }
}

}  // namespace

VrController::VrController(gvr::GvrApi* gvr_api)
    : gvr_api_(gvr_api), previous_button_states_{0} {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  controller_api_ = std::make_unique<gvr::ControllerApi>();
  controller_state_ = std::make_unique<gvr::ControllerState>();

  int32_t options = gvr::ControllerApi::DefaultOptions();

  options |= GVR_CONTROLLER_ENABLE_ARM_MODEL;

  CHECK(controller_api_->Init(options, gvr_api_->cobj()));
  controller_api_->Resume();

  handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();

  gesture_detector_ = std::make_unique<GestureDetector>();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

VrController::~VrController() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

void VrController::OnResume() {
  if (controller_api_) {
    controller_api_->Resume();
    handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();
  }
}

void VrController::OnPause() {
  if (controller_api_)
    controller_api_->Pause();
}

device::Gamepad VrController::GetGamepadData() {
  device::Gamepad gamepad;

  gamepad.connected = IsConnected();
  gamepad.timestamp = controller_state_->GetLastOrientationTimestamp();

  if (!gamepad.connected)
    return gamepad;

  if (handedness_ == GVR_CONTROLLER_RIGHT_HANDED) {
    gamepad.hand = device::GamepadHand::kRight;
  } else {
    gamepad.hand = device::GamepadHand::kLeft;
  }

  bool touched = controller_state_->IsTouching();
  bool pressed = controller_state_->GetButtonState(GVR_CONTROLLER_BUTTON_CLICK);
  double value = pressed ? 1.0 : 0.0;

  gamepad.buttons[gamepad.buttons_length++] =
      device::GamepadButton(pressed, touched, value);

  if (touched) {
    // Trackpad values reported by the GVR Android SDK are clamped to
    // [0.0, 1.0], with (0, 0) corresponding to the top-left of the touchpad.
    // Normalize the values to use X axis range -1 (left) to 1 (right) and Y
    // axis range -1 (top) to 1 (bottom).
    double x = GetPositionInTrackpad().x();
    double y = GetPositionInTrackpad().y();
    gamepad.axes[0] = (x * 2.0) - 1.0;
    gamepad.axes[1] = (y * 2.0) - 1.0;
  } else {
    gamepad.axes[0] = 0.0;
    gamepad.axes[1] = 0.0;
  }

  gamepad.axes_length = 2;

  return gamepad;
}

bool VrController::IsButtonDown(ButtonType type) const {
  return controller_state_->GetButtonState(PlatformToGvrButton(type));
}

bool VrController::IsTouchingTrackpad() const {
  return controller_state_->IsTouching();
}

gfx::PointF VrController::GetPositionInTrackpad() const {
  gfx::PointF position{controller_state_->GetTouchPos().x,
                       controller_state_->GetTouchPos().y};
  ClampTouchpadPosition(&position);
  return position;
}

base::TimeTicks VrController::GetLastOrientationTimestamp() const {
  // TODO(crbug/866040): Use controller_state_->GetLastTouchTimestamp() when
  // GVR is upgraded.
  return last_orientation_timestamp_;
}

base::TimeTicks VrController::GetLastTouchTimestamp() const {
  // TODO(crbug/866040): Use controller_state_->GetLastTouchTimestamp() when
  // GVR is upgraded.
  return last_touch_timestamp_;
}

base::TimeTicks VrController::GetLastButtonTimestamp() const {
  // TODO(crbug/866040): Use controller_state_->GetLastButtonTimestamp() when
  // GVR is upgraded.
  return last_button_timestamp_;
}

ControllerModel::Handedness VrController::GetHandedness() const {
  return handedness_ == GVR_CONTROLLER_RIGHT_HANDED
             ? ControllerModel::kRightHanded
             : ControllerModel::kLeftHanded;
}

bool VrController::GetRecentered() const {
  return controller_state_->GetRecentered();
}

int VrController::GetBatteryLevel() const {
  return static_cast<int>(controller_state_->GetBatteryLevel());
}

gfx::Quaternion VrController::Orientation() const {
  const gvr::Quatf& orientation = controller_state_->GetOrientation();
  return gfx::Quaternion(orientation.qx, orientation.qy, orientation.qz,
                         orientation.qw);
}

gfx::Point3F VrController::Position() const {
  const gvr::Vec3f& position = controller_state_->GetPosition();
  return gfx::Point3F(position.x + head_offset_.x(),
                      position.y + head_offset_.y(),
                      position.z + head_offset_.z());
}

void VrController::GetTransform(gfx::Transform* out) const {
  *out = gfx::Transform(Orientation());
  out->PostTranslate3d(Position().OffsetFromOrigin());
}

void VrController::GetRelativePointerTransform(gfx::Transform* out) const {
  *out = gfx::Transform();
  out->RotateAboutXAxis(-kErgoAngleOffset * 180.0f / base::kPiFloat);
  out->Translate3d(0, 0, -kLaserStartDisplacement);
}

void VrController::GetPointerTransform(gfx::Transform* out) const {
  gfx::Transform controller;
  GetTransform(&controller);

  GetRelativePointerTransform(out);
  out->PostConcat(controller);
}

float VrController::GetOpacity() const {
  return alpha_value_;
}

gfx::Point3F VrController::GetPointerStart() const {
  gfx::Transform pointer_transform;
  GetPointerTransform(&pointer_transform);

  gfx::Point3F pointer_position = pointer_transform.MapPoint(gfx::Point3F());
  return pointer_position;
}

bool VrController::TouchDownHappened() {
  return controller_state_->GetTouchDown();
}

bool VrController::TouchUpHappened() {
  return controller_state_->GetTouchUp();
}

bool VrController::ButtonDownHappened(ButtonType button) const {
  // Workaround for GVR sometimes not reporting GetButtonDown when it should.
  auto gvr_button = PlatformToGvrButton(button);
  bool detected_down = !previous_button_states_[static_cast<int>(gvr_button)] &&
                       ButtonState(gvr_button);
  return controller_state_->GetButtonDown(gvr_button) || detected_down;
}

bool VrController::ButtonUpHappened(ButtonType button) const {
  // Workaround for GVR sometimes not reporting GetButtonUp when it should.
  auto gvr_button = PlatformToGvrButton(button);
  bool detected_up = previous_button_states_[static_cast<int>(gvr_button)] &&
                     !ButtonState(gvr_button);
  return controller_state_->GetButtonUp(gvr_button) || detected_up;
}

bool VrController::ButtonState(gvr::ControllerButton button) const {
  return controller_state_->GetButtonState(button);
}

bool VrController::IsConnected() {
  return controller_state_->GetConnectionState() == gvr::kControllerConnected;
}

void VrController::UpdateState(const gfx::Transform& head_pose) {
  gfx::Transform inv_pose;
  if (head_pose.GetInverse(&inv_pose))
    head_offset_ = inv_pose.MapPoint(gfx::Point3F());

  gvr::Mat4f gvr_head_pose;
  TransformToGvrMat(head_pose, &gvr_head_pose);
  controller_api_->ApplyArmModel(handedness_, gvr::kArmModelBehaviorFollowGaze,
                                 gvr_head_pose);
  const int32_t old_status = controller_state_->GetApiStatus();
  const int32_t old_connection_state = controller_state_->GetConnectionState();
  // Due to DON flow skipping weirdness, it's possible for the controller to be
  // briefly disconnected. We don't want to miss a button up/down transition
  // during that time, so only update previous button states if the controller
  // is actually connected.
  if (IsConnected()) {
    for (int button = 0; button < GVR_CONTROLLER_BUTTON_COUNT; ++button) {
      previous_button_states_[button] =
          ButtonState(static_cast<gvr_controller_button>(button));
    }
  }
  // Read current controller state.
  controller_state_->Update(*controller_api_);
  // Print new API status and connection state, if they changed.
  if (controller_state_->GetApiStatus() != old_status ||
      controller_state_->GetConnectionState() != old_connection_state) {
    VLOG(1) << "Controller Connection status: "
            << gvr_controller_connection_state_to_string(
                   controller_state_->GetConnectionState());
  }
  UpdateAlpha();
  UpdateTimestamps();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

InputEventList VrController::DetectGestures() {
  if (controller_state_->GetConnectionState() != gvr::kControllerConnected) {
    return {};
  }

  return gesture_detector_->DetectGestures(*this, base::TimeTicks::Now());
}

void VrController::UpdateTimestamps() {
  // controller_state_->GetLast*Timestamp() returns timestamps in a
  // different timebase from base::TimeTicks::Now(), so we can't use the
  // timestamps in any meaningful way in the rest of Chrome.
  // TODO(crbug/866040): Use controller_state_->GetLast*Timestamp() after
  // we upgrade GVR.
  base::TimeTicks now = base::TimeTicks::Now();
  last_orientation_timestamp_ = now;
  if (IsTouchingTrackpad())
    last_touch_timestamp_ = now;
  for (int button = PlatformController::kButtonTypeFirst;
       button < PlatformController::kButtonTypeNumber; ++button) {
    if (IsButtonDown(static_cast<PlatformController::ButtonType>(button))) {
      last_button_timestamp_ = now;
      break;
    }
  }
}

void VrController::UpdateAlpha() {
  float distance_to_face = (Position() - gfx::Point3F()).Length();
  float alpha_change = kDeltaAlpha * DeltaTimeSeconds(last_timestamp_nanos_);
  alpha_value_ = base::clamp(distance_to_face < kFadeDistanceFromFace
                                 ? alpha_value_ - alpha_change
                                 : alpha_value_ + alpha_change,
                             0.0f, 1.0f);
}

}  // namespace vr
