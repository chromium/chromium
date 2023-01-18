// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_input_delegate.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/android/gvr/gvr_delegate.h"

namespace vr {

GvrInputDelegate::GvrInputDelegate(gvr::GvrApi* gvr_api)
    : controller_(std::make_unique<VrController>(gvr_api)), gvr_api_(gvr_api) {}

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
    state->gamepad = controller_->GetGamepadData();
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
