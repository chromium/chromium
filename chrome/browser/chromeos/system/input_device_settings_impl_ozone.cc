// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/system/fake_input_device_settings.h"
#include "chromeos/system/devicemode.h"
#include "content/public/browser/browser_thread.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromeos {
namespace system {
namespace {

InputDeviceSettings* g_input_device_settings_impl_ozone_instance = nullptr;

// InputDeviceSettings for Ozone.
class InputDeviceSettingsImplOzone : public InputDeviceSettings {
 public:
  InputDeviceSettingsImplOzone();

 protected:
  ~InputDeviceSettingsImplOzone() override {}

 private:
  // Overridden from InputDeviceSettings.
  void TouchpadExists(DeviceExistsCallback callback) override;
  void UpdateTouchpadSettings(const TouchpadSettings& settings) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void MouseExists(DeviceExistsCallback callback) override;
  void UpdateMouseSettings(const MouseSettings& settings) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void SetMouseReverseScroll(bool enabled) override;
  void SetMouseAcceleration(bool enabled) override;
  void SetTouchpadAcceleration(bool enabled) override;
  void ReapplyTouchpadSettings() override;
  void ReapplyMouseSettings() override;
  InputDeviceSettings::FakeInterface* GetFakeInterface() override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  void SetTouchscreensEnabled(bool enabled) override;

  ui::InputController* input_controller() {
    return ui::OzonePlatform::GetInstance()->GetInputController();
  }

  // Respective device setting objects.
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceSettingsImplOzone);
};

InputDeviceSettingsImplOzone::InputDeviceSettingsImplOzone() = default;

void InputDeviceSettingsImplOzone::TouchpadExists(
    DeviceExistsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(input_controller()->HasTouchpad());
}

void InputDeviceSettingsImplOzone::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  if (current_touchpad_settings_.Update(settings))
    ReapplyTouchpadSettings();
}

void InputDeviceSettingsImplOzone::SetTouchpadSensitivity(int value) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  current_touchpad_settings_.SetSensitivity(value);
  input_controller()->SetTouchpadSensitivity(value);
}

void InputDeviceSettingsImplOzone::SetNaturalScroll(bool enabled) {
  current_touchpad_settings_.SetNaturalScroll(enabled);
  input_controller()->SetNaturalScroll(enabled);
}

void InputDeviceSettingsImplOzone::SetTapToClick(bool enabled) {
  current_touchpad_settings_.SetTapToClick(enabled);
  input_controller()->SetTapToClick(enabled);
}

void InputDeviceSettingsImplOzone::SetThreeFingerClick(bool enabled) {
  // For Alex/ZGB.
  current_touchpad_settings_.SetThreeFingerClick(enabled);
  input_controller()->SetThreeFingerClick(enabled);
}

void InputDeviceSettingsImplOzone::SetTapDragging(bool enabled) {
  current_touchpad_settings_.SetTapDragging(enabled);
  input_controller()->SetTapDragging(enabled);
}

void InputDeviceSettingsImplOzone::MouseExists(DeviceExistsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(input_controller()->HasMouse());
}

void InputDeviceSettingsImplOzone::UpdateMouseSettings(
    const MouseSettings& update) {
  if (current_mouse_settings_.Update(update))
    ReapplyMouseSettings();
}

void InputDeviceSettingsImplOzone::SetMouseSensitivity(int value) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  current_mouse_settings_.SetSensitivity(value);
  input_controller()->SetMouseSensitivity(value);
}

void InputDeviceSettingsImplOzone::SetPrimaryButtonRight(bool right) {
  current_mouse_settings_.SetPrimaryButtonRight(right);
  input_controller()->SetPrimaryButtonRight(right);
}

void InputDeviceSettingsImplOzone::SetMouseReverseScroll(bool enabled) {
  current_mouse_settings_.SetReverseScroll(enabled);
  input_controller()->SetMouseReverseScroll(enabled);
}

void InputDeviceSettingsImplOzone::SetMouseAcceleration(bool enabled) {
  current_mouse_settings_.SetAcceleration(enabled);
  input_controller()->SetMouseAcceleration(enabled);
}

void InputDeviceSettingsImplOzone::SetTouchpadAcceleration(bool enabled) {
  current_touchpad_settings_.SetAcceleration(enabled);
  input_controller()->SetTouchpadAcceleration(enabled);
}

void InputDeviceSettingsImplOzone::ReapplyTouchpadSettings() {
  TouchpadSettings::Apply(current_touchpad_settings_, this);
}

void InputDeviceSettingsImplOzone::ReapplyMouseSettings() {
  MouseSettings::Apply(current_mouse_settings_, this);
}

InputDeviceSettings::FakeInterface*
InputDeviceSettingsImplOzone::GetFakeInterface() {
  return nullptr;
}

void InputDeviceSettingsImplOzone::SetInternalTouchpadEnabled(bool enabled) {
  input_controller()->SetInternalTouchpadEnabled(enabled);
}

void InputDeviceSettingsImplOzone::SetTouchscreensEnabled(bool enabled) {
  input_controller()->SetTouchscreensEnabled(enabled);
}

}  // namespace

// static
InputDeviceSettings* InputDeviceSettings::Get() {
  if (!g_input_device_settings_impl_ozone_instance) {
    if (IsRunningAsSystemCompositor())
      g_input_device_settings_impl_ozone_instance =
          new InputDeviceSettingsImplOzone;
    else
      g_input_device_settings_impl_ozone_instance =
          new FakeInputDeviceSettings();
  }
  return g_input_device_settings_impl_ozone_instance;
}

}  // namespace system
}  // namespace chromeos
