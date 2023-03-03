// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_dispatcher.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

InputDeviceSettingsDispatcher::InputDeviceSettingsDispatcher(
    ui::InputController* input_controller)
    : input_controller_(input_controller) {
  DCHECK(input_controller_);
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->input_device_settings_controller()->AddObserver(this);
  }
}

InputDeviceSettingsDispatcher::~InputDeviceSettingsDispatcher() {
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->input_device_settings_controller()->RemoveObserver(this);
  }
}

void InputDeviceSettingsDispatcher::OnMouseConnected(
    const mojom::Mouse& mouse) {
  DispatchMouseSettings(mouse);
}

void InputDeviceSettingsDispatcher::OnMouseSettingsUpdated(
    const mojom::Mouse& mouse) {
  DispatchMouseSettings(mouse);
}

void InputDeviceSettingsDispatcher::OnTouchpadConnected(
    const mojom::Touchpad& touchpad) {
  DispatchTouchpadSettings(touchpad);
}

void InputDeviceSettingsDispatcher::OnTouchpadSettingsUpdated(
    const mojom::Touchpad& touchpad) {
  DispatchTouchpadSettings(touchpad);
}

void InputDeviceSettingsDispatcher::OnPointingStickConnected(
    const mojom::PointingStick& pointing_stick) {
  DispatchPointingStickSettings(pointing_stick);
}

void InputDeviceSettingsDispatcher::OnPointingStickSettingsUpdated(
    const mojom::PointingStick& pointing_stick) {
  DispatchPointingStickSettings(pointing_stick);
}

void InputDeviceSettingsDispatcher::DispatchMouseSettings(
    const mojom::Mouse& mouse) {
  DCHECK(mouse.settings);
  const auto& settings = *mouse.settings;
  input_controller_->SetMouseAcceleration(mouse.id,
                                          settings.acceleration_enabled);
  input_controller_->SetMouseSensitivity(mouse.id, settings.sensitivity);
  input_controller_->SetMouseReverseScroll(mouse.id,
                                           settings.reverse_scrolling);
  input_controller_->SetMouseScrollAcceleration(mouse.id,
                                                settings.scroll_acceleration);
  input_controller_->SetMouseScrollSensitivity(mouse.id,
                                               settings.scroll_sensitivity);
  input_controller_->SetPrimaryButtonRight(mouse.id, settings.swap_right);
}

void InputDeviceSettingsDispatcher::DispatchTouchpadSettings(
    const mojom::Touchpad& touchpad) {
  DCHECK(touchpad.settings);
  const auto& settings = *touchpad.settings;
  input_controller_->SetTouchpadSensitivity(touchpad.id, settings.sensitivity);
  input_controller_->SetTouchpadScrollSensitivity(touchpad.id,
                                                  settings.scroll_sensitivity);
  input_controller_->SetTapToClick(touchpad.id, settings.tap_to_click_enabled);
  input_controller_->SetTapDragging(touchpad.id, settings.tap_dragging_enabled);
  input_controller_->SetNaturalScroll(touchpad.id, settings.reverse_scrolling);
  input_controller_->SetTouchpadAcceleration(touchpad.id,
                                             settings.acceleration_enabled);
  input_controller_->SetTouchpadScrollAcceleration(
      touchpad.id, settings.scroll_acceleration);
  input_controller_->SetTouchpadHapticClickSensitivity(
      touchpad.id, settings.haptic_sensitivity);
  input_controller_->SetTouchpadHapticFeedback(touchpad.id,
                                               settings.haptic_enabled);
}

void InputDeviceSettingsDispatcher::DispatchPointingStickSettings(
    const mojom::PointingStick& pointing_stick) {
  DCHECK(pointing_stick.settings);
  const auto& settings = *pointing_stick.settings;
  input_controller_->SetPointingStickAcceleration(
      pointing_stick.id, settings.acceleration_enabled);
  input_controller_->SetPointingStickSensitivity(pointing_stick.id,
                                                 settings.sensitivity);
  input_controller_->SetPointingStickPrimaryButtonRight(pointing_stick.id,
                                                        settings.swap_right);
}

}  // namespace ash
