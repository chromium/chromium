// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/input_device_settings.h"

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/system/fake_input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "content/public/browser/browser_thread.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {
namespace system {
namespace {

InputDeviceSettings* g_input_device_settings_impl_ozone_instance = nullptr;

// InputDeviceSettings for Ozone.
class InputDeviceSettingsImplOzone : public InputDeviceSettings {
 public:
  InputDeviceSettingsImplOzone();

  InputDeviceSettingsImplOzone(const InputDeviceSettingsImplOzone&) = delete;
  InputDeviceSettingsImplOzone& operator=(const InputDeviceSettingsImplOzone&) =
      delete;

 protected:
  ~InputDeviceSettingsImplOzone() override {}

 private:
  // Overridden from InputDeviceSettings.
  void TouchpadExists(DeviceExistsCallback callback) override;
  void UpdateTouchpadSettings(const TouchpadSettings& settings) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTouchpadScrollSensitivity(int value) override;
  void HapticTouchpadExists(DeviceExistsCallback callback) override;
  void SetTouchpadHapticFeedback(bool enabled) override;
  void SetTouchpadHapticClickSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void MouseExists(DeviceExistsCallback callback) override;
  void UpdateMouseSettings(const MouseSettings& settings) override;
  void SetMouseSensitivity(int value) override;
  void SetMouseScrollSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void SetMouseReverseScroll(bool enabled) override;
  void SetMouseAcceleration(bool enabled) override;
  void SetMouseScrollAcceleration(bool enabled) override;
  void PointingStickExists(DeviceExistsCallback callback) override;
  void UpdatePointingStickSettings(
      const PointingStickSettings& settings) override;
  void SetPointingStickSensitivity(int value) override;
  void SetPointingStickPrimaryButtonRight(bool right) override;
  void SetPointingStickAcceleration(bool enabled) override;
  void SetTouchpadAcceleration(bool enabled) override;
  void SetTouchpadScrollAcceleration(bool enabled) override;
  void ReapplyTouchpadSettings() override;
  void ReapplyMouseSettings() override;
  void ReapplyPointingStickSettings() override;
  InputDeviceSettings::FakeInterface* GetFakeInterface() override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  void SetTouchscreensEnabled(bool enabled) override;

  ui::InputController* input_controller() {
    return ui::OzonePlatform::GetInstance()->GetInputController();
  }

  // Respective device setting objects.
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;
  PointingStickSettings current_pointing_stick_settings_;
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
  DCHECK_GE(value, static_cast<int>(PointerSensitivity::kLowest));
  DCHECK_LE(value, static_cast<int>(PointerSensitivity::kHighest));
  current_touchpad_settings_.SetSensitivity(value);
  input_controller()->SetTouchpadSensitivity(std::nullopt, value);
}

void InputDeviceSettingsImplOzone::SetTouchpadScrollSensitivity(int value) {
  DCHECK_GE(value, static_cast<int>(PointerSensitivity::kLowest));
  DCHECK_LE(value, static_cast<int>(PointerSensitivity::kHighest));
  current_touchpad_settings_.SetScrollSensitivity(value);
  input_controller()->SetTouchpadScrollSensitivity(std::nullopt, value);
}

void InputDeviceSettingsImplOzone::HapticTouchpadExists(
    DeviceExistsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(input_controller()->HasHapticTouchpad());
}

void InputDeviceSettingsImplOzone::SetTouchpadHapticFeedback(bool enabled) {
  current_touchpad_settings_.SetHapticFeedback(enabled);
  input_controller()->SetTouchpadHapticFeedback(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::SetTouchpadHapticClickSensitivity(
    int value) {
  current_touchpad_settings_.SetHapticClickSensitivity(value);
  input_controller()->SetTouchpadHapticClickSensitivity(std::nullopt, value);
}

void InputDeviceSettingsImplOzone::SetNaturalScroll(bool enabled) {
  current_touchpad_settings_.SetNaturalScroll(enabled);
  input_controller()->SetNaturalScroll(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::SetTapToClick(bool enabled) {
  current_touchpad_settings_.SetTapToClick(enabled);
  input_controller()->SetTapToClick(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::SetThreeFingerClick(bool enabled) {
  // For Alex/ZGB.
  current_touchpad_settings_.SetThreeFingerClick(enabled);
  input_controller()->SetThreeFingerClick(enabled);
}

void InputDeviceSettingsImplOzone::SetTapDragging(bool enabled) {
  current_touchpad_settings_.SetTapDragging(enabled);
  input_controller()->SetTapDragging(std::nullopt, enabled);
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
  DCHECK_GE(value, static_cast<int>(PointerSensitivity::kLowest));
  DCHECK_LE(value, static_cast<int>(PointerSensitivity::kHighest));
  current_mouse_settings_.SetSensitivity(value);
  input_controller()->SetMouseSensitivity(std::nullopt, value);
}

void InputDeviceSettingsImplOzone::SetMouseScrollSensitivity(int value) {
  DCHECK_GE(value, static_cast<int>(PointerSensitivity::kLowest));
  DCHECK_LE(value, static_cast<int>(PointerSensitivity::kHighest));
  current_mouse_settings_.SetScrollSensitivity(value);
  input_controller()->SetMouseScrollSensitivity(std::nullopt, value);
}

void InputDeviceSettingsImplOzone::SetPrimaryButtonRight(bool right) {
  current_mouse_settings_.SetPrimaryButtonRight(right);
  input_controller()->SetPrimaryButtonRight(std::nullopt, right);
}

void InputDeviceSettingsImplOzone::SetMouseReverseScroll(bool enabled) {
  current_mouse_settings_.SetReverseScroll(enabled);
  input_controller()->SetMouseReverseScroll(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::SetMouseAcceleration(bool enabled) {
  current_mouse_settings_.SetAcceleration(enabled);
  input_controller()->SetMouseAcceleration(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::SetMouseScrollAcceleration(bool enabled) {
  current_mouse_settings_.SetScrollAcceleration(enabled);
  input_controller()->SetMouseScrollAcceleration(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::PointingStickExists(
    DeviceExistsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(input_controller()->HasPointingStick());
}

void InputDeviceSettingsImplOzone::UpdatePointingStickSettings(
    const PointingStickSettings& update) {
  if (current_pointing_stick_settings_.Update(update))
    ReapplyPointingStickSettings();
}

void InputDeviceSettingsImplOzone::SetPointingStickSensitivity(int value) {
  DCHECK_GE(value, static_cast<int>(PointerSensitivity::kLowest));
  DCHECK_LE(value, static_cast<int>(PointerSensitivity::kHighest));
  current_pointing_stick_settings_.SetSensitivity(value);
  input_controller()->SetPointingStickSensitivity(std::nullopt, value);
}

void InputDeviceSettingsImplOzone::SetPointingStickPrimaryButtonRight(
    bool right) {
  current_pointing_stick_settings_.SetPrimaryButtonRight(right);
  input_controller()->SetPointingStickPrimaryButtonRight(std::nullopt, right);
}

void InputDeviceSettingsImplOzone::SetPointingStickAcceleration(bool enabled) {
  current_pointing_stick_settings_.SetAcceleration(enabled);
  input_controller()->SetPointingStickAcceleration(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::ReapplyPointingStickSettings() {
  PointingStickSettings::Apply(current_pointing_stick_settings_, this);
}

void InputDeviceSettingsImplOzone::SetTouchpadAcceleration(bool enabled) {
  current_touchpad_settings_.SetAcceleration(enabled);
  input_controller()->SetTouchpadAcceleration(std::nullopt, enabled);
}

void InputDeviceSettingsImplOzone::SetTouchpadScrollAcceleration(bool enabled) {
  current_touchpad_settings_.SetScrollAcceleration(enabled);
  input_controller()->SetTouchpadScrollAcceleration(std::nullopt, enabled);
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
    if (base::SysInfo::IsRunningOnChromeOS()) {
      g_input_device_settings_impl_ozone_instance =
          new InputDeviceSettingsImplOzone();
    } else {
      g_input_device_settings_impl_ozone_instance =
          new FakeInputDeviceSettings();
    }
  }
  return g_input_device_settings_impl_ozone_instance;
}

}  // namespace system
}  // namespace ash
