// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/fake_input_device_settings.h"

#include <utility>

#include "base/callback.h"

namespace chromeos {
namespace system {

FakeInputDeviceSettings::FakeInputDeviceSettings() {}

FakeInputDeviceSettings::~FakeInputDeviceSettings() {}

// Overriden from InputDeviceSettings.
void FakeInputDeviceSettings::TouchpadExists(DeviceExistsCallback callback) {
  std::move(callback).Run(touchpad_exists_);
}

void FakeInputDeviceSettings::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  current_touchpad_settings_.Update(settings);
}

void FakeInputDeviceSettings::SetTouchpadSensitivity(int value) {
  TouchpadSettings settings;
  settings.SetSensitivity(value);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetTapToClick(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapToClick(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetThreeFingerClick(bool enabled) {
  TouchpadSettings settings;
  settings.SetThreeFingerClick(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetTapDragging(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapDragging(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetNaturalScroll(bool enabled) {
  TouchpadSettings settings;
  settings.SetNaturalScroll(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::MouseExists(DeviceExistsCallback callback) {
  std::move(callback).Run(mouse_exists_);
}

void FakeInputDeviceSettings::UpdateMouseSettings(
    const MouseSettings& settings) {
  current_mouse_settings_.Update(settings);
}

void FakeInputDeviceSettings::SetMouseSensitivity(int value) {
  MouseSettings settings;
  settings.SetSensitivity(value);
  UpdateMouseSettings(settings);
}

void FakeInputDeviceSettings::SetPrimaryButtonRight(bool right) {
  MouseSettings settings;
  settings.SetPrimaryButtonRight(right);
  UpdateMouseSettings(settings);
}

void FakeInputDeviceSettings::SetMouseReverseScroll(bool enabled) {
  MouseSettings settings;
  settings.SetReverseScroll(enabled);
  UpdateMouseSettings(settings);
}

void FakeInputDeviceSettings::SetMouseAcceleration(bool enabled) {
  MouseSettings settings;
  settings.SetAcceleration(enabled);
  UpdateMouseSettings(settings);
}

void FakeInputDeviceSettings::SetTouchpadAcceleration(bool enabled) {
  TouchpadSettings settings;
  settings.SetAcceleration(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::ReapplyTouchpadSettings() {
}

void FakeInputDeviceSettings::ReapplyMouseSettings() {
}

InputDeviceSettings::FakeInterface*
FakeInputDeviceSettings::GetFakeInterface() {
  return this;
}

void FakeInputDeviceSettings::set_touchpad_exists(bool exists) {
  touchpad_exists_ = exists;
}

void FakeInputDeviceSettings::set_mouse_exists(bool exists) {
  mouse_exists_ = exists;
}

const TouchpadSettings& FakeInputDeviceSettings::current_touchpad_settings()
    const {
  return current_touchpad_settings_;
}

const MouseSettings& FakeInputDeviceSettings::current_mouse_settings() const {
  return current_mouse_settings_;
}

}  // namespace system
}  // namespace chromeos
