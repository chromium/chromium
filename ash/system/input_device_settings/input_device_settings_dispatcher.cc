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

namespace {

bool ShouldModifiersBeBlockedForKeyEventsWithRestriction(
    mojom::CustomizationRestriction customization_restriction) {
  switch (customization_restriction) {
    case mojom::CustomizationRestriction::kAllowCustomizations:
    case mojom::CustomizationRestriction::kDisallowCustomizations:
    case mojom::CustomizationRestriction::kDisableKeyEventRewrites:
    case mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites:
    case mojom::CustomizationRestriction::
        kAllowAlphabetOrNumberKeyEventRewrites:
    case mojom::CustomizationRestriction::kAllowHorizontalScrollWheelRewrites:
    case mojom::CustomizationRestriction::kAllowFKeyRewrites:
      return false;
    case mojom::CustomizationRestriction::kAllowTabEventRewrites:
      return true;
  }
}

}  // namespace

InputDeviceSettingsDispatcher::InputDeviceSettingsDispatcher(
    ui::InputController* input_controller)
    : input_controller_(input_controller) {
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    input_device_settings_controller_ =
        Shell::Get()->input_device_settings_controller();
    input_device_settings_controller_->AddObserver(this);
  }

  if (features::IsPeripheralCustomizationEnabled()) {
    duplicate_id_finder_ =
        &input_device_settings_controller_->duplicate_id_finder();
    duplicate_id_finder_->AddObserver(this);
  }
}

InputDeviceSettingsDispatcher::~InputDeviceSettingsDispatcher() {
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    input_device_settings_controller_->RemoveObserver(this);
  }

  if (features::IsPeripheralCustomizationEnabled()) {
    duplicate_id_finder_->RemoveObserver(this);
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

  if (!features::IsPeripheralCustomizationEnabled()) {
    return;
  }

  UpdateDevicesToBlockModifiers();
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

void InputDeviceSettingsDispatcher::UpdateDevicesToBlockModifiers() {
  devices_with_blocked_modifiers_.clear();

  auto mice = input_device_settings_controller_->GetConnectedMice();
  for (const auto& mouse : mice) {
    if (!ShouldModifiersBeBlockedForKeyEventsWithRestriction(
            mouse->customization_restriction)) {
      continue;
    }

    bool block_current_mouse = false;
    for (const auto& button_remapping : mouse->settings->button_remappings) {
      if (button_remapping->button->is_customizable_button()) {
        continue;
      }

      // If the action is supposed to be "default", do nothing
      if (button_remapping->remapping_action.is_null()) {
        continue;
      }

      block_current_mouse = true;
      break;
    }

    if (!block_current_mouse) {
      continue;
    }

    auto vid_pid = duplicate_id_finder_->GetVendorProductIdForDevice(mouse->id);
    if (!vid_pid) {
      continue;
    }

    devices_with_blocked_modifiers_.insert(*vid_pid);
  }

  DispatchDevicesToBlockModifiers();
}

void InputDeviceSettingsDispatcher::OnDuplicateDevicesUpdated() {
  DispatchDevicesToBlockModifiers();
}

void InputDeviceSettingsDispatcher::DispatchDevicesToBlockModifiers() {
  std::vector<int> device_ids;
  for (const auto& vid_pid : devices_with_blocked_modifiers_) {
    auto* duplicate_ids = duplicate_id_finder_->GetDuplicateDeviceIds(vid_pid);
    if (!duplicate_ids) {
      continue;
    }

    device_ids.reserve(device_ids.size() + duplicate_ids->size());
    device_ids.insert(device_ids.end(), duplicate_ids->begin(),
                      duplicate_ids->end());
  }

  // Insert devices from `devices_with_blocked_modifiers_from_observing_` but
  // check to make sure we are not duplicating anything.
  for (const auto& vid_pid : devices_with_blocked_modifiers_from_observing_) {
    if (devices_with_blocked_modifiers_.contains(vid_pid)) {
      continue;
    }

    auto* duplicate_ids = duplicate_id_finder_->GetDuplicateDeviceIds(vid_pid);
    if (!duplicate_ids) {
      continue;
    }

    device_ids.reserve(device_ids.size() + duplicate_ids->size());
    device_ids.insert(device_ids.end(), duplicate_ids->begin(),
                      duplicate_ids->end());
  }

  input_controller_->BlockModifiersOnDevices(std::move(device_ids));
}

void InputDeviceSettingsDispatcher::OnCustomizableMouseObservingStarted(
    const mojom::Mouse& mouse) {
  if (!ShouldModifiersBeBlockedForKeyEventsWithRestriction(
          mouse.customization_restriction)) {
    return;
  }

  auto vid_pid = duplicate_id_finder_->GetVendorProductIdForDevice(mouse.id);
  if (!vid_pid) {
    return;
  }

  devices_with_blocked_modifiers_from_observing_.insert(*vid_pid);
  DispatchDevicesToBlockModifiers();
}

void InputDeviceSettingsDispatcher::OnCustomizableMouseObservingStopped() {
  if (devices_with_blocked_modifiers_from_observing_.empty()) {
    return;
  }

  devices_with_blocked_modifiers_from_observing_.clear();
  DispatchDevicesToBlockModifiers();
}

}  // namespace ash
