// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/fake_input_device_settings.h"

#include <utility>

#include "base/functional/callback.h"

namespace ash {
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

void FakeInputDeviceSettings::SetTouchpadScrollSensitivity(int value) {
  TouchpadSettings settings;
  settings.SetScrollSensitivity(value);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::HapticTouchpadExists(
    DeviceExistsCallback callback) {
  std::move(callback).Run(haptic_touchpad_exists_);
}

void FakeInputDeviceSettings::SetTouchpadHapticFeedback(bool enabled) {
  TouchpadSettings settings;
  settings.SetHapticFeedback(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetTouchpadHapticClickSensitivity(int value) {
  TouchpadSettings settings;
  settings.SetHapticClickSensitivity(value);
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

void FakeInputDeviceSettings::SetMouseScrollSensitivity(int value) {
  MouseSettings settings;
  settings.SetScrollSensitivity(value);
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

void FakeInputDeviceSettings::SetMouseScrollAcceleration(bool enabled) {
  MouseSettings settings;
  settings.SetScrollAcceleration(enabled);
  UpdateMouseSettings(settings);
}

void FakeInputDeviceSettings::PointingStickExists(
    DeviceExistsCallback callback) {
  std::move(callback).Run(pointing_stick_exists_);
}

void FakeInputDeviceSettings::UpdatePointingStickSettings(
    const PointingStickSettings& settings) {
  current_pointing_stick_settings_.Update(settings);
}

void FakeInputDeviceSettings::SetPointingStickSensitivity(int value) {
  PointingStickSettings settings;
  settings.SetSensitivity(value);
  UpdatePointingStickSettings(settings);
}

void FakeInputDeviceSettings::SetPointingStickPrimaryButtonRight(bool right) {
  PointingStickSettings settings;
  settings.SetPrimaryButtonRight(right);
  UpdatePointingStickSettings(settings);
}

void FakeInputDeviceSettings::SetPointingStickAcceleration(bool enabled) {
  PointingStickSettings settings;
  settings.SetAcceleration(enabled);
  UpdatePointingStickSettings(settings);
}

void FakeInputDeviceSettings::SetTouchpadAcceleration(bool enabled) {
  TouchpadSettings settings;
  settings.SetAcceleration(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetTouchpadScrollAcceleration(bool enabled) {
  TouchpadSettings settings;
  settings.SetScrollAcceleration(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::ReapplyTouchpadSettings() {
}

void FakeInputDeviceSettings::ReapplyMouseSettings() {
}

void FakeInputDeviceSettings::ReapplyPointingStickSettings() {}

InputDeviceSettings::FakeInterface*
FakeInputDeviceSettings::GetFakeInterface() {
  return this;
}

void FakeInputDeviceSettings::set_touchpad_exists(bool exists) {
  touchpad_exists_ = exists;
}

void FakeInputDeviceSettings::set_haptic_touchpad_exists(bool exists) {
  haptic_touchpad_exists_ = exists;
}

void FakeInputDeviceSettings::set_mouse_exists(bool exists) {
  mouse_exists_ = exists;
}

void FakeInputDeviceSettings::set_pointing_stick_exists(bool exists) {
  pointing_stick_exists_ = exists;
}

const TouchpadSettings& FakeInputDeviceSettings::current_touchpad_settings()
    const {
  return current_touchpad_settings_;
}

const MouseSettings& FakeInputDeviceSettings::current_mouse_settings() const {
  return current_mouse_settings_;
}

const PointingStickSettings&
FakeInputDeviceSettings::current_pointing_stick_settings() const {
  return current_pointing_stick_settings_;
}

}  // namespace system
}  // namespace ash
