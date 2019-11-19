// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/cardboard_input_delegate.h"

#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "device/vr/android/gvr/gvr_delegate.h"

namespace vr {

CardboardInputDelegate::CardboardInputDelegate(gvr::GvrApi* gvr_api)
    : gvr_api_(gvr_api) {}

CardboardInputDelegate::~CardboardInputDelegate() = default;

void CardboardInputDelegate::OnTriggerEvent(bool pressed) {
  if (pressed) {
    cardboard_trigger_pressed_ = true;
  } else if (cardboard_trigger_pressed_) {
    cardboard_trigger_pressed_ = false;
    cardboard_trigger_clicked_ = true;
  }
}

gfx::Transform CardboardInputDelegate::GetHeadPose() {
  gfx::Transform head_pose;
  device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_, &head_pose);
  return head_pose;
}

void CardboardInputDelegate::UpdateController(const gfx::Transform& head_pose,
                                              base::TimeTicks current_time,
                                              bool is_webxr_frame) {}

ControllerModel CardboardInputDelegate::GetControllerModel(
    const gfx::Transform& head_pose) {
  // On cardboard, we have some UI frames, such as when no WebXR frame have
  // arrived. But in this case, we don't have any elements that react to input.
  return {};
}

InputEventList CardboardInputDelegate::GetGestures(
    base::TimeTicks current_time) {
  return {};
}

device::mojom::XRInputSourceStatePtr
CardboardInputDelegate::GetInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();
  // Only one gaze input source to worry about, so it can have a static id.
  state->source_id = 1;

  // Report any trigger state changes made since the last call and reset the
  // state here.
  state->primary_input_pressed = cardboard_trigger_pressed_;
  state->primary_input_clicked = cardboard_trigger_clicked_;
  cardboard_trigger_clicked_ = false;

  state->description = device::mojom::XRInputSourceDescription::New();

  // It's a gaze-cursor-based device.
  state->description->target_ray_mode = device::mojom::XRTargetRayMode::GAZING;
  state->emulated_position = true;

  // No implicit handedness
  state->description->handedness = device::mojom::XRHandedness::NONE;

  // Pointer and grip transforms are omitted since this is a gaze-based source.

  return state;
}

void CardboardInputDelegate::OnResume() {}

void CardboardInputDelegate::OnPause() {}

}  // namespace vr
