// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_input_delegate.h"

#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/android/gvr/gvr_delegate.h"

namespace {
constexpr gfx::Vector3dF kForwardVector = {0.0f, 0.0f, -1.0f};

device::Gamepad CreateGamepad(const vr::GvrGamepadData& data) {
  device::Gamepad gamepad;

  // Unless the controller state is updated on a different thread,
  // data.connected should always be true when this function is called by
  // GvrInputDelegate::GetInputSourceState.
  gamepad.connected = data.connected;

  gamepad.timestamp = data.timestamp;

  gamepad.hand = data.right_handed ? device::GamepadHand::kRight
                                   : device::GamepadHand::kLeft;

  bool pressed = data.controller_button_pressed;
  bool touched = data.is_touching;
  double value = pressed ? 1.0 : 0.0;
  gamepad.buttons[gamepad.buttons_length++] =
      device::GamepadButton(pressed, touched, value);

  if (touched) {
    // data.touch_pos values reported by the GVR Android SDK are clamped to
    // [0.0, 1.0], with (0, 0) corresponding to the top-left of the touchpad.
    // Normalize the values to use X axis range -1 (left) to 1 (right) and Y
    // axis range -1 (top) to 1 (bottom).
    gamepad.axes[0] = (data.touch_pos.x() * 2.0) - 1.0;
    gamepad.axes[1] = (data.touch_pos.y() * 2.0) - 1.0;
  } else {
    gamepad.axes[0] = 0.0;
    gamepad.axes[1] = 0.0;
  }

  gamepad.axes_length = 2;

  return gamepad;
}
}  // namespace

namespace vr {

GvrInputDelegate::GvrInputDelegate(gvr::GvrApi* gvr_api,
                                   GlBrowserInterface* browser)
    : controller_(std::make_unique<VrController>(gvr_api)),
      gvr_api_(gvr_api),
      browser_(browser) {}

GvrInputDelegate::~GvrInputDelegate() = default;

gfx::Transform GvrInputDelegate::GetHeadPose() {
  gfx::Transform head_pose;
  device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_, &head_pose);
  return head_pose;
}

void GvrInputDelegate::OnTriggerEvent(bool pressed) {
  NOTREACHED();
}

void GvrInputDelegate::UpdateController(const gfx::Transform& head_pose,
                                        base::TimeTicks current_time,
                                        bool is_webxr_frame) {
  controller_->UpdateState(head_pose);

  GvrGamepadData controller_data = controller_->GetGamepadData();
  if (!is_webxr_frame)
    controller_data.connected = false;
  browser_->UpdateGamepadData(controller_data);
}

ControllerModel GvrInputDelegate::GetControllerModel(
    const gfx::Transform& head_pose) {
  gfx::Vector3dF head_direction = GetForwardVector(head_pose);

  gfx::Vector3dF controller_direction;
  gfx::Quaternion controller_quat;
  if (!controller_->IsConnected()) {
    // No controller detected, set up a gaze cursor that tracks the forward
    // direction.
    controller_direction = kForwardVector;
    controller_quat = gfx::Quaternion(kForwardVector, head_direction);
  } else {
    controller_direction = {0.0f, -sin(kErgoAngleOffset),
                            -cos(kErgoAngleOffset)};
    controller_quat = controller_->Orientation();
  }
  gfx::Transform(controller_quat).TransformVector(&controller_direction);

  ControllerModel controller_model;
  controller_->GetTransform(&controller_model.transform);
  controller_model.touchpad_button_state = ControllerModel::ButtonState::kUp;
  DCHECK(!(controller_->ButtonUpHappened(PlatformController::kButtonSelect) &&
           controller_->ButtonDownHappened(PlatformController::kButtonSelect)))
      << "Cannot handle a button down and up event within one frame.";
  if (controller_->ButtonState(gvr::kControllerButtonClick)) {
    controller_model.touchpad_button_state =
        ControllerModel::ButtonState::kDown;
  }
  controller_model.app_button_state =
      controller_->ButtonState(gvr::kControllerButtonApp)
          ? ControllerModel::ButtonState::kDown
          : ControllerModel::ButtonState::kUp;
  controller_model.home_button_state =
      controller_->ButtonState(gvr::kControllerButtonHome)
          ? ControllerModel::ButtonState::kDown
          : ControllerModel::ButtonState::kUp;
  controller_model.opacity = controller_->GetOpacity();
  controller_model.laser_direction = controller_direction;
  controller_model.laser_origin = controller_->GetPointerStart();
  controller_model.handedness = controller_->GetHandedness();
  controller_model.recentered = controller_->GetRecentered();
  controller_model.touching_touchpad = controller_->IsTouchingTrackpad();
  controller_model.touchpad_touch_position =
      controller_->GetPositionInTrackpad();
  controller_model.last_orientation_timestamp =
      controller_->GetLastOrientationTimestamp();
  controller_model.last_button_timestamp =
      controller_->GetLastButtonTimestamp();
  controller_model.battery_level = controller_->GetBatteryLevel();
  return controller_model;
}

InputEventList GvrInputDelegate::GetGestures(base::TimeTicks current_time) {
  if (!controller_->IsConnected())
    return {};
  return gesture_detector_.DetectGestures(*controller_, current_time);
}

device::mojom::XRInputSourceStatePtr GvrInputDelegate::GetInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();
  state->description = device::mojom::XRInputSourceDescription::New();

  // Only one controller is supported, so the source id can be static.
  state->source_id = 1;

  // It's a handheld pointing device.
  state->description->target_ray_mode =
      device::mojom::XRTargetRayMode::POINTING;

  // Controller uses an arm model.
  state->emulated_position = true;

  if (controller_->IsConnected()) {
    // Set the primary button state.
    bool select_button_down =
        controller_->IsButtonDown(PlatformController::kButtonSelect);
    state->primary_input_pressed = select_button_down;
    state->primary_input_clicked =
        was_select_button_down_ && !select_button_down;
    was_select_button_down_ = select_button_down;

    // Set handedness.
    state->description->handedness =
        controller_->GetHandedness() ==
                ControllerModel::Handedness::kRightHanded
            ? device::mojom::XRHandedness::RIGHT
            : device::mojom::XRHandedness::LEFT;

    // Get the grip transform
    gfx::Transform grip;
    controller_->GetTransform(&grip);
    state->mojo_from_input = grip;

    // Set the pointer offset from the grip transform.
    gfx::Transform pointer;
    controller_->GetRelativePointerTransform(&pointer);
    state->description->input_from_pointer = pointer;

    state->description->profiles.push_back("google-daydream");

    // This Gamepad data is used to expose touchpad position to WebXR.
    state->gamepad = CreateGamepad(controller_->GetGamepadData());
  }

  return state;
}

void GvrInputDelegate::OnResume() {
  controller_->OnResume();
}

void GvrInputDelegate::OnPause() {
  controller_->OnPause();
}

}  // namespace vr
