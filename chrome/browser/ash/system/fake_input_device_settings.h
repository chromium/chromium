// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_ASH_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_

#include "chrome/browser/ash/system/input_device_settings.h"

namespace ash {
namespace system {

// This fake just memorizes current values of input devices settings.
class FakeInputDeviceSettings : public InputDeviceSettings,
                                public InputDeviceSettings::FakeInterface {
 public:
  FakeInputDeviceSettings();

  FakeInputDeviceSettings(const FakeInputDeviceSettings&) = delete;
  FakeInputDeviceSettings& operator=(const FakeInputDeviceSettings&) = delete;

  ~FakeInputDeviceSettings() override;

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
  void SetNaturalScroll(bool enabled) override;
  void ReapplyTouchpadSettings() override;
  void ReapplyMouseSettings() override;
  void ReapplyPointingStickSettings() override;
  InputDeviceSettings::FakeInterface* GetFakeInterface() override;

  // Overridden from InputDeviceSettings::FakeInterface.
  void set_touchpad_exists(bool exists) override;
  void set_haptic_touchpad_exists(bool exists) override;
  void set_mouse_exists(bool exists) override;
  void set_pointing_stick_exists(bool exists) override;
  const TouchpadSettings& current_touchpad_settings() const override;
  const MouseSettings& current_mouse_settings() const override;
  const PointingStickSettings& current_pointing_stick_settings() const override;

 private:
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;
  PointingStickSettings current_pointing_stick_settings_;

  bool touchpad_exists_ = true;
  bool haptic_touchpad_exists_ = true;
  bool mouse_exists_ = true;
  bool pointing_stick_exists_ = true;
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_
